#include <AsyncMqttClient.h>
#include "abstract_pubsub.h"

#if ASYNC_TCP_SSL_ENABLED
#define MQTT_SECURE true
#endif

//
//@********************** class PubsubEspAsyncMQTTLeaf ***********************
//
// This class encapsulates an Mqtt connection using esp32 wifi
//

class PubsubEspAsyncMQTTLeaf : public AbstractPubsubLeaf
{
public:
  PubsubEspAsyncMQTTLeaf(String name, String target="", bool use_ssl=false, bool use_device_topic=true, bool run=true) : AbstractPubsubLeaf(name, target, use_ssl, use_device_topic) {
    LEAF_ENTER(L_INFO);
    do_heartbeat = false;
    this->run = run;
    this->impersonate_backplane = true;
    LEAF_LEAVE;
  }

  virtual void setup();
  virtual void loop(void);
  virtual uint16_t _mqtt_publish(String topic, String payload, int qos=0, bool retain=false);
  virtual void _mqtt_subscribe(String topic, int qos=0);
  virtual void _mqtt_unsubscribe(String topic);
  virtual bool mqtt_receive(String type, String name, String topic, String payload);


private:
  //
  // Network resources
  //
  AsyncMqttClient mqttClient;
  Ticker mqttReconnectTimer;
  uint16_t sleep_pub_id = 0;
  int sleep_duration_ms = 0;
  char lwt_topic[80];

  void _mqtt_connect();
  void _mqtt_connect_callback(bool sessionPresent);
  void _mqtt_disconnect_callback(AsyncMqttClientDisconnectReason reason);
  void _mqtt_subscribe_callback(uint16_t packetId, uint8_t qos);
  void _mqtt_unsubscribe_callback(uint16_t packetId);
  void _mqtt_receive_callback(char* topic,
			      char* payload,
			      AsyncMqttClientMessageProperties properties,
			      size_t len,
			      size_t index,
			      size_t total);
  void _mqtt_publish_callback(uint16_t packetId);
  void handle_connect_event();
  virtual void initiate_sleep_ms(int ms);

};

