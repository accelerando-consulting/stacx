#pragma once
#ifndef ESP32
#error This leaf requires ESP32
#endif

#include "abstract_pubsub.h"
#include "abstract_ip.h"
#include <mqtt_client.h>

struct PubsubIdfReceiveMessage
{
  String *topic;
  String *payload;
};

struct PubsubIdfEventMessage
{
  enum PubsubEventCode code;
  int context;
};

esp_err_t _pubsub_esp_idf_event(esp_mqtt_event_handle_t event);

//
//@********************** class PubsubESPIDFLeaf ***********************
//
// This class encapsulates an Mqtt connection that utilises an Espressif's
// MQTT stack
//
//
class PubsubMQTTEspIdfLeaf : public AbstractPubsubLeaf
{
protected:
  esp_mqtt_client_config_t mqtt_config;
  esp_mqtt_client_handle_t mqtt_handle;

  Ticker mqttReconnectTimer;
  QueueHandle_t receive_queue;
  QueueHandle_t event_queue;

public:
  PubsubMQTTEspIdfLeaf(String name, String target, bool use_ssl=true, bool use_device_topic=true, bool run = true)
    : AbstractPubsubLeaf(name, target, use_ssl, use_device_topic)
    , Debuggable(name)
  {
    LEAF_ENTER(L_INFO);
    do_heartbeat = false;
    this->run = run;
    this->impersonate_backplane = true;
    this->pubsub_keepalive_sec = 60;
    // further the setup happens in the superclass

#ifdef PUBSUB_WIFI_AUTOCONNECT
    this->pubsub_autoconnect = PUBSUB_WIFI_AUTOCONNECT;
#endif
    
    LEAF_LEAVE;
  }

  virtual void setup();
  virtual void loop(void);
  virtual void status_pub();
  virtual uint16_t _mqtt_publish(String topic, String payload, int qos=0, bool retain=false);
  virtual void _mqtt_subscribe(String topic, int qos=0, codepoint_t where=undisclosed_location);
  virtual void _mqtt_unsubscribe(String topic, int level=L_NOTICE);
  virtual bool mqtt_receive(String type, String name, String topic, String payload, bool direct=false);
  virtual void initiate_sleep_ms(int ms);

  virtual bool pubsubConnect(void);
  virtual void pubsubDisconnect(bool deliberate=true);
  virtual void pre_sleep(int duration=0);

  void eventQueueSend(struct PubsubIdfEventMessage *msg);
  void receiveQueueSend(struct PubsubIdfReceiveMessage *msg);
  void processEvent(struct PubsubIdfEventMessage *msg);
  void processReceive(struct PubsubIdfReceiveMessage *msg);

  esp_err_t _mqttEvent(esp_mqtt_event_t *event);

};

void PubsubMQTTEspIdfLeaf::setup()
{
  AbstractPubsubLeaf::setup();
  LEAF_ENTER(L_NOTICE);

  receive_queue = xQueueCreate(10, sizeof(struct PubsubIdfReceiveMessage));
  event_queue = xQueueCreate(10, sizeof(struct PubsubIdfEventMessage));
  pubsub_client_id = String(device_id)+"-wifi" ;
  memset(&mqtt_config, 0, sizeof(mqtt_config));

  mqtt_config.host = strdup(pubsub_host.c_str());
  mqtt_config.port = pubsub_port;
  mqtt_config.client_id = pubsub_client_id.c_str();
  if (pubsub_user) {
    mqtt_config.username = pubsub_user.c_str();
  }
  if (pubsub_pass) {
    mqtt_config.password = pubsub_pass.c_str();
  }
  mqtt_config.event_handle = &_pubsub_esp_idf_event;
  mqtt_config.user_context = this;
  mqtt_config.keepalive = pubsub_keepalive_sec;
  mqtt_config.disable_clean_session = pubsub_use_clean_session?0:1;

  char lwt_topic[256];
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
  pubsub_lwt_topic = String(lwt_topic);
  mqtt_config.lwt_topic = pubsub_lwt_topic.c_str();
  mqtt_config.lwt_msg_len = pubsub_lwt_topic.length();

  LEAF_NOTICE("Initialising client handle");
  mqtt_handle = esp_mqtt_client_init(&mqtt_config);
  if (mqtt_handle==NULL) {
    LEAF_ALERT("ERROR initialising MQTT");
  }

  LEAF_LEAVE;
}

esp_err_t PubsubMQTTEspIdfLeaf::_mqttEvent(esp_mqtt_event_t *event) 
{
  struct PubsubIdfEventMessage msg;
  struct PubsubIdfReceiveMessage rmsg;
  static char topic_buf[1025];
  static char payload_buf[1025];

  switch ((int)event->event_id)
  {
  case MQTT_EVENT_CONNECTED:
    LEAF_NOTICE("MQTT_EVENT_CONNECTED");
    msg.code = PUBSUB_EVENT_CONNECT;
    eventQueueSend(&msg);
    break;
  case MQTT_EVENT_DISCONNECTED:
    LEAF_NOTICE("MQTT_EVENT_DISCONNECTED");
    msg.code = PUBSUB_EVENT_DISCONNECT;
    eventQueueSend(&msg);
    break;
  case MQTT_EVENT_SUBSCRIBED:
    memcpy(topic_buf, event->topic, event->topic_len);
    topic_buf[event->topic_len]='\0';
    LEAF_INFO("MQTT_EVENT_SUBSCRIBED [%s]", topic_buf);
    //msg.code = PUBSUB_EVENT_SUBSCRIBE_DONE;
    //msg.context = event->msg_id;
    //eventQueueSend(&msg);
    break;
  case MQTT_EVENT_UNSUBSCRIBED:
    memcpy(topic_buf, event->topic, event->topic_len);
    topic_buf[event->topic_len]='\0';
    LEAF_INFO("MQTT_EVENT_UNSUBSCRIBED [%s]", topic_buf);
    //msg.code = PUBSUB_EVENT_UNSUBSCRIBE_DONE;
    //msg.context = event->msg_id;
    //eventQueueSend(&msg);
    break;
  case MQTT_EVENT_PUBLISHED:
    memcpy(topic_buf, event->topic, event->topic_len);
    topic_buf[event->topic_len]='\0';
    LEAF_INFO("MQTT_EVENT_PUBLISHED [%s]", topic_buf);
    //msg.code = PUBSUB_EVENT_PUBLISH_DONE;
    //msg.context = event->msg_id;
    //eventQueueSend(&msg);
    break;
  case MQTT_EVENT_DATA:
    memcpy(topic_buf, event->topic, event->topic_len);
    topic_buf[event->topic_len]='\0';
    memcpy(payload_buf, event->data, event->data_len);
    payload_buf[event->data_len]='\0';
    LEAF_INFO("MQTT_EVENT_DATA [%s] <= [%s]\n", topic_buf, payload_buf);
    rmsg.topic = new String(topic_buf);
    rmsg.payload = new String(payload_buf);
    receiveQueueSend(&rmsg);
    break;
  case MQTT_EVENT_ERROR:
    LEAF_NOTICE("MQTT_EVENT_ERROR");
    break;
  case MQTT_EVENT_BEFORE_CONNECT:
    LEAF_NOTICE("MQTT_EVENT_BEFORE_CONNECT\n");
    break;
  case MQTT_EVENT_DELETED:
    LEAF_NOTICE("MQTT_EVENT_DELETED\n");
    break;
  default:
    Serial.printf("Unhandled event type %d\n", (int)event->event_id );
    break;
  }
  return ESP_OK;
}

void PubsubMQTTEspIdfLeaf::eventQueueSend(struct PubsubIdfEventMessage *msg)
{
  INFO("eventQueueSend type %d", (int)msg->code);
  xQueueGenericSend(event_queue, (void *)msg, (TickType_t)0, queueSEND_TO_BACK);
}

void PubsubMQTTEspIdfLeaf::receiveQueueSend(struct PubsubIdfReceiveMessage *msg)
{
  xQueueGenericSend(receive_queue, (void *)msg, (TickType_t)0, queueSEND_TO_BACK);
}


void PubsubMQTTEspIdfLeaf::status_pub()
{
  char status[48];
  uint32_t secs;
  if (pubsub_connected) {
    secs = (millis() - pubsub_connect_time)/1000;
    snprintf(status, sizeof(status), "%s online %d:%02d", getNameStr(), secs/60, secs%60);
  }
  else {
    secs = (millis() - pubsub_disconnect_time)/1000;
    snprintf(status, sizeof(status), "%s offline %d:%02d", getNameStr(), secs/60, secs%60);
  }
  if (pubsub_status_log) {
    message("fs", "cmd/log/" PUBSUB_LOG_FILE, status);
  }
  mqtt_publish("status/pubsub_status", status);
}

bool PubsubMQTTEspIdfLeaf::mqtt_receive(String type, String name, String topic, String payload, bool direct)
{
  LEAF_ENTER(L_DEBUG);
  bool handled = false;

  LEAF_INFO("%s, %s", topic.c_str(), payload.c_str());

  WHENFROM("wifi", "_ip_connect",{
    if (ipLeaf) {
      if (pubsub_autoconnect) {
	LEAF_NOTICE("IP is online, autoconnecting MQTT");
	pubsubConnect();
      }
    }
    })
  ELSEWHENFROM("wifi", "_ip_disconnect",{
    if (pubsub_connected) {
      pubsubDisconnect();
    }
  })
  ELSEWHEN("cmd/pubsub_status",{
    bool was = pubsub_status_log;
    if (payload == "log") {
      pubsub_status_log = true;
    }
    status_pub();
    pubsub_status_log = was;
  });
  if (!handled) {
    handled = AbstractPubsubLeaf::mqtt_receive(type, name, topic, payload, direct);
  }

  return handled;
}

void PubsubMQTTEspIdfLeaf::processEvent(struct PubsubIdfEventMessage *event)
{
  LEAF_ENTER_CSTR(L_NOTICE, pubsub_event_names[event->code]);
  switch (event->code) {
  case PUBSUB_EVENT_CONNECT:
    pubsubSetConnected();        
    pubsubOnConnect(true);
    break;
  case PUBSUB_EVENT_DISCONNECT:
    pubsubSetConnected(false);        
    pubsubOnDisconnect();
    break;
  default:
    LEAF_ALERT("Event code %s unhandled", pubsub_event_names[event->code]);
  }
  LEAF_LEAVE;
}


void PubsubMQTTEspIdfLeaf::processReceive(struct PubsubIdfReceiveMessage *msg)
{
  LEAF_NOTICE("MQTT message from server %s <= [%s]",
	      msg->topic->c_str(), msg->payload->c_str());
  this->_mqtt_route(*msg->topic, *msg->payload);
  delete msg->topic;
  delete msg->payload;
}

void PubsubMQTTEspIdfLeaf::loop()
{
  AbstractPubsubLeaf::loop();

  unsigned long now = millis();

  if (pubsub_reconnect_due) {
    LEAF_NOTICE("Attempting reconenct");
    pubsub_reconnect_due=false;
    pubsubConnect();
  }


  if (!pubsub_connect_notified && pubsub_connected) {
    LEAF_NOTICE("Notifying of pubsub connection");
    pubsubOnConnect(true);
    publish("_pubsub_connect",String(1));
    pubsub_connect_notified = pubsub_connected;
  }
  else if (pubsub_connect_notified && !pubsub_connected) {
    LEAF_NOTICE("Notifying of pubsub disconnection");
    publish("_pubsub_disconnect", String(1));
    pubsub_connect_notified = pubsub_connected;
  }

  struct PubsubIdfEventMessage event;
  struct PubsubIdfReceiveMessage msg;

  // deliberately don't consume all messages, leave some for future loops
  if (xQueueReceive(event_queue, &event, 10)) {
    LEAF_NOTICE("Event received from queue");
    processEvent(&event);
  }
  else if (xQueueReceive(receive_queue, &msg, 10)) {
    LEAF_NOTICE("Message received from queue");
    processReceive(&msg);
  }

}

void PubsubMQTTEspIdfLeaf::pre_sleep(int duration)
{
  pubsubDisconnect(true);
}

//
// Initiate connection to MQTT server
//
bool PubsubMQTTEspIdfLeaf::pubsubConnect() {

  if (!AbstractPubsubLeaf::pubsubConnect()) {
    // superclass says no
    LEAF_NOTICE("Connection aborted");
    return false;
  }

  LEAF_ENTER(L_NOTICE);
  bool result = false;

  //DEBUG_AUGMENT(level, 4);
  //DEBUG_AUGMENT(flush, 1);
  //DEBUG_AUGMENT(wait, 50);
  
  
  if (canRun() && ipLeaf && ipLeaf->isConnected()) {
    LEAF_NOTICE("Connecting to MQTT at %s:%d as %s...",pubsub_host.c_str(), pubsub_port, pubsub_client_id.c_str());

    if (pubsub_connected && !pubsub_reuse_connection) {
      LEAF_WARN("Disconnecting stale MQTT connection");
      esp_err_t err = esp_mqtt_client_stop(mqtt_handle);
      if (err != ESP_OK) {
	LEAF_ALERT("DISCONNECT ERROR %d\n", (int)err);
      }
    }

    LEAF_NOTICE("Starting MQTT client task");
    esp_err_t err = esp_mqtt_client_start(mqtt_handle);
    if (err != ESP_OK) {
      LEAF_ALERT("CONNECT ERROR %d\n", (int)err);
      result = false;
    }
    else {
      result = true;
    }
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

void PubsubMQTTEspIdfLeaf::pubsubDisconnect(bool deliberate) {
  AbstractPubsubLeaf::pubsubDisconnect(deliberate);
  LEAF_ENTER(L_NOTICE);
  esp_err_t err = esp_mqtt_client_stop(mqtt_handle);
  if (err != ESP_OK) {
    LEAF_ALERT("DISCONNECT ERROR %d\n", (int)err);
  }
  LEAF_VOID_RETURN;
}
 
uint16_t PubsubMQTTEspIdfLeaf::_mqtt_publish(String topic, String payload, int qos, bool retain)
{
  LEAF_ENTER(L_DEBUG);
  LEAF_INFO("PUB %s => [%s]", topic.c_str(), payload.c_str());
  int i;
  bool published = false;

  if (pubsub_loopback) {
    sendLoopback(topic, payload);
    return 0;
  }

  if (pubsub_connected
#ifdef ESP32
      && !pubsub_always_queue
#endif
    ) {
    if (ipLeaf) {
      ipLeaf->ipCommsState(TRANSACTION, HERE);
    }
    else {
      LEAF_ALERT("WTF ipLeaf is null");
    }
    LEAF_DEBUG("Initiate publish %s", topic.c_str());
    int pub_result = esp_mqtt_client_publish(mqtt_handle, topic.c_str(), payload.c_str(), payload.length(), qos, retain);
    if (pub_result < 0) {
      LEAF_ALERT("Publish failed");
    }
    if (ipLeaf) {
      ipLeaf->ipCommsState(REVERT, HERE);
      published = true;
    }
  }
#ifdef ESP32
  if (!published && send_queue) {
    LEAF_DEBUG("Queueing publish");
    _mqtt_queue_publish(topic, payload, qos, retain);
  }
#endif
  else if (pubsub_warn_noconn) {
    LEAF_WARN("Publish skipped while MQTT connection is down: %s=>%s", topic.c_str(), payload.c_str());
  }

#ifndef ESP8266
  yield();
#endif
  //LEAVE;
  LEAF_RETURN(1);
}


void PubsubMQTTEspIdfLeaf::_mqtt_subscribe(String topic, int qos, codepoint_t where)
{
  LEAF_ENTER(L_INFO);

  LEAF_NOTICE_AT(CODEPOINT(where), "MQTT SUB %s", topic.c_str());
  if (pubsub_connected) {
    int result = esp_mqtt_client_subscribe(mqtt_handle, topic.c_str(), qos);
    if (result < 0) {
      LEAF_INFO("Subscription FAILED for topic=%s", topic.c_str());
    }
    else {
      if (pubsub_subscriptions) {
	pubsub_subscriptions->put(topic, qos);
      }
    }
  }
  else {
    LEAF_ALERT("Warning: Subscription attempted while MQTT connection is down (%s)", topic.c_str());
  }
    
  LEAF_LEAVE;
}

void PubsubMQTTEspIdfLeaf::_mqtt_unsubscribe(String topic, int level)
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
    int result = esp_mqtt_client_unsubscribe(mqtt_handle, topic.c_str());
    if (result < 0) {
      LEAF_ALERT("Unsubscription FAILED for topic=%s", topic.c_str());
    }
    else {
      //LEAF_DEBUG("UNSUBSCRIPTION initiated topic=%s", topic.c_str());
      if (pubsub_subscriptions) {
	pubsub_subscriptions->remove(topic);
      }
    }
  }
  else {
    LEAF_ALERT("Warning: Unsubscription attempted while MQTT connection is down (%s)", topic.c_str());
  }
}

void PubsubMQTTEspIdfLeaf::initiate_sleep_ms(int ms)
{
  LEAF_NOTICE("Prepare for deep sleep");

  mqtt_publish("event/sleep",String(millis()/1000,10));

  // Apply sleep in reverse order, highest level leaf first
  int leaf_index;
  for (leaf_index=0; leaves[leaf_index]; leaf_index++);
  for (leaf_index--; leaf_index>=0; leaf_index--) {
    leaves[leaf_index]->pre_sleep(ms/1000);
  }

  if (ms == 0) {
    LEAF_ALERT("Initiating indefinite deep sleep (wake source GPIO0)");
  }
  else {
    LEAF_ALERT("Initiating deep sleep (wake sources GPIO0 plus timer %dms)", ms);
  }

  Serial.flush();
  if (ms != 0) {
    // zero means forever
    esp_sleep_enable_timer_wakeup(ms * 1000ULL);
  }
  esp_sleep_enable_ext0_wakeup((gpio_num_t)0, 0);

  esp_deep_sleep_start();
}

// global callback, which routes to leaf via leaf pointer in user_context

esp_err_t _pubsub_esp_idf_event(esp_mqtt_event_handle_t event)
{
  PubsubMQTTEspIdfLeaf *that = (PubsubMQTTEspIdfLeaf *)event->user_context;
  if (that) {
    return that->_mqttEvent(event);
  }
  return ESP_OK;
}



// Local Variables:
// mode: C++
// c-basic-offset: 2
// End:
