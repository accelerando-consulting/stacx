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

struct PubsubReceiveMessage 
{
  String *topic;
  String *payload;
  AsyncMqttClientMessageProperties properties;
};

enum PubsubEventCode {
  PUBSUB_EVENT_CONNECT,
  PUBSUB_EVENT_DISCONNECT,
  PUBSUB_EVENT_SUBSCRIBE_DONE,
  PUBSUB_EVENT_UNSUBSCRIBE_DONE,
  PUBSUB_EVENT_PUBLISH_DONE
};

struct PubsubEventMessage 
{
  enum PubsubEventCode code;
  int context;
};

const char *pubsub_esp_disconnect_reasons[] = {
  "TCP_DISCONNECTED", // 0
  "MQTT_UNACCEPTABLE_PROTOCOL_VERSION",
  "MQTT_IDENTIFIER_REJECTED",
  "MQTT_SERVER_UNAVAILABLE",
  "MQTT_MALFORMED_CREDENTIALS",
  "MQTT_NOT_AUTHORIZED",
  "ESP8266_NOT_ENOUGH_SPACE",
  "TLS_BAD_FINGERPRINT"
};


class PubsubEspAsyncMQTTLeaf : public AbstractPubsubLeaf
{
public:
  PubsubEspAsyncMQTTLeaf(String name, String target="", bool use_ssl=false, bool use_device_topic=true, bool run=true) : AbstractPubsubLeaf(name, target, use_ssl, use_device_topic) {
    LEAF_ENTER(L_INFO);
    do_heartbeat = false;
    this->run = run;
    this->impersonate_backplane = true;
    this->pubsub_keepalive_sec = 60;
    LEAF_LEAVE;
  }

  virtual void setup();
  virtual void loop(void);
  virtual uint16_t _mqtt_publish(String topic, String payload, int qos=0, bool retain=false);
  virtual void _mqtt_subscribe(String topic, int qos=0);
  virtual void _mqtt_unsubscribe(String topic);
  virtual bool mqtt_receive(String type, String name, String topic, String payload);


  virtual bool pubsubConnect(void) ;
  virtual void pubsubDisconnect(bool deliberate=true) ;
  virtual void pubsubOnConnect(bool do_subscribe=true);
  void eventQueueSend(struct PubsubEventMessage *msg) 
  {
    xQueueGenericSend(event_queue, (void *)msg, (TickType_t)0, queueSEND_TO_BACK);
  }
  void receiveQueueSend(struct PubsubReceiveMessage *msg) 
  {
    xQueueGenericSend(receive_queue, (void *)msg, (TickType_t)0, queueSEND_TO_BACK);
  }


private:
  //
  // Network resources
  //
  AsyncMqttClient mqttClient;
  Ticker mqttReconnectTimer;
  uint16_t sleep_pub_id = 0;
  int sleep_duration_ms = 0;
  char lwt_topic[80];
  QueueHandle_t receive_queue;
  QueueHandle_t event_queue;

  void _mqtt_receive_callback(char* topic,
			      char* payload,
			      AsyncMqttClientMessageProperties properties,
			      size_t len,
			      size_t index,
			      size_t total);
  void _mqtt_publish_callback(uint16_t packetId);

  virtual void initiate_sleep_ms(int ms);

};

void PubsubEspAsyncMQTTLeaf::setup()
{
  AbstractPubsubLeaf::setup();
  LEAF_ENTER(L_NOTICE);

  //
  // Set up the MQTT Client
  //
  LEAF_NOTICE("MQTT Setup [%s:%d] %s", pubsub_host.c_str(), pubsub_port, device_id);
  receive_queue = xQueueCreate(10, sizeof(struct PubsubReceiveMessage));
  event_queue = xQueueCreate(10, sizeof(struct PubsubEventMessage));
  mqttClient.setServer(pubsub_host.c_str(), pubsub_port);
  mqttClient.setClientId(device_id);
  mqttClient.setCleanSession(pubsub_use_clean_session);
  if (pubsub_keepalive_sec) {
    mqttClient.setKeepAlive(pubsub_keepalive_sec);
  }
  if (pubsub_user && (pubsub_user.length()>0)) {
    LEAF_NOTICE("Using MQTT username %s", pubsub_user.c_str());
    LEAF_NOTICE("Using MQTT password %s", pubsub_pass.c_str());//NOCOMMIT
    mqttClient.setCredentials(pubsub_user.c_str(), pubsub_pass.c_str());
  }

  mqttClient.onConnect(
    [](bool sessionPresent) {
      NOTICE("mqtt onConnect callback");
      PubsubEspAsyncMQTTLeaf *that = (PubsubEspAsyncMQTTLeaf *)Leaf::get_leaf_by_name(leaves, String("espmqtt"));
      if (!that) {
	ALERT("I don't know who I am!");
	return;
      }
      struct PubsubEventMessage msg = {.code=PUBSUB_EVENT_CONNECT, .context=(int)sessionPresent};
      that->eventQueueSend(&msg);
    });

  mqttClient.onDisconnect(
    [](AsyncMqttClientDisconnectReason reason) {
      NOTICE("mqtt onDisconnect callback");
      PubsubEspAsyncMQTTLeaf *that = (PubsubEspAsyncMQTTLeaf *)Leaf::get_leaf_by_name(leaves, String("espmqtt"));
      if (!that) {
	ALERT("I don't know who I am!");
	return;
      }
      struct PubsubEventMessage msg = {.code=PUBSUB_EVENT_DISCONNECT, .context=(int)reason};
      that->eventQueueSend(&msg);
    });

  mqttClient.onSubscribe(
    [](uint16_t packetId, uint8_t qos)
    {
	   PubsubEspAsyncMQTTLeaf *that = (PubsubEspAsyncMQTTLeaf *)Leaf::get_leaf_by_name(leaves, String("espmqtt"));
	   if (!that) {
	     ALERT("I don't know who I am!");
	     return;
	   }
	   struct PubsubEventMessage msg = {.code=PUBSUB_EVENT_SUBSCRIBE_DONE, .context=(int)(qos<<16|packetId)};
	   that->eventQueueSend(&msg);
    });

  mqttClient.onUnsubscribe(
    [](uint16_t packetId){
	   PubsubEspAsyncMQTTLeaf *that = (PubsubEspAsyncMQTTLeaf *)Leaf::get_leaf_by_name(leaves, String("espmqtt"));
	   if (!that) {
	     ALERT("I don't know who I am!");
	     return;
	   }
	   struct PubsubEventMessage msg = {.code=PUBSUB_EVENT_UNSUBSCRIBE_DONE, .context=(int)packetId};
	   that->eventQueueSend(&msg);
    });

  mqttClient.onPublish(
    [](uint16_t packetId){
	   PubsubEspAsyncMQTTLeaf *that = (PubsubEspAsyncMQTTLeaf *)Leaf::get_leaf_by_name(leaves, String("espmqtt"));
	   if (!that) {
	     ALERT("I don't know who I am!");
	     return;
	   }
	   struct PubsubEventMessage msg = {.code=PUBSUB_EVENT_PUBLISH_DONE, .context=(int)packetId};
    });

   mqttClient.onMessage(
     [](char* topic,
			    char* payload,
			    AsyncMqttClientMessageProperties properties,
			    size_t len,
			    size_t index,
	size_t total){
            if (len==0) { return; } // some weird ass-bug in mosquitto causes double messages, one real, one with null payload
	    PubsubEspAsyncMQTTLeaf *that = (PubsubEspAsyncMQTTLeaf *)Leaf::get_leaf_by_name(leaves, String("espmqtt"));
	    if (!that) {
	      ALERT("I don't know who I am!");
	      return;
	    }
	    that->_mqtt_receive_callback(topic,payload,properties,len,index,total);
     });

   if (hasPriority()) {
     snprintf(lwt_topic, sizeof(lwt_topic), "%sadmin/status/presence", base_topic.c_str());
   }
   else {
     snprintf(lwt_topic, sizeof(lwt_topic), "%sstatus/presence", base_topic.c_str());
   }
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
  bool handled = AbstractPubsubLeaf::mqtt_receive(type, name, topic, payload);
  LEAF_ENTER(L_DEBUG);
  LEAF_NOTICE("PubsubEspAsyncMQTTLeaf::RECV [%s] <= [%s]", topic.c_str(), payload.c_str());

  WHEN("_ip_connect", {
    if (pubsub_autoconnect) {
      LEAF_NOTICE("IP is online, autoconnecting MQTT");
      pubsubConnect();
    }
    })
  ELSEWHENPREFIX("cmd/join/", {
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
  });
  
  return handled;
}

void PubsubEspAsyncMQTTLeaf::loop()
{
  AbstractPubsubLeaf::loop();
  //ENTER(L_DEBUG);

  static unsigned long lastHeartbeat = 0;
  unsigned long now = millis();

  struct PubsubEventMessage event;
  struct PubsubReceiveMessage msg;

  while (xQueueReceive(event_queue, &event, 10)) {
    //LEAF_DEBUG("Received pubsub event %d", (int)event.code)
    switch (event.code) {
    case PUBSUB_EVENT_CONNECT:
      LEAF_DEBUG("Received connection event");
      pubsubSetSessionPresent(event.context);
      pubsubOnConnect(!event.context);
      break;
    case PUBSUB_EVENT_DISCONNECT:
      LEAF_ALERT("Disconnected from MQTT (%d %s)", event.context, ((event.context>=0) && (event.context<=7))?pubsub_esp_disconnect_reasons[event.context]:"unknown");
      pubsubDisconnect(false);
      pubsubOnDisconnect();
      break;
    case PUBSUB_EVENT_SUBSCRIBE_DONE: {
      int packetId = event.context & 0xFFFF;
      LEAF_DEBUG("Subscribe acknowledged %d", (int)packetId);
    }
      break;
    case PUBSUB_EVENT_UNSUBSCRIBE_DONE: {
      int packetId = event.context & 0xFFFF;
      LEAF_DEBUG("Unsubscribe acknowledged %d", (int)packetId);
    }
      break;
    case PUBSUB_EVENT_PUBLISH_DONE: {
      int packetId = event.context & 0xFFFF;
      LEAF_DEBUG("Publish acknowledged %d", (int)packetId);
      if (event.context == sleep_pub_id) {
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
      break;
    }
  }

  while (xQueueReceive(receive_queue, &msg, 10)) {
    LEAF_NOTICE("MQTT message from server %s <= [%s] (q%d%s)",
		msg.topic->c_str(), msg.payload->c_str(), (int)msg.properties.qos, msg.properties.retain?" retain":"");

    this->_mqtt_receive(*msg.topic, *msg.payload);
    delete msg.topic;
    delete msg.payload;
  }

    
  


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
bool PubsubEspAsyncMQTTLeaf::pubsubConnect() {
  AbstractPubsubLeaf::pubsubConnect();
  bool result=false;

  LEAF_ENTER(L_WARN);
  if (canRun() && ipLeaf && ipLeaf->isConnected()) {
    LEAF_NOTICE("Connecting to MQTT at %s...",pubsub_host.c_str());
    mqttClient.connect();
    LEAF_NOTICE("MQTT Connection initiated");
    result = true;
  }
  else {
    pubsubScheduleReconnect();
  }

  LEAF_BOOL_RETURN(result);
}

void PubsubEspAsyncMQTTLeaf::pubsubDisconnect(bool deliberate) 
{
  AbstractPubsubLeaf::pubsubDisconnect(deliberate);
  LEAF_ENTER(L_NOTICE);
  mqttClient.disconnect();
  LEAF_VOID_RETURN;
}


void PubsubEspAsyncMQTTLeaf::pubsubOnConnect(bool do_subscribe)
{
  AbstractPubsubLeaf::pubsubOnConnect(do_subscribe);

  LEAF_ENTER(L_NOTICE);
  LEAF_NOTICE("Connected to MQTT.  pubsub_session_present=%s", TRUTH(pubsub_session_present));

  // Once connected, publish an announcement...
  mqtt_publish("status/presence", "online", 0, true);
  if (wake_reason.length()) {
    mqtt_publish("status/wake", wake_reason, 0, true);
  }
  if (ipLeaf) {
    mqtt_publish("status/ip", ipLeaf->ipAddressString(), 0, true);
  }
  mqtt_subscribe("get/#", HERE);
  mqtt_subscribe("set/#", HERE);
  if (hasPriority()) {
    mqtt_subscribe("normal/read-request/#", HERE);
    mqtt_subscribe("normal/write-request/#", HERE);
    mqtt_subscribe("admin/cmd/#", HERE);
  }

  // ... and resubscribe
  if (use_wildcard_topic) {
    mqtt_subscribe("cmd/#", HERE);
  }
  else {
    mqtt_subscribe("cmd/restart", HERE);
    mqtt_subscribe("cmd/setup", HERE);
    mqtt_subscribe("cmd/join", HERE);
#ifdef _OTA_OPS_H
    mqtt_subscribe("cmd/update", HERE);
    mqtt_subscribe("cmd/rollback", HERE);
    mqtt_subscribe("cmd/bootpartition", HERE);
    mqtt_subscribe("cmd/nextpartition", HERE);
#endif
    mqtt_subscribe("cmd/ping", HERE);
    mqtt_subscribe("cmd/leaves", HERE);
    mqtt_subscribe("cmd/format", HERE);
    mqtt_subscribe("cmd/status", HERE);
    mqtt_subscribe("cmd/subscriptions", HERE);
    mqtt_subscribe("set/name", HERE);
    mqtt_subscribe("set/debug", HERE);
    mqtt_subscribe("set/debug_wait", HERE);
    mqtt_subscribe("set/debug_lines", HERE);
    mqtt_subscribe("set/debug_flush", HERE);
  }
  
  LEAF_NOTICE("MQTT Connection setup complete");
  LEAF_LEAVE;
}

uint16_t PubsubEspAsyncMQTTLeaf::_mqtt_publish(String topic, String payload, int qos, bool retain)
{
  LEAF_ENTER(L_DEBUG);
  
  uint16_t packetId = 0;
  //ENTER(L_DEBUG);
  const char *topic_c_str = topic.c_str();
  const char *payload_c_str = payload.c_str();
  LEAF_NOTICE("PUB %s => [%s]", topic_c_str, payload_c_str);

  if (pubsub_loopback) {
    LEAF_INFO("LOOPBACK PUB %s => %s", topic_c_str, payload_c_str);
    pubsub_loopback_buffer += topic + ' ' + payload + '\n';
    LEAF_RETURN(0);
  }

  if (pubsub_connected) {
    packetId = mqttClient.publish(topic.c_str(), qos, retain, payload_c_str);
    //DEBUG("Publish initiated, ID=%d", packetId);
  }
  else {
    LEAF_ALERT("Publish skipped while MQTT connection is down: %s=>%s", topic_c_str, payload_c_str);
  }
  yield();
  //LEAVE;
  LEAF_RETURN(packetId);
}


void PubsubEspAsyncMQTTLeaf::_mqtt_subscribe(String topic, int qos)
{
  LEAF_ENTER(L_DEBUG);
  LEAF_NOTICE("SUB %s", topic.c_str());
  if (pubsub_connected) {
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
  if (pubsub_connected) {
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

void PubsubEspAsyncMQTTLeaf::_mqtt_receive_callback(char* topic,
			    char* payload,
			    AsyncMqttClientMessageProperties properties,
			    size_t len,
			    size_t index,
			    size_t total) {
  LEAF_ENTER(L_DEBUG);

  // handle message arrived
  static char payload_buf[512];
  if (len > sizeof(payload_buf)-1) len=sizeof(payload_buf)-1;
  memcpy(payload_buf, payload, len);
  payload_buf[len]='\0';
  (void)index;
  (void)total;

  // dont log in interrupt context
  //LEAF_NOTICE("MQTT message from server %s <= [%s] (q%d%s)",
  //topic, payload_buf, (int)properties.qos, properties.retain?" retain":"");
  struct PubsubReceiveMessage msg={.topic=new String(topic), .payload=new String(payload_buf), .properties=properties};
  receiveQueueSend(&msg);
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