void PubsubEspAsyncMQTTLeaf::setup()
{
  AbstractPubsubLeaf::setup();
  LEAF_ENTER(L_INFO);

  if (prefsLeaf) {
    String value = prefsLeaf->get("mqtt_host");
    if (value.length()) {
      // there's a preference, overwrite the default
      strlcpy(mqtt_host, value.c_str(), sizeof(mqtt_host));
    }
    else {
      // nothing saved, save the default
      prefsLeaf->put("mqtt_host", mqtt_host);
    }

    value = prefsLeaf->get("mqtt_port");
    if (value.length()) {
      strlcpy(mqtt_port, value.c_str(), sizeof(mqtt_port));
    }
    else {
      // nothing saved, save the default
      prefsLeaf->put("mqtt_port", mqtt_port);
    }

    value = prefsLeaf->get("mqtt_user");
    if (value.length()) {
      strlcpy(mqtt_user, value.c_str(), sizeof(mqtt_user));
    }
    else {
      prefsLeaf->put("mqtt_user", mqtt_user);
    }


    value = prefsLeaf->get("mqtt_pass");
    if (value.length()) {
      strlcpy(mqtt_pass, value.c_str(), sizeof(mqtt_pass));
    }
    else {
      prefsLeaf->put("mqtt_pass", mqtt_pass);
    }
  }

  //
  // Set up the MQTT Client
  //
  uint16_t portno = atoi(mqtt_port);

  LEAF_NOTICE("MQTT Setup [%s:%hu] %s", mqtt_host, portno, device_id);
  mqttClient.setServer(mqtt_host, portno);
  mqttClient.setClientId(device_id);
  mqttClient.setCleanSession(pubsub_use_clean_session);
  if (mqtt_user && strlen(mqtt_user)>0) {
    LEAF_NOTICE("Using MQTT username %s", mqtt_user);
    mqttClient.setCredentials(mqtt_user, mqtt_pass);
  }

  mqttClient.onConnect(
    [](bool sessionPresent) {
      NOTICE("mqtt onConnect");
      PubsubEspAsyncMQTTLeaf *that = (PubsubEspAsyncMQTTLeaf *)Leaf::get_leaf_by_type(leaves, String("pubsub"));
      if (!that) {
	ALERT("I don't know who I am!");
	return;
      }
      that->_mqtt_connect_callback(sessionPresent);
    });

  mqttClient.onDisconnect(
    [](AsyncMqttClientDisconnectReason reason) {
      PubsubEspAsyncMQTTLeaf *that = (PubsubEspAsyncMQTTLeaf *)Leaf::get_leaf_by_type(leaves, String("pubsub"));
      if (!that) {
	ALERT("I don't know who I am!");
	return;
      }
      that->_mqtt_disconnect_callback(reason);
    });

  mqttClient.onSubscribe(
    [](uint16_t packetId, uint8_t qos)
    {
	   PubsubEspAsyncMQTTLeaf *that = (PubsubEspAsyncMQTTLeaf *)Leaf::get_leaf_by_type(leaves, String("pubsub"));
	   if (!that) {
	     ALERT("I don't know who I am!");
	     return;
	   }
	   that->_mqtt_subscribe_callback(packetId, qos);
    });

  mqttClient.onUnsubscribe(
    [](uint16_t packetId){
	   PubsubEspAsyncMQTTLeaf *that = (PubsubEspAsyncMQTTLeaf *)Leaf::get_leaf_by_type(leaves, String("pubsub"));
	   if (!that) {
	     ALERT("I don't know who I am!");
	     return;
	   }
	   that->_mqtt_unsubscribe_callback(packetId);
    });

  mqttClient.onPublish(
    [](uint16_t packetId){
	   PubsubEspAsyncMQTTLeaf *that = (PubsubEspAsyncMQTTLeaf *)Leaf::get_leaf_by_type(leaves, String("pubsub"));
	   if (!that) {
	     ALERT("I don't know who I am!");
	     return;
	   }
	   that->_mqtt_publish_callback(packetId);
    });

   mqttClient.onMessage(
     [](char* topic,
			    char* payload,
			    AsyncMqttClientMessageProperties properties,
			    size_t len,
			    size_t index,
	size_t total){
	    PubsubEspAsyncMQTTLeaf *that = (PubsubEspAsyncMQTTLeaf *)Leaf::get_leaf_by_type(leaves, String("pubsub"));
	    if (!that) {
	      ALERT("I don't know who I am!");
	      return;
	    }
	    that->_mqtt_receive_callback(topic,payload,properties,len,index,total);
     });

   snprintf(lwt_topic, sizeof(lwt_topic), "%sstatus/presence", base_topic.c_str());
   LEAF_INFO("LWT topic is %s", lwt_topic);
   mqttClient.setWill(lwt_topic, 0, true, "offline");

#if ASYNC_TCP_SSL_ENABLED
   mqttClient.setSecure(use_ssl);
   if (use_ssl) {
     //mqttClient.addServerFingerprint((const uint8_t[])MQTT_SERVER_FINGERPRINT);
   }
#endif

   LEAF_LEAVE;
}

bool PubsubEspAsyncMQTTLeaf::mqtt_receive(String type, String name, String topic, String payload)
{
  //LEAF_ENTER(L_DEBUG);
  bool handled = Leaf::mqtt_receive(type, name, topic, payload);
  //LEAF_INFO("%s, %s", topic.c_str(), payload.c_str());

  WHEN("_ip_connect", {
    _mqtt_connect();
    handled = true; 
   })
  else if (topic.startsWith("cmd/join/")) {
    handled=true;
      String ssid = topic.substring(9);
      LEAF_NOTICE("Joining wifi [%s] [%s]", ssid.c_str(), payload.c_str());
      WiFi.mode(WIFI_STA);
      WiFi.begin(ssid.c_str(), payload.c_str());
      for (int retries=0;
	   (WiFi.status() != WL_CONNECTED) && (retries < 20);
	   retries++) {
        delay(1000);
        Serial.print(".");
      }
      Serial.println();
      if (WiFi.status() != WL_CONNECTED) {
	LEAF_ALERT("    WiFi failed");
      }
      else {
	LEAF_NOTICE("    WiFi connected");
      }
  }
  
  return handled;
}

