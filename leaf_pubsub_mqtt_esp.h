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

class PubsubEspAsyncMQTTLeaf;

PubsubEspAsyncMQTTLeaf *pubsub_wifi_leaf = NULL;

class PubsubEspAsyncMQTTLeaf : public AbstractPubsubLeaf
{
public:
  PubsubEspAsyncMQTTLeaf(String name, String target="", bool use_ssl=false, bool use_device_topic=true, bool run=true)
    : AbstractPubsubLeaf(name, target, use_ssl, use_device_topic)
    , TraitDebuggable(name)
  {
    LEAF_ENTER(L_INFO);
    do_heartbeat = false;
    this->run = run;
    this->impersonate_backplane = true;
    this->pubsub_keepalive_sec = 60;
    LEAF_LEAVE;
  }

  virtual void setup();
  virtual void loop(void);
  virtual void status_pub();
  virtual uint16_t _mqtt_publish(String topic, String payload, int qos=0, bool retain=false);
  virtual void _mqtt_subscribe(String topic, int qos=0,codepoint_t where=undisclosed_location);
  virtual void _mqtt_unsubscribe(String topic);
  virtual bool mqtt_receive(String type, String name, String topic, String payload);


  virtual bool pubsubConnect(void) ;
  virtual void pubsubDisconnect(bool deliberate=true) ;
  virtual void processEvent(struct PubsubEventMessage *msg);
  virtual void processReceive(struct PubsubReceiveMessage *msg);
  void eventQueueSend(struct PubsubEventMessage *msg) 
  {
#ifdef ESP32
    xQueueGenericSend(event_queue, (void *)msg, (TickType_t)0, queueSEND_TO_BACK);
#else
    processEvent(msg);
#endif
  }
  void receiveQueueSend(struct PubsubReceiveMessage *msg) 
  {
#ifdef ESP32
    xQueueGenericSend(receive_queue, (void *)msg, (TickType_t)0, queueSEND_TO_BACK);
#else
    processReceive(msg);
#endif
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
#ifdef ESP32
  QueueHandle_t receive_queue;
  QueueHandle_t event_queue;
#endif

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
#ifdef ESP32
  receive_queue = xQueueCreate(10, sizeof(struct PubsubReceiveMessage));
  event_queue = xQueueCreate(10, sizeof(struct PubsubEventMessage));
#endif
  mqttClient.setServer(pubsub_host.c_str(), pubsub_port);
  if (hasPriority() && (getPriority()=="service")) {
    char buf[2*DEVICE_ID_MAX];
    snprintf(buf, sizeof(buf), "%s-%s", device_id, getPriority().c_str());
    LEAF_NOTICE("Using augmented client-id \"%s\"", buf);
    mqttClient.setClientId(buf);
  }
  else {
    LEAF_NOTICE("Using simple client-id \"%s\"", device_id);
    mqttClient.setClientId(device_id);
  }
  mqttClient.setCleanSession(pubsub_use_clean_session);
  if (pubsub_keepalive_sec) {
    mqttClient.setKeepAlive(pubsub_keepalive_sec);
  }
  if (pubsub_user && (pubsub_user.length()>0)) {
    LEAF_NOTICE("Using MQTT username %s", pubsub_user.c_str());
    mqttClient.setCredentials(pubsub_user.c_str(), pubsub_pass.c_str());
  }

  mqttClient.onConnect(
    [](bool sessionPresent) {
      if (!pubsub_wifi_leaf) {
	ALERT("I don't know who I am!");
	return;
      }
      struct PubsubEventMessage msg = {.code=PUBSUB_EVENT_CONNECT, .context=(int)sessionPresent};
      pubsub_wifi_leaf->eventQueueSend(&msg);
    });

  mqttClient.onDisconnect(
    [](AsyncMqttClientDisconnectReason reason) {
      NOTICE("mqtt onDisconnect callback");
      if (!pubsub_wifi_leaf) {
	ALERT("I don't know who I am!");
	return;
      }
      struct PubsubEventMessage msg = {.code=PUBSUB_EVENT_DISCONNECT, .context=(int)reason};
      pubsub_wifi_leaf->eventQueueSend(&msg);
    });

  mqttClient.onSubscribe(
    [](uint16_t packetId, uint8_t qos)
    {
	   if (!pubsub_wifi_leaf) {
	     ALERT("I don't know who I am!");
	     return;
	   }
	   struct PubsubEventMessage msg = {.code=PUBSUB_EVENT_SUBSCRIBE_DONE, .context=(int)(qos<<16|packetId)};
	   pubsub_wifi_leaf->eventQueueSend(&msg);
    });

  mqttClient.onUnsubscribe(
    [](uint16_t packetId){
	   if (!pubsub_wifi_leaf) {
	     ALERT("I don't know who I am!");
	     return;
	   }
	   struct PubsubEventMessage msg = {.code=PUBSUB_EVENT_UNSUBSCRIBE_DONE, .context=(int)packetId};
	   pubsub_wifi_leaf->eventQueueSend(&msg);
    });

  mqttClient.onPublish(
    [](uint16_t packetId){
	   if (!pubsub_wifi_leaf) {
	     ALERT("I don't know who I am!");
	     return;
	   }
	   struct PubsubEventMessage msg = {.code=PUBSUB_EVENT_PUBLISH_DONE, .context=(int)packetId};
	   pubsub_wifi_leaf->eventQueueSend(&msg);
    });

   mqttClient.onMessage(
     [](char* topic,
			    char* payload,
			    AsyncMqttClientMessageProperties properties,
			    size_t len,
			    size_t index,
	size_t total){
            if (len==0) { return; } // some weird ass-bug in mosquitto causes double messages, one real, one with null payload
	    if (!pubsub_wifi_leaf) {
	      ALERT("I don't know who I am!");
	      return;
	    }
	    pubsub_wifi_leaf->_mqtt_receive_callback(topic,payload,properties,len,index,total);
     });

   if (isPriority("service")) {
     snprintf(lwt_topic, sizeof(lwt_topic), "%sservice/status/presence", base_topic.c_str());
   }
   else if (hasPriority()) {
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
   pubsub_wifi_leaf = this;
   

   LEAF_LEAVE;
}

void PubsubEspAsyncMQTTLeaf::status_pub() 
{
  char status[32];
  uint32_t secs;
  if (pubsub_connected) {
    secs = (millis() - pubsub_connect_time)/1000;
    snprintf(status, sizeof(status), "%s online %d:%02d", getNameStr(), secs/60, secs%60);
  }
  else {
    secs = (millis() - pubsub_disconnect_time)/1000;
    snprintf(status, sizeof(status), "%s offline %d:%02d", getNameStr(), secs/60, secs%60);
  }
  mqtt_publish("status/pubsub_status", status);
}


bool PubsubEspAsyncMQTTLeaf::mqtt_receive(String type, String name, String topic, String payload)
{
  bool handled = AbstractPubsubLeaf::mqtt_receive(type, name, topic, payload);
  LEAF_ENTER(L_DEBUG);
  LEAF_NOTICE("PubsubEspAsyncMQTTLeaf::RECV [%s] <= [%s]", topic.c_str(), payload.c_str());

  WHENFROM("wifi", "_ip_connect", {
    if (canRun() && canStart() && pubsub_autoconnect) {
      LEAF_NOTICE("IP/wifi is online, autoconnecting WiFi MQTT");
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
        DBGPRINT(".");
      }
      DBGPRINTLN();
      if (WiFi.status() != WL_CONNECTED) {
	LEAF_ALERT("    WiFi failed");
      }
      else {
	LEAF_NOTICE("    WiFi connected");
      }
  })
  ELSEWHEN("cmd/pubsub_status",{
      // this looks like it could be handled by common code in abstract_pubsub, but no.
      // it is potentially going to be handled as a leaf command by multiple pubsub leaves
      status_pub();
  })
  ;
  
  return handled;
}

void PubsubEspAsyncMQTTLeaf::processEvent(struct PubsubEventMessage *event) 
{
  LEAF_INFO("Received pubsub event %d", (int)event->code)
  switch (event->code) {
  case PUBSUB_EVENT_CONNECT:
    LEAF_NOTICE("Received connection event");
    pubsubSetSessionPresent(event->context);
    // do not call pubsubOnConnect here, set the flag and the loop will detect and call
    pubsub_connected=true;
    break;
  case PUBSUB_EVENT_DISCONNECT:
    LEAF_ALERT("Disconnected from MQTT (%d %s)", event->context, ((event->context>=0) && (event->context<=7))?pubsub_esp_disconnect_reasons[event->context]:"unknown");
    pubsubDisconnect(false);
    pubsubOnDisconnect();
    break;
  case PUBSUB_EVENT_SUBSCRIBE_DONE: {
    int packetId = event->context & 0xFFFF;
    LEAF_NOTICE("Subscribe acknowledged %d", (int)packetId);
  }
    break;
  case PUBSUB_EVENT_UNSUBSCRIBE_DONE: {
    int packetId = event->context & 0xFFFF;
    LEAF_NOTICE("Unsubscribe acknowledged %d", (int)packetId);
  }
    break;
  case PUBSUB_EVENT_PUBLISH_DONE: {
    int packetId = event->context & 0xFFFF;
    LEAF_NOTICE("Publish acknowledged %d", (int)packetId);
    // this appears to never get called
    //ipLeaf->ipCommsState(REVERT, HERE);
    if (event->context == sleep_pub_id) {
      LEAF_NOTICE("Going to sleep for %d ms", sleep_duration_ms);
#ifdef ESP8266
      ESP.deepSleep(1000*sleep_duration_ms, WAKE_RF_DEFAULT);
#else
      esp_sleep_enable_timer_wakeup(sleep_duration_ms * 1000);
      DBGFLUSH();
      esp_deep_sleep_start();
#endif
    }
  }
  default:
    LEAF_WARN("Unhandled pubsub event %d", (int)event->code)
    break;
  }
}

void PubsubEspAsyncMQTTLeaf::processReceive(struct PubsubReceiveMessage *msg) 
{
  LEAF_NOTICE("MQTT message from server %s <= [%s] (q%d%s)",
	      msg->topic->c_str(), msg->payload->c_str(), (int)msg->properties.qos, msg->properties.retain?" retain":"");

  this->_mqtt_receive(*msg->topic, *msg->payload);
  delete msg->topic;
  delete msg->payload;
}

void PubsubEspAsyncMQTTLeaf::loop()
{
  AbstractPubsubLeaf::loop();
  //ENTER(L_DEBUG);

  static unsigned long lastHeartbeat = 0;
  unsigned long now = millis();

#ifdef ESP32
  struct PubsubEventMessage event;
  struct PubsubReceiveMessage msg;

  while (xQueueReceive(event_queue, &event, 10)) {
    processEvent(&event);
  }
  

  while (xQueueReceive(receive_queue, &msg, 10)) {
    processReceive(&msg);
  }
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
bool PubsubEspAsyncMQTTLeaf::pubsubConnect() {
  LEAF_NOTICE("connecting pubsub using ESP wifi (%s)", ipLeaf?ipLeaf->describe().c_str():"NO IP");
  
  if (!AbstractPubsubLeaf::pubsubConnect()) {
    // superclass says no
    return false;
  }

  LEAF_ENTER(L_NOTICE);
  bool result=false;

  if (canRun() && ipLeaf && ipLeaf->isConnected()) {
    LEAF_NOTICE("Connecting to MQTT at %s...",pubsub_host.c_str());
    mqttClient.connect();
    LEAF_NOTICE("MQTT Connection initiated");
    result = true;
  }
  else {
    if (!ipLeaf) {
      LEAF_WARN("No IP leaf");
    }
    else if (!ipLeaf->isConnected()) {
      LEAF_NOTICE("IP leaf (%s) is not connected", ipLeaf->describe().c_str());
    }
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

uint16_t PubsubEspAsyncMQTTLeaf::_mqtt_publish(String topic, String payload, int qos, bool retain)
{
  LEAF_ENTER(L_DEBUG);
  
  uint16_t packetId = 0;
  //ENTER(L_DEBUG);
  const char *topic_c_str = topic.c_str();
  const char *payload_c_str = payload.c_str();
  LEAF_NOTICE("PUB %s => [%s]", topic_c_str, payload_c_str);
  ipLeaf->ipCommsState(TRANSACTION, HERE);

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
    if (pubsub_autoconnect) {
      LEAF_ALERT("Publish skipped while MQTT connection is down: %s=>%s", topic_c_str, payload_c_str);
    }
  }
  ipLeaf->ipCommsState(REVERT, HERE);
#ifndef ESP8266
  yield();
#endif
  //LEAVE;
  LEAF_RETURN(packetId);
}


void PubsubEspAsyncMQTTLeaf::_mqtt_subscribe(String topic, int qos,codepoint_t where)
{
  LEAF_ENTER(L_DEBUG);
  LEAF_NOTICE_AT(CODEPOINT(where), "SUB %s", topic.c_str());
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
  char payload_buf[512];
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
