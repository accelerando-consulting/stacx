#include <AsyncMqttClient.h>
#include "abstract_pubsub.h"

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

const char *pubsub_event_names[] = {
  "CONNECT",
  "DISCONNECT",
  "SUBSCRIBE_DONE",
  "UNSUBSCRIBE_DONE",
  "PUBLISH_DONE"
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
  PubsubEspAsyncMQTTLeaf(
    String name,
    String target="",
    bool use_ssl = false,
    bool use_device_topic=true,
    bool run=true
    )
    : AbstractPubsubLeaf(name, target, use_ssl, use_device_topic)
    , Debuggable(name)
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
  virtual void _mqtt_unsubscribe(String topic, int level = L_NOTICE);
  virtual bool mqtt_receive(String type, String name, String topic, String payload, bool direct=false);


  virtual bool pubsubConnect(void) ;
  virtual void pubsubDisconnect(bool deliberate=true) ;
  virtual void processEvent(struct PubsubEventMessage *msg);
  virtual void processReceive(struct PubsubReceiveMessage *msg);

  void eventQueueSend(struct PubsubEventMessage *msg)
  {
    INFO("eventQueueSend type %d", (int)msg->code);
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
  LEAF_ENTER(L_INFO);

#ifdef ESP32
  receive_queue = xQueueCreate(10, sizeof(struct PubsubReceiveMessage));
  event_queue = xQueueCreate(10, sizeof(struct PubsubEventMessage));
#endif

  //
  // Set up the MQTT Client
  //
  LEAF_NOTICE("MQTT Setup [%s:%d] %s", pubsub_host.c_str(), pubsub_port, device_id);

  //
  // Set up callbacks
  //
  mqttClient.onConnect(
    [](bool sessionPresent) {
      if (!pubsub_wifi_leaf) {
	ALERT("I don't know who I am!");
	return;
      }
      NOTICE("MQTT connect event");
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
       size_t total)
    {
      if (len==0) { return; } // some weird ass-bug in mosquitto causes double messages, one real, one with null payload
      if (!pubsub_wifi_leaf) {
	ALERT("I don't know who I am!");
	return;
      }
      pubsub_wifi_leaf->_mqtt_receive_callback(topic,payload,properties,len,index,total);
    });

  //
  // Set connection parameters
  //
  LEAF_NOTICE("setServer %s:%d", pubsub_host.c_str(), pubsub_port);
  mqttClient.setServer(pubsub_host.c_str(), pubsub_port);

#if 0
  if (hasPriority() && (getPriority()=="service")) {
    char buf[2*DEVICE_ID_MAX];
    snprintf(buf, sizeof(buf), "%s-s", device_id);
    LEAF_NOTICE("Using augmented client-id \"%s\"", buf);
    mqttClient.setClientId(buf);
    pubsub_client_id = buf;
  }
  else {
    LEAF_NOTICE("Using simple client-id \"%s\"", device_id);
    mqttClient.setClientId(device_id);
    pubsub_client_id = device_id;
  }
#else
  pubsub_client_id = String(device_id)+"-wifi" ;
#endif
  
  mqttClient.setCleanSession(pubsub_use_clean_session);
  if (pubsub_keepalive_sec) {
    mqttClient.setKeepAlive(pubsub_keepalive_sec);
  }

  if (pubsub_user && (pubsub_user.length()>0)) {
    LEAF_NOTICE("Using MQTT username %s", pubsub_user.c_str());
    mqttClient.setCredentials(pubsub_user.c_str(), pubsub_pass.c_str());
  }

  if (isPriority("service")) {
    snprintf(lwt_topic, sizeof(lwt_topic), "%sservice/status/presence", base_topic.c_str());
  }
  else if (hasPriority()) {
    snprintf(lwt_topic, sizeof(lwt_topic), "%sadmin/status/presence", base_topic.c_str());
  }
  else {
    snprintf(lwt_topic, sizeof(lwt_topic), "%sstatus/presence", base_topic.c_str());
  }
  LEAF_NOTICE("LWT topic is %s", lwt_topic);
  mqttClient.setWill(lwt_topic, 0, true, "offline");

#if ASYNC_TCP_SSL_ENABLED
  #error wtf
   LEAF_NOTICE("MQTT will use %s",use_ssl?"SSL":"plain-text");
   mqttClient.setSecure(pubsub_use_ssl);
   if (pubsub_use_ssl) {
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
    if (secs >= 86400) {
      // more than one day
      snprintf(status, sizeof(status), "%s online as %s %dd:%dh%dm:%02d", getNameStr(), pubsub_client_id.c_str(), secs/86400,(secs%86400)/3600,(secs%3600)/60, secs%60);
    }
    else if (secs > 3600) {
      // less than one day
      snprintf(status, sizeof(status), "%s online as %s %dh%dm:%02d", getNameStr(), pubsub_client_id.c_str(), secs/3600,(secs%3600)/60, secs%60);
    }
    else {
      // less than one hour
      snprintf(status, sizeof(status), "%s online as %s %d:%02d", getNameStr(), pubsub_client_id.c_str(), secs/60, secs%60);
    }
  }
  else {
    secs = (millis() - pubsub_disconnect_time)/1000;
    snprintf(status, sizeof(status), "%s offline %d:%02d", getNameStr(), secs/60, secs%60);
  }
  mqtt_publish("status/pubsub_status", status);
}


bool PubsubEspAsyncMQTTLeaf::mqtt_receive(String type, String name, String topic, String payload, bool direct)
{
  bool handled = false;
  LEAF_ENTER(L_DEBUG);
  LEAF_INFO("PubsubEspAsyncMQTTLeaf::RECV [%s] <= [%s]", topic.c_str(), payload.c_str());

  WHENFROM("wifi", "_ip_connect", {
    if (canRun() && canStart() && pubsub_autoconnect) {
      LEAF_NOTICE("IP/wifi is online, autoconnecting WiFi MQTT");
      if (!pubsubConnect()) {
	// failed, try again later
	pubsubScheduleReconnect();
      }
    }
    })
  ELSEWHENFROM("wifi", "status/time_source", {
    if (canRun() && canStart() && !pubsub_connected && pubsub_autoconnect) {
      LEAF_NOTICE("IP/wifi got time from NTP, retry MQTT");
      pubsubSetReconnectDue();
    }
    })
  ELSEWHENPREFIX("cmd/join/", {
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
      //
      // This command looks like it could be handled
      // by common code in the base class, but no!
      // This command needs to be handled by
      // potentially more than one instance of
      // different subclasses, whereas if the base
      // class were to handle it then it would only
      // get handled once.
      //
      // Number of times you have attempted to refactor
      // this but then rediscovered the above warning: 3
      //
      status_pub();
  })
  else {
    LEAF_DEBUG("Offer topic to superclass handled=%s", TRUTH_lc(handled));
    handled = AbstractPubsubLeaf::mqtt_receive(type, name, topic, payload, direct);
  }
  if (!handled) {
    LEAF_INFO("Did not handle topic %s", topic.c_str());
  }

  return handled;
}

void PubsubEspAsyncMQTTLeaf::processEvent(struct PubsubEventMessage *event)
{
  LEAF_INFO("Received pubsub event %d (%s)", (int)event->code, pubsub_event_names[event->code]);
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
    LEAF_INFO("Subscribe acknowledged %d", (int)packetId);
  }
    break;
  case PUBSUB_EVENT_UNSUBSCRIBE_DONE: {
    int packetId = event->context & 0xFFFF;
    LEAF_INFO("Unsubscribe acknowledged %d", (int)packetId);
  }
    break;
  case PUBSUB_EVENT_PUBLISH_DONE: {
    int packetId = event->context & 0xFFFF;
    LEAF_INFO("Publish acknowledged %d", (int)packetId);
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

  this->_mqtt_route(*msg->topic, *msg->payload);
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
  LEAF_ENTER(L_DEBUG);
  LEAF_NOTICE("connecting pubsub using ESP wifi (%s)", ipLeaf?ipLeaf->describe().c_str():"NO IP");

  if (!AbstractPubsubLeaf::pubsubConnect()) {
    // superclass says no
    LEAF_NOTICE("Connection aborted");
    return false;
  }

#if USE_NTP
  if (ipLeaf && (ipLeaf->getTimeSource()==0) && (pubsub_connect_attempt_count==1)) {
    LEAF_NOTICE("Delay pubsub connect until clock is set");
    pubsubScheduleReconnect();
    LEAF_BOOL_RETURN(false);
  }
#endif
  
  bool result=false;

  if (canRun() && ipLeaf && ipLeaf->isConnected()) {
    LEAF_NOTICE("Connecting to MQTT at %s:%d as %s...",pubsub_host.c_str(), pubsub_port, pubsub_client_id.c_str());

    if (mqttClient.connected() && !pubsub_reuse_connection) {
      LEAF_WARN("Disconnecting stale MQTT connection");
      mqttClient.disconnect();
    }

    mqttClient.setClientId(pubsub_client_id.c_str());
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

  if (pubsub_loopback) {
    sendLoopback(topic, payload);
    return 0;
  }

  uint16_t packetId = 0;
  //ENTER(L_DEBUG);
  const char *topic_c_str = topic.c_str();
  const char *payload_c_str = payload.c_str();
  LEAF_NOTICE("PUB %s => [%s] qos=%d retain=%s", topic_c_str, payload_c_str, qos, TRUTH_lc(retain));

  if (pubsub_connected) {
    if (ipLeaf) {
      ipLeaf->ipCommsState(TRANSACTION, HERE);
    }
    else {
      LEAF_ALERT("WTF ipLeaf is null");
    }
    packetId = mqttClient.publish(topic.c_str(), qos, retain, payload_c_str);
    LEAF_DEBUG("Publish initiated, ID=%d", packetId);
    if (ipLeaf) {
      ipLeaf->ipCommsState(REVERT, HERE);
    }
  }
#ifdef ESP32
  else if (send_queue) {
    LEAF_DEBUG("Queueing publish");
    _mqtt_queue_publish(topic, payload, qos, retain);
  }
#endif
  else if (pubsub_warn_noconn) {
    LEAF_WARN("Publish skipped while MQTT connection is down: %s=>%s", topic_c_str, payload_c_str);
  }

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

void PubsubEspAsyncMQTTLeaf::_mqtt_unsubscribe(String topic, int level)
{
  LEAF_ENTER_STR(level, topic);

  if (topic == "ALL") {
    for (int i=0; i<pubsub_subscriptions->size(); i++) {
      String t = pubsub_subscriptions->getKey(i);
      if (t != "ALL") _mqtt_unsubscribe(t);
    }
    LEAF_VOID_RETURN;
  }


  if (pubsub_connected) {
    uint16_t packetIdSub = mqttClient.unsubscribe(topic.c_str());
    //LEAF_DEBUG("UNSUBSCRIPTION initiated id=%d topic=%s", (int)packetIdSub, topic.c_str());
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