void PubsubEspAsyncMQTTLeaf::loop()
{
  AbstractPubsubLeaf::loop();
  //ENTER(L_DEBUG);

  static unsigned long lastHeartbeat = 0;
  unsigned long now = millis();

#if 0
  static bool was_connected = false;

  if (!was_connected && pubsub_connected) {
    LEAF_NOTICE("triggering connection event");
    handle_connect_event();
  }
  else if (was_connected && !pubsub_connected) {
    LEAF_NOTICE("MQTT lost connection");
    mqttConnected = false;
  }
  was_connected = pubsub_connected;
#endif

  //
  // Handle MQTT Events
  //
  if (pubsub_connected && do_heartbeat) {
    //
    // MQTT is active, process any pending events
    //
    if (now > (lastHeartbeat + heartbeat_interval_seconds*1000)) {
      lastHeartbeat = now;
      _mqtt_publish(String(base_topic+"status/heartbeat"), String(now/1000, DEC));
    }
  }
  //LEAVE;
}

//
// Initiate connection to MQTT server
//
void PubsubEspAsyncMQTTLeaf::_mqtt_connect() {
  LEAF_ENTER(L_INFO);

  if (wifiConnected && mqttConfigured) {
    LEAF_NOTICE("Connecting to MQTT at %s...",mqtt_host);
    mqttClient.connect();
    LEAF_INFO("MQTT Connection initiated");
  }
  else {
    //LEAF_DEBUG("MQTT not configured yet.  Retry in %d sec.", PUBSUB_RECONNECT_SECONDS);
    mqttReconnectTimer.once(
      PUBSUB_RECONNECT_SECONDS,
      []() {
	PubsubEspAsyncMQTTLeaf *that = (PubsubEspAsyncMQTTLeaf *)Leaf::get_leaf_by_type(leaves, "pubsub");
	if (that) that->_mqtt_connect();
      }
      );
  }


  LEAF_LEAVE;
}

void PubsubEspAsyncMQTTLeaf::_mqtt_connect_callback(bool sessionPresent) {
  // set a flag that will cause loop to invoke handle_connect_event.
  // we do this rigmarole to avoid race conditions in trace output
  LEAF_ENTER(L_INFO);
  pubsub_connected = true;
  this->pubsub_session_present = sessionPresent;
  LEAF_LEAVE;
}

void PubsubEspAsyncMQTTLeaf::handle_connect_event()
{
  LEAF_ENTER(L_INFO);

  LEAF_NOTICE("Connected to MQTT.  pubsub_session_present=%s", TRUTH(pubsub_session_present));

  // Once connected, publish an announcement...
  mqttConnected = true;
  mqtt_publish("status/presence", "online", 0, true);
  if (wake_reason.length()) {
    mqtt_publish("status/wake", wake_reason, 0, true);
  }
  mqtt_publish("status/ip", ip_addr_str, 0, true);
  for (int i=0; leaves[i]; i++) {
    Leaf *leaf = leaves[i];
    LEAF_INFO("call connect hook for leaf %d %s", i, leaf->get_name().c_str());
    leaf->mqtt_do_subscribe();
  }

  // Subscribe to common topics rather than individuals (some servers don't
  // like wide wildcards tho!)

  if (use_wildcard_topic) {
    INFO("Using wildcard topic subscriptions under %s", base_topic.c_str());
    if (use_cmd) {
      _mqtt_subscribe(base_topic+"cmd/#");
      if (pubsub_use_device_topic) {
	_mqtt_subscribe(base_topic+"+/+/cmd/#");
      }
    }
    if (use_get) {
      _mqtt_subscribe(base_topic+"get/#");
      if (pubsub_use_device_topic) {
	_mqtt_subscribe(base_topic+"+/+/get/#");
      }
    }
    if (use_set) {
      _mqtt_subscribe(base_topic+"set/#");
      if (pubsub_use_device_topic) {
	_mqtt_subscribe(base_topic+"+/+/set/#");
      }
    }
  }

  // ... and resubscribe
  mqtt_subscribe("cmd/restart");
  mqtt_subscribe("cmd/setup");
  mqtt_subscribe("cmd/join");
#ifdef _OTA_OPS_H
  mqtt_subscribe("cmd/update");
  mqtt_subscribe("cmd/rollback");
  mqtt_subscribe("cmd/bootpartition");
  mqtt_subscribe("cmd/nextpartition");
#endif
  mqtt_subscribe("cmd/ping");
  mqtt_subscribe("cmd/leaves");
  mqtt_subscribe("cmd/format");
  mqtt_subscribe("cmd/status");
  mqtt_subscribe("cmd/subscriptions");
  mqtt_subscribe("set/name");
  mqtt_subscribe("set/debug");
  mqtt_subscribe("set/debug_wait");
  mqtt_subscribe("set/debug_lines");
  mqtt_subscribe("set/debug_flush");

  LEAF_INFO("Set up leaf subscriptions");
  for (int i=0; leaves[i]; i++) {
    Leaf *leaf = leaves[i];
    LEAF_INFO("Initiate subscriptions for %s", leaf->get_name().c_str());
    leaf->mqtt_do_subscribe();
  }

  LEAF_NOTICE("MQTT Connection setup complete");

  idle_pattern(2000,10);

  LEAF_LEAVE;
}

void PubsubEspAsyncMQTTLeaf::_mqtt_disconnect_callback(AsyncMqttClientDisconnectReason reason) {
  LEAF_ENTER(L_INFO);
  const char *reasons[] = {
    "TCP_DISCONNECTED", // 0
    "MQTT_UNACCEPTABLE_PROTOCOL_VERSION",
    "MQTT_IDENTIFIER_REJECTED",
    "MQTT_SERVER_UNAVAILABLE",
    "MQTT_MALFORMED_CREDENTIALS",
    "MQTT_NOT_AUTHORIZED",
    "ESP8266_NOT_ENOUGH_SPACE",
    "TLS_BAD_FINGERPRINT"
  };
  int reasonInt = (int)reason;

  LEAF_ALERT("Disconnected from MQTT (%d %s)", reasonInt, ((reasonInt>=0) && (reasonInt<=7))?reasons[reasonInt]:"unknown");

  pubsub_connected = false;
  if (WiFi.isConnected()) {
    mqttReconnectTimer.once(
      PUBSUB_RECONNECT_SECONDS,
      []()
      {
	PubsubEspAsyncMQTTLeaf *that = (PubsubEspAsyncMQTTLeaf *)Leaf::get_leaf_by_type(leaves, "pubsub");
	if (that) that->_mqtt_connect();
      }
      );
  }

  for (int i=0; leaves[i]; i++) {
    leaves[i]->mqtt_disconnect();
  }
  LEAF_LEAVE;
}

uint16_t PubsubEspAsyncMQTTLeaf::_mqtt_publish(String topic, String payload, int qos, bool retain)
{
  uint16_t packetId = 0;
  //ENTER(L_DEBUG);
  const char *topic_c_str = topic.c_str();
  const char *payload_c_str = payload.c_str();
  LEAF_INFO("PUB %s => [%s]", topic_c_str, payload_c_str);

  if (isStarted() && mqttConnected) {
    packetId = mqttClient.publish(topic.c_str(), qos, retain, payload_c_str);
    //DEBUG("Publish initiated, ID=%d", packetId);
  }
  else {
    //LEAF_DEBUG("Publish skipped while MQTT connection is down: %s=>%s", topic_c_str, payload_c_str);
  }
  yield();
  //LEAVE;
  return packetId;
}

void PubsubEspAsyncMQTTLeaf::_mqtt_publish_callback(uint16_t packetId) {
  //DEBUG("Publish acknowledged %d", (int)packetId);
  if (packetId == sleep_pub_id) {
    LEAF_NOTICE("Going to sleep for %d ms", sleep_duration_ms);
#ifdef ESP8266
    ESP.deepSleep(1000*sleep_duration_ms, WAKE_RF_DEFAULT);
#else
    esp_sleep_enable_timer_wakeup(sleep_duration_ms * 1000);
    Serial.flush();
    esp_deep_sleep_start();
#endif
  }
}

void PubsubEspAsyncMQTTLeaf::_mqtt_subscribe(String topic, int qos)
{
  LEAF_ENTER(L_INFO);
  LEAF_NOTICE("SUB %s", topic.c_str());
  if (mqttConnected) {
    uint16_t packetIdSub = mqttClient.subscribe(topic.c_str(), qos);
    if (packetIdSub == 0) {
      LEAF_INFO("Subscription FAILED for topic=%s", topic.c_str());
    }
    else {
      //LEAF_DEBUG("Subscription initiated id=%d topic=%s", (int)packetIdSub, topic.c_str());
    }

    if (pubsub_subscriptions) {
      //LEAF_DEBUG("Record subscription");
      pubsub_subscriptions->put(topic, 0);
    }
  }
  else {
    LEAF_ALERT("Warning: Subscription attempted while MQTT connection is down (%s)", topic.c_str());
  }
  LEAF_LEAVE;
}

void PubsubEspAsyncMQTTLeaf::_mqtt_unsubscribe(String topic)
{
  LEAF_ENTER(L_DEBUG);
  //LEAF_NOTICE("MQTT UNSUB %s", topic.c_str());
  if (mqttConnected) {
    uint16_t packetIdSub = mqttClient.unsubscribe(topic.c_str());
    LEAF_DEBUG("UNSUBSCRIPTION initiated id=%d topic=%s", (int)packetIdSub, topic.c_str());
    if (pubsub_subscriptions) {
      pubsub_subscriptions->remove(topic);
    }
  }
  else {
    LEAF_ALERT("Warning: Unsubscription attempted while MQTT connection is down (%s)", topic.c_str());
  }
  LEAF_LEAVE;
}

void PubsubEspAsyncMQTTLeaf::_mqtt_subscribe_callback(uint16_t packetId, uint8_t qos) {
  //DEBUG("Subscribe acknowledged %d QOS %d", (int)packetId, (int)qos);
}

void PubsubEspAsyncMQTTLeaf::_mqtt_unsubscribe_callback(uint16_t packetId) {
  //LEAF_DEBUG("Unsubscribe acknowledged %d", (int)packetId);
}

void PubsubEspAsyncMQTTLeaf::_mqtt_receive_callback(char* topic,
			    char* payload,
			    AsyncMqttClientMessageProperties properties,
			    size_t len,
			    size_t index,
			    size_t total) {
  LEAF_ENTER(L_DEBUG);

  // handle message arrived
  char payload_buf[256];
  if (len > sizeof(payload_buf)-1) len=sizeof(payload_buf)-1;
  memcpy(payload_buf, payload, len);
  payload_buf[len]='\0';
  (void)index;
  (void)total;

  LEAF_NOTICE("MQTT message from server %s <= [%s] (q%d%s)",
	 topic, payload_buf, (int)properties.qos, properties.retain?" retain":"");

  this->_mqtt_receive(String(topic), String(payload_buf));
  LEAF_LEAVE;
}

void PubsubEspAsyncMQTTLeaf::initiate_sleep_ms(int ms)
{
  LEAF_ENTER(L_NOTICE);
  sleep_duration_ms = ms;
  sleep_pub_id = _mqtt_publish(base_topic+"status/presence", String("sleep"), 1, true);
  LEAF_LEAVE;
}

// Local Variables:
// mode: C++
// c-basic-offset: 2
// End:
