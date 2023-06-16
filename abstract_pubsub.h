//@**************************** class AbstractPubsub ******************************
//
// This class encapsulates a publish-subscribe mechanism such as ESP32's EspAsyncMQTT,
// the MQTT AT-command interface provided by a cellular modem, or LoRAWAN.
//
#ifndef _LEAF_PUBSUB_ABSTRACT
#define _LEAF_PUBSUB_ABSTRACT

#include "abstract_storage.h"
#include <StreamString.h>

bool pubsub_loopback = false;

#define PUBSUB_LOOPBACK 1
#define PUBSUB_SHELL 2

#ifndef PUBSUB_HOST_DEFAULT
#define PUBSUB_HOST_DEFAULT "mqtt.lan"
#endif
#ifndef PUBSUB_PORT_DEFAULT
#define PUBSUB_PORT_DEFAULT 1883
#endif
#ifndef PUBSUB_USER_DEFAULT
#define PUBSUB_USER_DEFAULT ""
#endif
#ifndef PUBSUB_PASS_DEFAULT
#define PUBSUB_PASS_DEFAULT ""
#endif
#ifndef PUBSUB_BROKER_HEARTBEAT_TOPIC
#define PUBSUB_BROKER_HEARTBEAT_TOPIC ""
#endif
#ifndef PUBSUB_BROKER_KEEPALIVE_SEC
#define PUBSUB_BROKER_KEEPALIVE_SEC 660
#endif

#ifndef PUBSUB_RECONNECT_SECONDS
#define PUBSUB_RECONNECT_SECONDS 20
#endif
#ifndef PUBSUB_CONNECT_ATTEMPT_LIMIT
#define PUBSUB_CONNECT_ATTEMPT_LIMIT 0
#endif
#ifndef PUBSUB_SEND_QUEUE_SIZE
#define PUBSUB_SEND_QUEUE_SIZE 10
#endif

#ifndef PUBSUB_LOG_CONNECT
#define PUBSUB_LOG_CONNECT false
#endif

#ifndef PUBSUB_LOG_FILE
#define PUBSUB_LOG_FILE STACX_LOG_FILE
#endif


#define PUBSUB_SSL_ENABLE true
#define PUBSUB_SSL_DISABLE false
#define PUBSUB_DEVICE_TOPIC_ENABLE true
#define PUBSUB_DEVICE_TOPIC_DISABLE false


struct PubsubSendQueueMessage
{
  String *topic;
  String *payload;
  byte qos;
  bool retain;
};

extern bool check_bod();


class AbstractPubsubLeaf : public Leaf
{
public:

  AbstractPubsubLeaf(String name, String target="", bool use_ssl = false, bool use_device_topic=true)
    : Leaf("pubsub", name)
    , Debuggable(name)
  {
    do_heartbeat = false;
    this->tap_targets = target;
    this->pubsub_use_ssl = use_ssl;
    this->pubsub_use_device_topic = use_device_topic ;
    this->impersonate_backplane = true;
  }

  virtual void setup();
  virtual void start();
  virtual void stop();
  virtual void loop();
  virtual void pubsubScheduleReconnect();
  virtual bool isConnected() { return pubsub_connected; }
  virtual void pubsubSetConnected(bool state=true) {
    LEAF_NOTICE("pubsubSetConnected %s", TRUTH_lc(state));
    pubsub_connected=state;
  }
  virtual bool isAutoConnect() { return pubsub_autoconnect; }
  void pubsubSetReconnectDue() {pubsub_reconnect_due=true;};

  void post_error(enum post_error e, int count)
  {
    if (pubsub_log_connect) {
    char buf[80];
    snprintf(buf, sizeof(buf), "%s ERROR %s %d", getNameStr(), post_error_names[e], count);
    message("fs", "cmd/appendl/" IP_LOG_FILE, buf);
    }
    ::post_error(e, count);
  }

  virtual void pubsubOnDisconnect(){
    LEAF_ENTER(L_INFO);
    if (ipLeaf->isConnected()) {
      ipLeaf->ipCommsState(WAIT_PUBSUB, HERE);
    }
    publish("_pubsub_disconnect", String(1));
    pubsub_connecting = false;
    pubsubSetConnected(false);
    pubsub_disconnect_time=millis();
    ACTION("PUBSUB disc (%s)", ipLeaf->getNameStr());
    if (pubsub_log_connect) {
      char buf[80];
      int duration_sec = (pubsub_disconnect_time-pubsub_connect_time)/1000;
      snprintf(buf, sizeof(buf), "%s disconnect %d duration=%d uptime=%lu clock=%lu",
	       getNameStr(),
	       pubsub_connect_count,
	       duration_sec,
	       (unsigned long)millis(),
	       (unsigned long)time(NULL));
      WARN("%s", buf);
      message("fs", "cmd/appendl/" PUBSUB_LOG_FILE, buf);
    }


    for (int i=0; leaves[i]; i++) {
      if (leaves[i]->canRun()) {
	leaves[i]->mqtt_disconnect();
      }
    }
    LEAF_VOID_RETURN;
  }
  bool pubsubUseDeviceTopic(){return pubsub_use_device_topic;}
  virtual void pubsubOnConnect(bool do_subscribe=true);

  virtual bool pubsubConnect(void){
    LEAF_ENTER(L_INFO);

    if (!ipLeaf->isConnected()) {
      LEAF_NOTICE("Delay pubsub connect until IP is ready");
      LEAF_BOOL_RETURN(false);
    }

    ACTION("PUBSUB try (%s)", ipLeaf->getNameStr());
    ipLeaf->ipCommsState(TRY_PUBSUB,HERE);//signal attempt in progress
    pubsub_connecting = true;
    pubsub_connect_attempt_count++;
    if (pubsub_log_connect) {
      char buf[80];
      snprintf(buf, sizeof(buf), "%s attempt %d uptime=%lu clock=%lu",
	       getNameStr(),
	       pubsub_connect_attempt_count,
	       (unsigned long)millis(),
	       (unsigned long)time(NULL));
      WARN("%s", buf);
      message("fs", "cmd/appendl/" PUBSUB_LOG_FILE, buf);
    }


    LEAF_BOOL_RETURN(true);
  }
  virtual void pubsubDisconnect(bool deliberate=true){
    LEAF_ENTER_BOOL(L_INFO, deliberate);
    if (!deliberate && pubsub_autoconnect) pubsubScheduleReconnect();
    LEAF_VOID_RETURN;
  };
  virtual bool valueChangeHandler(String topic, Value *v);
  virtual bool commandHandler(String type, String name, String topic, String payload);
  virtual void flushSendQueue(int count = 0);
  virtual int sendQueueCount()
  {
    return (int)uxQueueMessagesWaiting(send_queue);
  }


  virtual uint16_t _mqtt_publish(String topic, String payload, int qos=0, bool retain=false)=0;
  virtual bool _mqtt_queue_publish(String topic, String payload, int qos=0, bool retain=false);
  virtual void _mqtt_subscribe(String topic, int qos=0, codepoint_t where=undisclosed_location)=0;

  virtual void _mqtt_unsubscribe(String topic)=0;
  virtual bool wants_topic(String type, String name, String topic);
  virtual void _mqtt_route(String topic, String payload, int flags = 0);
  virtual bool mqtt_receive(String type, String name, String topic, String payload, bool direct=false);
  virtual void initiate_sleep_ms(int ms);
  virtual void pubsubSetSessionPresent(bool p) { pubsub_session_present = p; };

  virtual void enableLoopback(Stream *s=NULL) {pubsub_loopback = ::pubsub_loopback = true; if (s) loopback_stream=s; }
  virtual void cancelLoopback() { pubsub_loopback = ::pubsub_loopback = false;;}
  virtual void setLoopbackStream(Stream *s) { loopback_stream=s; }
  virtual bool isLoopback() { return pubsub_loopback; }
  virtual void sendLoopback(String &topic, String &payload) { if (loopback_stream) { loopback_stream->printf("%s %s\n", topic.c_str(), payload.c_str()); }}

  // deprecated methods
  virtual bool connect(void) {LEAF_ALERT("connect method is deprecated");return pubsubConnect();}
  virtual void disconnect(bool deliberate=true){LEAF_ALERT("disconnect method is deprecated");pubsubDisconnect(deliberate);}

protected:
  String pubsub_host=PUBSUB_HOST_DEFAULT;
  int pubsub_port = PUBSUB_PORT_DEFAULT;
  String pubsub_user=PUBSUB_USER_DEFAULT;
  String pubsub_pass=PUBSUB_PASS_DEFAULT;
  String pubsub_lwt_topic="";
  String pubsub_broker_heartbeat_topic=PUBSUB_BROKER_HEARTBEAT_TOPIC;
  String pubsub_client_id="";
  int pubsub_broker_keepalive_sec = PUBSUB_BROKER_KEEPALIVE_SEC;
  unsigned long last_broker_heartbeat = 0;

  int pubsub_keepalive_sec = 120;

  bool pubsub_use_device_topic = false;
  bool pubsub_autoconnect = true;
  bool pubsub_ip_autoconnect = true;
  bool pubsub_log_connect = PUBSUB_LOG_CONNECT;
  bool pubsub_reuse_connection = false;
  bool pubsub_connected = false;
  bool pubsub_connecting = false;
  bool pubsub_connect_notified = false;
  bool pubsub_session_present = false;
  bool pubsub_use_clean_session = true;
  bool pubsub_use_ssl = false;
  bool pubsub_use_status = USE_STATUS;
  bool pubsub_use_event = USE_EVENT;
  bool pubsub_use_set = USE_SET;
  bool pubsub_use_get = USE_GET;
  bool pubsub_use_cmd = USE_CMD;
  bool pubsub_use_flat_topic = USE_FLAT_TOPIC;
  bool pubsub_use_wildcard_topic = USE_WILDCARD_TOPIC;
  bool pubsub_warn_noconn = false;
  bool pubsub_onconnect_ip = true;
  bool pubsub_onconnect_signal = true;
  bool pubsub_onconnect_uptime = true;
  bool pubsub_onconnect_wake = true;
  bool pubsub_onconnect_mac = true;
  bool pubsub_onconnect_time = false;
  bool pubsub_subscribe_allcall = false;
  bool pubsub_subscribe_mac = false;
  size_t heap_free_prev = 0;

  bool pubsub_use_ssl_client_cert = false;
  bool pubsub_loopback = false;
  Stream *loopback_stream = NULL;
  int pubsub_connect_timeout_ms = 10000;
  int pubsub_connect_count = 0;
  int pubsub_connect_attempt_count = 0;
  int pubsub_connect_attempt_limit = PUBSUB_CONNECT_ATTEMPT_LIMIT;
  uint32_t pubsub_connect_time = 0;
  uint32_t pubsub_disconnect_time = 0;
  int pubsub_reconnect_interval_sec = PUBSUB_RECONNECT_SECONDS;
  Ticker pubsub_reconnect_timer;
  bool pubsub_reconnect_due = false;
  SimpleMap<String,int> *pubsub_subscriptions = NULL;

#ifdef ESP32
  int pubsub_send_queue_size = PUBSUB_SEND_QUEUE_SIZE;
  QueueHandle_t send_queue = NULL;
#endif
  unsigned long pubsub_dequeue_delay = 500;

};

void AbstractPubsubLeaf::setup(void)
{
  Leaf::setup();
  LEAF_ENTER(L_INFO);
  LEAF_NOTICE("Pubsub client will %s use device-topic", pubsubUseDeviceTopic()?"use":"not use");

#ifndef ESP8266
  pubsub_subscriptions = new SimpleMap<String,int>(_compareStringKeys);
#endif

#ifdef ESP32
  if (pubsub_send_queue_size) {
    LEAF_WARN("Create pubsub send queue of size %d", pubsub_send_queue_size);
    send_queue = xQueueCreate(pubsub_send_queue_size, sizeof(struct PubsubSendQueueMessage));
  }
#endif

  registerCommand(HERE,"setup", "enter wifi setup mode");
  registerCommand(HERE,"reboot", "reboot the device");
  registerCommand(HERE,"pubsub_connect", "initiate (re-) connection to pubsub broker");
  registerCommand(HERE,"pubsub_disconnect", "close any connection to pubsub broker");
  registerCommand(HERE,"pubsub_status", "report the status of pubsub connection");
  registerCommand(HERE,"pubsub_clean", "disconnect and reestablish a clean session to pubsub broker");
  registerCommand(HERE,"pubsub_subscribe", "Subscribe to a new topic");
  registerCommand(HERE,"pubsub_unsubscribe", "Unsubscribe from a subscribed topic");
#ifdef ESP32
  registerCommand(HERE,"pubsub_sendq_flush", "flush send queue");
  registerCommand(HERE,"pubsub_sendq_stat", "print send queue status");
#endif
  registerCommand(HERE,"reboot", "reboot the module");
  registerCommand(HERE,"update", "Perform a firmware update from the payload URL");
  registerCommand(HERE,"update_test", "Simulate a firmware update from the payload URL");
  registerCommand(HERE,"wifi_update", "Perform a firmware update from the payload URL, using wifi only");
  registerCommand(HERE,"lte_update", "Perform a firmware update from the payload URL, using LTE only");
  registerCommand(HERE,"lte_update_test", "Simulate a firmware update from the payload URL, using LTE only");
  registerCommand(HERE,"rollback", "Roll back the last firmware update");
  registerCommand(HERE,"bootpartition", "Publish the currently active boot partition");
  registerCommand(HERE,"nextpartition", "Publish the next available boot partition");
  registerCommand(HERE,"otherpartition", "Switch to the next available boot partition");
  registerCommand(HERE,"ping", "Return the supplied payload to status/ack");
  registerCommand(HERE,"post", "Flash a power-on-self-test blink code");
  registerCommand(HERE,"ip", "Publish current ip address to status/ip");
  registerCommand(HERE,"subscriptions", "Publish the currently subscribed topics");
  registerCommand(HERE,"leaf/list", "List active stacx leaves");
  registerCommand(HERE,"leaf/status", "List status of active stacx leaves");
  registerCommand(HERE,"leaf/setup", "Run the setup method of the named leaf");
  registerCommand(HERE,"leaf/inhibit", "Disable the named leaf");
  registerCommand(HERE,"leaf/disable", "Disable the named leaf");
  registerCommand(HERE,"leaf/enable", "Enable the named leaf");
  registerCommand(HERE,"leaf/start", "Start the named leaf");
  registerCommand(HERE,"leaf/stop", "Stop the named leaf");
  registerCommand(HERE,"sleep", "Enter lower power mode (optional value in seconds)");
  registerCommand(HERE,"brownout_disable", "Disable the brownout-detector");
  registerCommand(HERE,"brownout_enable", "Enable the brownout-detector");
  registerCommand(HERE,"brownout_status", "Report the status of the brownout-detector");
  registerCommand(HERE,"memstat", "print memory usage statistics");

#if USE_WDT
  registerCommand(HERE,"starve", "Deliberately trigger watchdog timer)");
#endif

  registerBoolValue("pubsub_use_get", &use_get, "Subscribe to get topics");
  registerBoolValue("pubsub_use_set", &use_set, "Subscribe to set topics");
  registerBoolValue("pubsub_use_cmd", &use_cmd, "Subscribe to command topics");
  registerBoolValue("pubsub_use_flat_topic", &use_flat_topic, "Use verb-noun not verb/noun in topics");
  registerBoolValue("pubsub_use_wildcard_topic", &use_wildcard_topic, "Subscribe using wildcards");
  registerBoolValue("pubsub_use_status", &pubsub_use_status, "Publish status messages");
  use_status = pubsub_use_status;
  registerBoolValue("pubsub_use_event", &pubsub_use_event, "Publish event messages");
  use_event = pubsub_use_event;
  registerBoolValue("pubsub_log_connect", &pubsub_log_connect, "Log pubsub connect events to flash");

  registerBoolValue("pubsub_use_ssl", &pubsub_use_ssl, "Use SSL for pubsub server connection");
  registerBoolValue("pubsub_use_ssl_client_cert", &pubsub_use_ssl_client_cert, "Use a client certificate for SSL");
  registerBoolValue("pubsub_use_clean_session", &pubsub_use_clean_session, "Enable MQTT Clean Session");

  registerBoolValue("pubsub_onconnect_ip", &pubsub_onconnect_ip, "Publish device's IP address upon connection");
  registerBoolValue("pubsub_onconnect_signal", &pubsub_onconnect_ip, "Publish device's network signal strength upon connection");
  registerBoolValue("pubsub_onconnect_uptime", &pubsub_onconnect_uptime, "Publish device's uptime upon connection");
  registerBoolValue("pubsub_onconnect_wake", &pubsub_onconnect_wake, "Publish device's wake reason upon connection");
  registerBoolValue("pubsub_onconnect_mac", &pubsub_onconnect_mac, "Publish device's MAC address upon connection");
  registerBoolValue("pubsub_subscribe_allcall", &pubsub_subscribe_allcall, "Subscribe to all-call topic (*/#)");
  registerBoolValue("pubsub_subscribe_mac", &pubsub_subscribe_mac, "Subscribe to a backup topic based on last 6 digits of mac address");
  registerStrValue("pubsub_broker_heartbeat_topic", &pubsub_broker_heartbeat_topic, "Broker heartbeat topic (disconnect if this topic is not seen after pubsub_broker_keepalive_sec)");
  registerIntValue("pubsub_broker_keepalive_sec", &pubsub_broker_keepalive_sec, "Duration of no message to pubsub_broker_heartbeat_topic after which broker connection is considered dead");
  registerUlongValue("pubsub_broker_heartbeat_last", &last_broker_heartbeat, "Time of the last seen heartbeat from the broker", ACL_GET_ONLY, VALUE_NO_SAVE);

  registerStrValue("pubsub_host", &pubsub_host, "Host to which publish-subscribe client connects");
  registerIntValue("pubsub_port", &pubsub_port, "Port to which publish-subscribe client connects");
  registerStrValue("pubsub_user", &pubsub_user, "Pub-sub server username");
  registerStrValue("pubsub_pass", &pubsub_pass, "Pub-sub server password");
  registerIntValue("pubsub_keepalive_sec", &pubsub_keepalive_sec, "Keepalive value for MQTT");
  registerIntValue("pubsub_connect_timeout_ms", &pubsub_connect_timeout_ms, "MQTT connect timeout in milliseconds");
  registerBoolValue("pubsub_autoconnect", &pubsub_autoconnect, "Automatically connect to pub-sub when IP connects");
  registerBoolValue("pubsub_reuse_connection", &pubsub_reuse_connection, "If pubsub is already connected at boot time, re-use connection");
  registerBoolValue("pubsub_ip_autoconnect", &pubsub_ip_autoconnect, "Automatically connect IP layer when needing to publish");
  registerBoolValue("pubsub_warn_noconn", &pubsub_warn_noconn, "Log a warning if unable to publish due to no connection");
  registerIntValue("pubsub_connect_attempt_limit", &pubsub_connect_attempt_limit);
  registerIntValue("pubsub_connect_attempt_count", &pubsub_connect_attempt_count,"",ACL_GET_ONLY, VALUE_NO_SAVE);


#ifdef ESP32
  registerIntValue("pubsub_send_queue_size", &pubsub_send_queue_size);
#endif

  LEAF_NOTICE("Pubsub settings host=[%s] port=%d user=[%s] auto=%s", pubsub_host.c_str(), pubsub_port, pubsub_user.c_str(), TRUTH_lc(pubsub_autoconnect));

  LEAF_LEAVE;
}

void AbstractPubsubLeaf::start()
{
  Leaf::start();

  if (ipLeaf) {
    if (isAutoConnect() && ipLeaf->isConnected() && !pubsub_connecting) {
      LEAF_NOTICE("PUBSUB autoconnecting at startup");
      pubsubConnect();
    }
    else {
      if (isAutoConnect()) {
	LEAF_NOTICE("Delaying pubsub connection: network layer not connected");
      }
    }
  }
  else {
    LEAF_NOTICE("IP leaf not found");
  }
}

void AbstractPubsubLeaf::stop(void)
{
  LEAF_ENTER(L_WARN);
  pubsubDisconnect(true); // deliberate disconnect, no reschedule
  this->loop();  // process any state hooks
  Leaf::stop();
  LEAF_LEAVE;
}

void AbstractPubsubLeaf::initiate_sleep_ms(int ms)
{
  LEAF_NOTICE("Prepare for deep sleep");

  mqtt_publish("event/sleep",String(millis()/1000));

  // Apply sleep in reverse order, highest level leaf first
  int leaf_index;
  for (leaf_index=0; leaves[leaf_index]; leaf_index++);
  for (leaf_index--; leaf_index>=0; leaf_index--) {
    LEAF_NOTICE("Call pre_sleep for leaf %d: %s", leaf_index, leaves[leaf_index]->describe().c_str());
    leaves[leaf_index]->pre_sleep(ms/1000);
  }

#if defined(USE_HELLO_PIN)||defined(USE_HELLO_PIXEL)
  hello_off();
#endif
  ACTION("SLEEP");
#ifdef ESP32
  if (ms == 0) {
    LEAF_ALERT("Initiating indefinite deep sleep (wake source GPIO0)");
  }
  else {
    LEAF_ALERT("Initiating deep sleep (wake sources GPIO0 plus timer %dms)", ms);
  }
  DBGFLUSH();

  if (ms != 0) {
    // zero means forever
    esp_sleep_enable_timer_wakeup(ms * 1000ULL);
  }
#if !defined(ARDUINO_ESP32C3_DEV) && !defined(ARDUINO_TTGO_T_OI_PLUS_DEV)
  esp_sleep_enable_ext0_wakeup((gpio_num_t)0, 0);
#endif

  esp_deep_sleep_start();
#else
  //FIXME sleep not implemented on esp8266
#endif
}

void AbstractPubsubLeaf::pubsubOnConnect(bool do_subscribe)
{
  LEAF_ENTER_BOOL(L_INFO, do_subscribe);

  if (pubsub_reconnect_timer.active()) {
    LEAF_WARN("Cancelling pubsub reconnect timer");
    pubsub_reconnect_timer.detach();
  }

  pubsubSetConnected(true);
  pubsub_connecting = false;
  ++pubsub_connect_count;
  ipLeaf->ipCommsState(ONLINE, HERE);
  ACTION("PUBSUB conn %s", ipLeaf->getNameStr());
  if (pubsub_log_connect) {
    char buf[80];
    snprintf(buf, sizeof(buf), "%s connect %d attempts=%d uptime=%lu clock=%lu",
	     getNameStr(),
	     pubsub_connect_count,
	     pubsub_connect_attempt_count,
	     (unsigned long)millis(),
	     (unsigned long)time(NULL));
    WARN("%s", buf);
    message("fs", "cmd/appendl/" PUBSUB_LOG_FILE, buf);
  }
  pubsub_connect_attempt_count=0;


  publish("_pubsub_connect",String(1));
  last_external_input = millis();

  mqtt_publish("status/presence", "online", 0, true);

  if (pubsub_connect_count==1) {
    // publish device info on the first connect after wake (not on subsequent)
    if (pubsub_onconnect_wake && wake_reason) {
      mqtt_publish("status/wake", wake_reason, 0, true);
    }
    if (ipLeaf && pubsub_onconnect_uptime) {
      mqtt_publish("status/uptime", String(millis()/1000));
    }
    if (ipLeaf && pubsub_onconnect_signal) {
      mqtt_publish("status/signal", String(ipLeaf->getRssi()));
    }
    if (ipLeaf && pubsub_onconnect_ip) {

      mqtt_publish("status/ip", ipLeaf->ipAddressString(), 0, true);
      mqtt_publish("status/transport", ipLeaf->getName(), 0, true);
    }
    if (pubsub_onconnect_mac) {
      mqtt_publish("status/mac", mac, 0, true);
    }
  }

  if (do_subscribe) {
    // we skip this if the modem told us "already connected, dude", which
    // can happen after sleep.

    if (pubsub_broker_heartbeat_topic.length() > 0) {
      // subscribe to broker heartbeats
      _mqtt_subscribe(pubsub_broker_heartbeat_topic, 0, HERE);
      // consider the broker online as of now
      last_broker_heartbeat = millis();
    }

    //_mqtt_subscribe("ping",0,HERE);
    //mqtt_subscribe(_ROOT_TOPIC+"*/#", HERE); // all-call topics
    if (pubsub_use_wildcard_topic) {
      if (hasPriority()) {
	mqtt_subscribe(getPriority()+"/read-request/#", HERE);
	mqtt_subscribe(getPriority()+"/write-request/#", HERE);
	mqtt_subscribe("admin/cmd/#", HERE);
	mqtt_subscribe("admin/get/#", HERE);
	mqtt_subscribe("admin/set/#", HERE);
      }
      else {
	mqtt_subscribe("cmd/#", HERE);
	mqtt_subscribe("get/#", HERE);
	mqtt_subscribe("set/#", HERE);
	if (pubsubUseDeviceTopic()) {
	  mqtt_subscribe("+/+/cmd/#", HERE);
	  mqtt_subscribe("+/+/get/#", HERE);
	  mqtt_subscribe("+/+/set/#", HERE);
	}
      }
    }
    else {
      mqtt_subscribe("cmd/restart",HERE);
      mqtt_subscribe("cmd/setup",HERE);
#ifdef _OTA_OPS_H
      mqtt_subscribe("cmd/update", HERE);
      mqtt_subscribe("cmd/rollback", HERE);
      mqtt_subscribe("cmd/bootpartition", HERE);
      mqtt_subscribe("cmd/nextpartition", HERE);
      mqtt_subscribe("cmd/otherpartition", HERE);
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

    if (pubsub_subscribe_allcall) {
      _mqtt_subscribe(_ROOT_TOPIC+"*/#", 0, HERE);
    }
    if (pubsub_subscribe_mac) {
      _mqtt_subscribe(_ROOT_TOPIC+mac_short+"/#", 0, HERE);
    }

    //LEAF_INFO("Set up leaf subscriptions");
    for (int i=0; leaves[i]; i++) {
      Leaf *leaf = leaves[i];
      //LEAF_INFO("Initiate subscriptions for %s", leaf->getName().c_str());
      leaf->mqtt_do_subscribe();
    }
  }

#ifdef ESP32
  struct PubsubSendQueueMessage msg;

  while (send_queue && xQueueReceive(send_queue, &msg, 10)) {
    LEAF_WARN("Re send queued publish %s < %s", msg.topic->c_str(), msg.payload->c_str());
    _mqtt_publish(*msg.topic, *msg.payload, msg.qos, msg.retain);
    delete msg.topic;
    delete msg.payload;
  }
#endif

  publish("_pubsub_connect", pubsub_host.c_str());
  last_external_input = millis();

  LEAF_LEAVE;
}


void AbstractPubsubLeaf::loop()
{
  if (isConnected() &&
      (pubsub_broker_heartbeat_topic.length() > 0) &&
      (pubsub_broker_keepalive_sec > 0) &&
      (last_broker_heartbeat > 0)) {
    int sec_since_last_heartbeat = (millis() - last_broker_heartbeat)/1000;
    if (sec_since_last_heartbeat > pubsub_broker_keepalive_sec) {
      LEAF_ALERT("Declaring pubsub offline due to broker heartbeat timeout (%d > %d)",
		 sec_since_last_heartbeat, pubsub_broker_keepalive_sec);
      if (pubsub_log_connect) {
	char buf[80];
	snprintf(buf, sizeof(buf), "%s broker heartbeat timeout (%d > %d)",
		 getNameStr(),
		 sec_since_last_heartbeat, pubsub_broker_keepalive_sec);
	WARN("%s", buf);
	message("fs", "cmd/appendl/" PUBSUB_LOG_FILE, buf);
      }
      pubsubDisconnect(false);
      // put a reason into the broker publish queue, to be published upon reconnect
      mqtt_publish("event/broker_keepalive", String(sec_since_last_heartbeat));
    }
  }

  if (ipLeaf && ipLeaf->isConnected() && pubsub_reconnect_due) {
    LEAF_WARN("Pub-sub reconnection attempt is due");
    pubsub_reconnect_due=false;
    if (!pubsubConnect()) {
      LEAF_WARN("Reconnect attempt failed");
      if (pubsub_autoconnect) pubsubScheduleReconnect();
    }
  }

  if (!pubsub_connect_notified && pubsub_connected) {
    LEAF_NOTICE("Notifying of pubsub connection");
    pubsubOnConnect(true);
    pubsub_connect_notified = pubsub_connected;
  }
  else if (pubsub_connect_notified && !pubsub_connected) {
    LEAF_NOTICE("Notifying of pubsub disconnection");
    pubsubOnDisconnect();
    pubsub_connect_notified = pubsub_connected;
  }

  static unsigned long last_dequeue = 0;
  unsigned long now = millis();
  if (isConnected() && (now > (last_dequeue+pubsub_dequeue_delay))) {
    if (sendQueueCount()) {
      LEAF_NOTICE("Releasing one message from send queue");
      flushSendQueue(1);
    }
    last_dequeue = now;
  }

}

void pubsubReconnectTimerCallback(AbstractPubsubLeaf *leaf) { leaf->pubsubSetReconnectDue(); }

void AbstractPubsubLeaf::pubsubScheduleReconnect()
{
  LEAF_ENTER(L_INFO);

  if ((pubsub_connect_attempt_limit > 0) &&
      (pubsub_connect_attempt_count >= pubsub_connect_attempt_limit)) {
    LEAF_ALERT("Pubsub retry watchdog limit (%d>=%d) exceeded, rebooting", pubsub_connect_attempt_count, pubsub_connect_attempt_limit);
    Leaf::reboot("pubsub_retry_watchdog");
  }

  if (pubsub_reconnect_interval_sec == 0) {
    LEAF_NOTICE("Immediate retry");
    pubsubSetReconnectDue();
  }
  else {
    LEAF_NOTICE("Scheduling pubsub reconnect in %dsec", pubsub_reconnect_interval_sec);
    pubsub_reconnect_timer.once(pubsub_reconnect_interval_sec,
				&pubsubReconnectTimerCallback,
				this);
  }
  LEAF_VOID_RETURN;
}


bool AbstractPubsubLeaf::_mqtt_queue_publish(String topic, String payload, int qos, bool retain)
{
#ifdef ESP32
  if (send_queue) {
    struct PubsubSendQueueMessage msg={.topic=new String(topic), .payload=new String(payload), .qos=(byte)qos, .retain=retain};
    int free = uxQueueSpacesAvailable(send_queue);
    if (free == 0) {
      LEAF_ALERT("Send queue overflow");
      // drop the oldest message
      flushSendQueue(1);
      free++;
    }

    if (xQueueGenericSend(send_queue, (void *)&msg, (TickType_t)0, queueSEND_TO_BACK)==pdPASS) {
      LEAF_NOTICE("Queued (%d/%d): %s < %s",
		pubsub_send_queue_size-free, pubsub_send_queue_size,
		topic.c_str(), payload.c_str());
      return true;
    }
    else {
      LEAF_WARN("Send queue store failed for %s", topic.c_str());
    }
  }
  // all failures in the above block fall thru to false below
#endif
  return false;
}


bool AbstractPubsubLeaf::wants_topic(String type, String name, String topic)
{
  LEAF_ENTER_STR(L_DEBUG, topic);

  if ((pubsub_broker_heartbeat_topic.length() > 0) &&
      (topic==pubsub_broker_heartbeat_topic)) {
    LEAF_BOOL_RETURN(true);
  }
  LEAF_BOOL_RETURN(Leaf::wants_topic(type, name, topic));
}

bool AbstractPubsubLeaf::valueChangeHandler(String topic, Value *v) {
  LEAF_HANDLER(L_INFO);

  if (false) {
  }
#ifdef ESP32
  ELSEWHEN("pubsub_send_queue_size",{
    if (!send_queue) {
      LEAF_WARN("Create pubsub send queue of size %d", pubsub_send_queue_size);
      send_queue = xQueueCreate(VALUE_AS_INT(v), sizeof(struct PubsubSendQueueMessage));
    }
    else {
      LEAF_WARN("Change of queue size will take effect after reboot");
    }
  })
#endif
  else handled = Leaf::valueChangeHandler(topic, v);

  LEAF_HANDLER_END;
}


void AbstractPubsubLeaf::flushSendQueue(int count)
{
#ifdef ESP32
  struct PubsubSendQueueMessage msg;
  int n =0;

  while (send_queue && xQueueReceive(send_queue, &msg, 10)) {
    LEAF_WARN("Flush queued publish %s < %s", msg.topic->c_str(), msg.payload->c_str());
    delete msg.topic;
    delete msg.payload;
    n++;

    // a count of zero means drop all, otherwise drop the first {count} messages
    if (count && n>=count) break;
  }
#endif
}

bool AbstractPubsubLeaf::commandHandler(String type, String name, String topic, String payload) {
  LEAF_HANDLER(L_INFO);

  WHEN("reboot", {
      Leaf::reboot("cmd");
    })
#if USE_WDT
  WHEN("starve", {
      unsigned long duration = payload.toInt();
      if (duration == 0) duration = 10000;
      unsigned long now=millis();
      unsigned long until = now+duration;
      LEAF_ALERT("Starving watchdog timer for %dms", (int)duration);
      while(now <= until) {
	delay(100);
	now=millis();
      }
      LEAF_ALERT("Resuming normal operation");
    })
#endif
  ELSEWHEN("setup",{
      LEAF_ALERT("Opening IP setup mode");
      if (ipLeaf) ipLeaf->ipConfig();
      LEAF_ALERT("IP setup mode done");
  })
  ELSEWHEN("pubsub_connect", {
      LEAF_ALERT("Doing pubsub connect");
      pubsubConnect();
  })
  ELSEWHEN("pubsub_disconnect",{
      LEAF_ALERT("Doing pubsub disconnect");
      pubsubDisconnect((payload=="1"));
  })
  ELSEWHEN("pubsub_clean", {
      LEAF_ALERT("Doing MQTT clean session connect");
      this->pubsubDisconnect(true);
      bool was = pubsub_use_clean_session;
      pubsub_use_clean_session = true;
      pubsubConnect();
      pubsub_use_clean_session = was;
  })
  ELSEWHEN("pubsub_subscribe",{
      _mqtt_subscribe(payload);
  })
  ELSEWHEN("pubsub_unsubscribe",{
      if (payload == "ALL") {
	// Unsubscribe from all topics
	for (int i=0; i<pubsub_subscriptions->size(); i++) {
	  String t = pubsub_subscriptions->getKey(i);
	  _mqtt_unsubscribe(t);
	}
      }
      else {
	// single topic
	_mqtt_unsubscribe(payload);
      }
  })
  ELSEWHENEITHER("update", "update_test", {
      bool noaction = topic.endsWith("update_test");
#ifdef DEFAULT_UPDATE_URL
      if ((payload=="") || (payload.indexOf("://")<0)) {
	payload = DEFAULT_UPDATE_URL;
      }
#endif
      AbstractIpLeaf *wifi = (AbstractIpLeaf *)find("wifi","ip");
      AbstractIpLeaf *lte = (AbstractIpLeaf *)find("lte","ip");
      if (wifi && wifi->isConnected()) {
	LEAF_ALERT("Doing HTTP/wifi OTA update from %s", payload.c_str());
	wifi->ipPullUpdate(payload, noaction);  // reboots if success
	LEAF_ALERT("WiFi update failed");
      }
      else if (lte && lte->isConnected()) {
	LEAF_ALERT("Doing HTTP/lte OTA update from %s", payload.c_str());
	lte->ipPullUpdate(payload, noaction);  // reboots if success
	LEAF_ALERT("LTE update failed");
      }
      else if (ipLeaf && ipLeaf->isConnected()) {
	LEAF_ALERT("Doing HTTP update from %s", payload.c_str());
	ipLeaf->ipPullUpdate(payload, noaction);  // reboots if success
	LEAF_ALERT("HTTP update failed");
      }
    })
  ELSEWHEN("wifi_update", {
#ifdef DEFAULT_UPDATE_URL
      if ((payload=="") || (payload.indexOf("://")<0)) {
	payload = DEFAULT_UPDATE_URL;
      }
#endif
      AbstractIpLeaf *wifi = (AbstractIpLeaf *)find("wifi","ip");
      if (wifi && wifi->isConnected()) {
	LEAF_ALERT("Doing HTTP/wifi OTA update from %s", payload.c_str());
	wifi->ipPullUpdate(payload);  // reboots if success
	LEAF_ALERT("HTTP OTA update failed");
      }
      else {
	LEAF_ALERT("WiFi leaf not ready");
      }
    })
  ELSEWHENEITHER("lte_update", "lte_update_test", {
      bool lte_noaction = topic.endsWith("lte_update_test");
#ifdef DEFAULT_UPDATE_URL
      if ((payload=="") || (payload.indexOf("://")<0)) {
	payload = DEFAULT_UPDATE_URL;
      }
#endif
      AbstractIpLeaf *lte = (AbstractIpLeaf *)find("lte","ip");
      if (lte && lte->isConnected()) {
	LEAF_ALERT("Doing HTTP/lte OTA update from %s", payload.c_str());
	lte->ipPullUpdate(payload, lte_noaction);  // reboots if success
	LEAF_ALERT("HTTP OTA update failed");
      }
      else {
	LEAF_ALERT("LTE leaf not ready");
      }
    })
  ELSEWHEN("rollback", {
      LEAF_ALERT("Doing OTA rollback");
      if (ipLeaf) ipLeaf->ipRollbackUpdate(payload);  // reboots if success
      LEAF_ALERT("HTTP OTA rollback failed");
    })
#ifdef ESP32
  ELSEWHEN("bootpartition", {
      const esp_partition_t *part = esp_ota_get_boot_partition();
      mqtt_publish("status/bootpartition", part->label);
    })
  ELSEWHEN("nextpartition", {
      const esp_partition_t *part = esp_ota_get_next_update_partition(NULL);
      mqtt_publish("status/nextpartition", part->label);
    })
  ELSEWHEN("otherpartition", {
      const esp_partition_t *part = esp_ota_get_next_update_partition(NULL);
      int err = esp_ota_set_boot_partition(part);
      if (err == ESP_OK) {
	LEAF_NOTICE("Changed to partition %s", part->label);
	mqtt_publish("status/otherpartition", part->label);
      }
      else {
	LEAF_ALERT("Partition update failed (error %d)", err);
	mqtt_publish("status/otherpartition", "error");
      }
    })
#endif
  ELSEWHEN("ping", {
      //LEAF_INFO("RCVD PING %s", payload.c_str());
      mqtt_publish("status/ack", payload);
    })
#if 0
  ELSEWHEN("post", {
      //LEAF_INFO("RCVD PING %s", payload.c_str());
      pos = payload.indexOf(",");
      int code,reps;
      if (pos < 0) {
	code = payload.toInt();
	reps = 3;
      }
      else {
	code = payload.substring(0,pos).toInt();
	reps = payload.substring(pos+1).toInt();
      }
      post_error((enum post_error)code, reps);
    })
#endif
  ELSEWHEN("ip", {
      if (ipLeaf) {
	mqtt_publish("status/ip", ipLeaf->ipAddressString());
      }
      else {
	LEAF_ALERT("No IP leaf");
      }
    })
#ifndef ESP8266
  ELSEWHENAND("subscriptions",(pubsub_subscriptions != NULL), {
      //LEAF_INFO("RCVD SUBSCRIPTIONS %s", payload.c_str());
      String subs = "[\n    ";
      for (int s = 0; s < pubsub_subscriptions->size(); s++) {
	if (s) {
	  subs += ",\n    ";
	}
	subs += '"';
	subs += pubsub_subscriptions->getKey(s);
	subs += '"';
      }
      subs += "\n]";

      mqtt_publish("status/subscriptions", subs);
    })
#endif
  ELSEWHEN("format", {
      // FIXME: leafify
      //_writeConfig(true);
    })
  ELSEWHEN("leaf/list", {
      //LEAF_INFO("Leaf inventory");
      String inv = "[\n    ";
      for (int i=0; leaves[i]; i++) {
	if (i) {
	  inv += ",\n    ";
	}
	inv += '"';
	inv += leaves[i]->describe();
	inv += '"';
      }
      inv += "\n]";
      LEAF_NOTICE("Leaf inventory [%s]", inv.c_str());
      mqtt_publish("status/leaves", inv);
    })
  ELSEWHEN("leaf/status", {
      //LEAF_INFO("Leaf inventory");
      String inv = "[\n    ";
      for (int i=0; leaves[i]; i++) {
	Leaf *leaf = leaves[i];
	String stanza = "{\"leaf\":\"";
	stanza += leaf->describe();
	stanza += "\",\"comms\":\"";
	stanza += leaf->describeComms();
	//stanza += "\",\"topic\":\"";
	//stanza += leaf->getBaseTopic();
	stanza += "\",\"debug_level\":";
	stanza += String(leaf->getDebugLevel());
	stanza += ",\"status\":\"";
	if (leaf->canRun()) {
	  stanza += "RUN";
	}
	else {
	  stanza += "STOP";
	}
	stanza += "\"}";
	LEAF_NOTICE("Leaf %d status: %s", i, stanza.c_str());

	if (i) {
	  inv += ",\n    ";
	}
	inv += stanza;
      }
      inv += "\n]";
      mqtt_publish("status/leafstatus", inv);
    })
  ELSEWHEN("leaf/setup", {
      Leaf *l = get_leaf_by_name(leaves, payload);
      if (l != NULL) {
	LEAF_ALERT("Setting up leaf %s", l->describe().c_str());
	l->setup();
      }
    })
  ELSEWHENEITHER("leaf/inhibit","leaf/disable", {
      Leaf *l = get_leaf_by_name(leaves, payload);
      if (l != NULL) {
	setBoolPref(payload+"_leaf_enable", false);
      }
    })
  ELSEWHEN("leaf/enable", {
      Leaf *l = get_leaf_by_name(leaves, payload);
      if (l != NULL) {
	setBoolPref(payload+"leaf_enable", true);
      }
    })
  ELSEWHEN("leaf/start", {
      Leaf *l = get_leaf_by_name(leaves, payload);
      if (l != NULL) {
	LEAF_ALERT("Starting leaf %s", l->describe().c_str());
	l->start();
      }
    })
  ELSEWHEN("leaf/stop", {
      Leaf *l = get_leaf_by_name(leaves, payload);
      if (l != NULL) {
	LEAF_ALERT("Stopping leaf %s", l->describe().c_str());
	l->stop();
      }
    })
  ELSEWHENEITHER("leaf/mute","leaf/unmute", {
      LEAF_NOTICE("unmute topic=%s payload=%s", topic.c_str(), payload.c_str());
      Leaf *l = get_leaf_by_name(leaves, payload);
      if (l!=NULL) {
	bool m = topic.endsWith("/mute");
	LEAF_NOTICE("Setting mute for leaf %s to %s", l->describe().c_str(), ABILITY(m));
	l->setMute(m);
      }
      else {
	LEAF_WARN("Did not find leaf matching [%s]", topic.c_str());
      }
    })
  ELSEWHENPREFIX("leaf/msg/",{
      //LEAF_INFO("Finding leaf named '%s'", topic.c_str());
      Leaf *tgt = Leaf::get_leaf_by_name(leaves, topic);
      if (!tgt) {
	LEAF_ALERT("Did not find leaf named %s", topic.c_str());
      }
      else {
	String msg;
	int pos = payload.indexOf(' ');
	if (pos > 0) {
	  msg = payload.substring(0, pos);
	  payload.remove(pos+1);
	}
	else {
	  msg = payload;
	  payload = "1";
	}
	LEAF_NOTICE("Dispatching message to %s [%s] <= [%s]",
		    tgt->describe().c_str(), msg.c_str(), payload.c_str());
	StreamString result;
	enableLoopback(&result);
	tgt->mqtt_receive(getType(), getName(), msg, payload, true);
	cancelLoopback();
	LEAF_NOTICE("Message result from %s [%s] <= [%s] => [%s]",
		    tgt->describe().c_str(), msg.c_str(), payload.c_str(), result.c_str());
	mqtt_publish("result/msg/"+topic, result);
      }
    })
  ELSEWHEN("sleep", {
      LEAF_ALERT("sleep payload [%s]", payload.c_str());
      int secs = payload.toInt();
      LEAF_ALERT("Sleep for %d sec", secs);
      initiate_sleep_ms(secs * 1000);
    })
  ELSEWHEN("brownout_enable", {
      LEAF_ALERT("enable brownout detector");
      enable_bod();
    })
  ELSEWHEN("brownout_disable", {
      LEAF_ALERT("disable brownout detector");
      disable_bod();
    })
  ELSEWHEN("brownout_status", {
      bool bod_status = check_bod();
      mqtt_publish("status/brownout", ABILITY(bod_status));
    })
  ELSEWHEN("memstat", {
      char msg[132];
      int pos = 0;
#ifdef ESP32
      size_t heap_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
      size_t heap_largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
      size_t spiram_free;
      size_t spiram_largest;
      pos += snprintf(msg+pos, sizeof(msg)-pos, "{\"uptime\":%lu, ", (unsigned long)(millis()/1000));
      if (heap_free_prev != 0) {
	pos+=snprintf(msg+pos, sizeof(msg)-pos, "\"change\":%d, ",(int)heap_free-(int)heap_free_prev);
      }
      heap_free_prev = heap_free;
      pos += snprintf(msg+pos, sizeof(msg)-pos, "\"free\":%lu, \"largest\":%lu", (unsigned long)heap_free, (unsigned long)heap_largest);

      if (psramFound()) {
	spiram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
	spiram_largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
	pos += snprintf(msg+pos, sizeof(msg)-pos, ", \"spiram_free\":%lu, \"spiram_largest\":%lu", (unsigned long)spiram_free, (unsigned long)spiram_largest);
      }
      size_t stack_size = getArduinoLoopTaskStackSize();
      size_t stack_max = uxTaskGetStackHighWaterMark(NULL);
      pos += snprintf(msg+pos, sizeof(msg)-pos, ", \"stack_size\":%lu, \"stack_max\":%lu}", (unsigned long)stack_size, (unsigned long)stack_max);
      mqtt_publish("status/memory", msg);
#elif defined (ESP8266)
      uint32_t heap_free;
      uint32_t heap_largest;
      uint8_t frag;
      ESP.getHeapStats(&heap_free, &heap_largest, &frag);
      pos += snprintf(msg+pos, sizeof(msg)-pos, "{\"uptime\":%lu, ", (unsigned long)(millis()/1000));
      if (heap_free_prev != 0) {
	pos+=snprintf(msg+pos, sizeof(msg)-pos, "\"change\":%d, ",(int)heap_free_prev-(int)heap_free);
      }
      heap_free_prev = heap_free;
      pos += snprintf(msg+pos, sizeof(msg)-pos, "\"free\":%lu, \"largest\":%lu}", (unsigned long)heap_free, (unsigned long)heap_largest);
#endif
      if (pos) {
	LEAF_WARN("MEMSTAT %s", msg);
	if (payload == "log") {
	  message("fs", "cmd/appendl/" STACX_LOG_FILE, String("memstat ")+msg);
	}
	mqtt_publish("status/memory", msg);
      }
    })
#ifdef ESP32
  ELSEWHEN("pubsub_sendq_flush", flushSendQueue())
  ELSEWHEN("pubsub_sendq_stat", {
      mqtt_publish("status/pubsub_send_queue_size", String(pubsub_send_queue_size));
      int free = 0;
      if (send_queue) {
	free = uxQueueSpacesAvailable(send_queue);
      }
      mqtt_publish("status/pubsub_send_queue_free", String(free));
    })
#endif //ESP32
  else handled = Leaf::commandHandler(type, name, topic, payload);

  LEAF_HANDLER_END;
}

void AbstractPubsubLeaf::_mqtt_route(String Topic, String Payload, int flags)
{
  LEAF_ENTER(L_DEBUG);
  LEAF_NOTICE("AbstractPubsubLeaf ROUTE %s <= %s %s", this->describe().c_str(), Topic.c_str(), Payload.c_str());

  bool handled = false;
  bool isShell = flags&PUBSUB_SHELL;

  do {
    int pos, lastPos;
    String device_type;
    String device_name;
    String device_target;
    String device_topic;

    // Parse the device address from the topic.
    // When the shell is used to inject fake messages (pubsub_loopback) we do not do this
    if (pubsubUseDeviceTopic() && !isShell) {
      LEAF_INFO("Parsing device topic...");
      if (Topic.startsWith(_ROOT_TOPIC+"devices/")) {
	lastPos = _ROOT_TOPIC.length()+strlen("devices/");
      }
      else {
	// the topic does not begin with "devices/"
	LEAF_WARN("Cannot find device header in topic %s", Topic.c_str());
	// it might be an external topic subscribed to by a leaf
	break;
      }

      pos = Topic.indexOf('/', lastPos);
      if (pos < 0) {
	LEAF_ALERT("Cannot find device id in topic %s", Topic.c_str());
	break;
      }

      device_target = Topic.substring(lastPos, pos);
      LEAF_INFO("Parsed device ID [%s] from topic at %d:%d", device_target.c_str(), lastPos, pos);
      lastPos = pos+1;

      pos = Topic.indexOf('/', lastPos);
      if (pos < 0) {
	LEAF_ALERT("Cannot find device type in topic %s", Topic.c_str());
	break;
      }
      device_type = Topic.substring(lastPos, pos);

      // special case devices/foo/{cmd,get,set}/# is shorthand for devices/foo/*/*/cmd etc
      if ((device_type=="cmd") || (device_type=="get") || (device_type=="set")) {
	device_type="*";
	device_name="*";
	device_topic = Topic.substring(lastPos);
	LEAF_INFO("Special case global cmd/get/set device_topic<=[%s]", device_topic);
      }
      else {


	LEAF_INFO("Parsed device type [%s] from topic at %d:%d", device_type.c_str(), lastPos, pos)
	lastPos = pos+1;

	pos = Topic.indexOf('/', lastPos);
	if (pos < 0) {
	  LEAF_ALERT("Cannot find device name in topic %s", Topic.c_str());
	  break;
	}

	device_name = Topic.substring(lastPos, pos);
	LEAF_INFO("Parsed device name [%s] from topic at %d:%d", device_name.c_str(), lastPos, pos);

	device_topic = Topic.substring(pos+1);
	LEAF_INFO("Parsed device topic [%s] from topic", device_topic.c_str());
      }
    }
    else { // !pubsub_use_device_topic
      LEAF_INFO("Parsing non-device topic...");
      // we are using simplified topics that do not address devices by type+name
      device_type="*";
      device_name="*";
      device_target="*";
      device_topic = Topic;
      if (device_topic.startsWith(base_topic)) {
	LEAF_DEBUG("Snip base topic [%s] from [%s]", base_topic.c_str(), device_topic.c_str());
	device_topic.remove(0, base_topic.length());
      }
      if (device_topic.startsWith("/")) {
	// we must have an empty app_topic
	LEAF_DEBUG("Snip empty app topic [/] from [%s]", device_topic.c_str());
	device_topic.remove(0, 1);
      }

      if (hasPriority() && !device_topic.startsWith("_")) {
	device_topic.remove(0, device_topic.indexOf('/')+1);
	LEAF_DEBUG("Snip priority from device_topic => [%s]", device_topic.c_str());
	if (device_topic.startsWith("read-request/")) {
	  device_topic.replace("read-request/", "get/");
	  LEAF_DEBUG("Transform read-request to get => [%s]", device_topic.c_str());
	}
	if (device_topic.startsWith("write-request/")) {
	  device_topic.replace("write-request/", "set/");
	  LEAF_DEBUG("Transform write-request to set => [%s]", device_topic.c_str());
	}
      }
      if (pubsub_use_flat_topic) {
	device_topic.replace("-", "/");
	LEAF_DEBUG("Transform to flat topic => [%s]", device_topic.c_str());
      }
    }

    LEAF_INFO("Topic parse device_name=%s device_type=%s device_target=%s device_id=%s device_topic=%s",
    device_name.c_str(), device_type.c_str(), device_target.c_str(), device_id, device_topic.c_str());
    if ( ((device_type=="*") || (device_type == "backplane")) &&
	 ((device_target == "*") || (device_target == device_id))
      )
    {
      LEAF_INFO("Testing backplane patterns with device_type=%s device_target=%s device_id=%s device_topic=%s",
      device_type.c_str(), device_target.c_str(), device_id, device_topic.c_str());
      handled = this->mqtt_receive(device_type, device_target, Topic, Payload, false);
      if (handled) {
	LEAF_DEBUG("Topic %s was handed as a backplane topic", device_topic.c_str());
      }
    }

    if (!handled) {

      for (int i=0; leaves[i]; i++) {
	Leaf *leaf = leaves[i];
	if (leaf->canRun() && leaf->wants_topic(device_type, device_name, device_topic)
	  ) {
	  bool h = leaf->mqtt_receive(device_type, device_name, device_topic, Payload);
	  if (h) {
	    handled = true;
	  }
	}
      }
    }

  } while (false); // the while loop is only to allow use of 'break' as an escape

  if (!handled) {
    // Leaves can also subscribe to raw topics, so try that
    for (int i=0; leaves[i]; i++) {
      Leaf *leaf = leaves[i];
      if (leaf->canRun() && leaf->wants_raw_topic(Topic)) {
	handled |= leaf->mqtt_receive_raw(Topic, Payload);
      }
    }
  }

  if (!handled) {
    LEAF_ALERT("Nobody handled topic %s", Topic.c_str());
  }

  LEAF_LEAVE_SLOW(1000);
}

bool AbstractPubsubLeaf::mqtt_receive(String type, String name, String topic, String payload, bool direct)
{
  LEAF_ENTER(L_DEBUG);

  LEAF_INFO("AbstractPubsubLeaf RECV (%s %s) %s <= %s", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());

  bool handled = false;

  if (!setup_done) {
    // When in "rescue mode", the leaf may not have been initialised thus
    // have not registered its commands.   We detect this case and auto-init.
    //
    LEAF_WARN("Auto-registering leaf %s for rescue mode", getNameStr());
    setup();
  }

  WHENAND(pubsub_broker_heartbeat_topic, (pubsub_broker_heartbeat_topic.length() > 0), {
      // received a broker heartbeat
      LEAF_NOTICE("Received broker heartbeat: %s", payload.c_str());
      last_broker_heartbeat = millis();
      if (ipLeaf->getTimeSource()==AbstractIpLeaf::TIME_SOURCE_NONE) {
	struct timeval tv;
	struct timezone tz;
	tz.tz_minuteswest = -timeZone*60+minutesTimeZone;
	tz.tz_dsttime=0;
	tv.tv_sec = strtoull(payload.c_str(), NULL, 10);
	tv.tv_usec = 0;
	settimeofday(&tv, &tz);
	ipLeaf->setTimeSource(AbstractIpLeaf::TIME_SOURCE_BROKER);
	LEAF_WARN("Set time from broker %s", ctime(&tv.tv_sec));
      }


    })
  ELSEWHEN("status/time_source",{
      // time has changed, retry any scheduled connection
      if (pubsub_reconnect_timer.active()) {
	LEAF_NOTICE("Time source changed, trigger reconnect");
	pubsub_reconnect_timer.detach();
	pubsubSetReconnectDue();
      }
  })
#ifdef BUILD_NUMBER
  ELSEWHEN("get/build", {
      mqtt_publish("status/build", String(BUILD_NUMBER,10));
  })
#endif
  ELSEWHEN("get/uptime", {
    mqtt_publish("status/uptime", String(millis()/1000));
  })
  ELSEWHEN("get/mac", {
      mqtt_publish("status/mac", mac);
  })
  ELSEWHEN("set/device_id", {
      LEAF_NOTICE("Updating device ID [%s]", payload);
      if (payload.length() > 0) {
	strlcpy(device_id, payload.c_str(), sizeof(device_id));
	setPref("device_id", payload);
      }
  })
  ELSEWHEN("set/pubsub_autoconect", {
    pubsub_autoconnect = parseBool(payload, pubsub_autoconnect);
    setBoolPref("pubsub_autoconnect", pubsub_autoconnect);
  })
  ELSEWHEN("set/pubsub_host", {
    if (payload.length() > 0) {
      pubsub_host=payload;
      setPref("pubsub_host", pubsub_host);
    }
  })
  ELSEWHEN("set/pubsub_port", {
    int p = payload.toInt();
    if ((p > 0) && (p<65536)) {
      pubsub_port=p;
      setIntPref("pubsub_port", p);
    }
  })
  ELSEWHEN("set/ts", {
    struct timeval tv;
    struct timezone tz;
    tv.tv_sec = strtoull(payload.c_str(), NULL, 10);
    tv.tv_usec = 0;
    tz.tz_minuteswest = 0;
    tz.tz_dsttime = 0;
    settimeofday(&tv, &tz);
  })
  ELSEWHEN("get/debug_level", {
    mqtt_publish("status/debug_level", String(debug_level, DEC));
  })
  ELSEWHEN("set/debug_level", {
    LEAF_NOTICE("Set DEBUG");
    if (payload == "more") {
      debug_level++;
    }
    else if (payload == "less" && (debug_level > 0)) {
      debug_level--;
    }
    else {
      debug_level = payload.toInt();
    }
    mqtt_publish("status/debug_level", String(debug_level, DEC));
  })
  ELSEWHEN("get/debug_wait", {
    mqtt_publish("status/debug_wait", String(debug_wait, DEC));
  })
  ELSEWHEN("set/debug_wait", {
    LEAF_NOTICE("Set debug_wait");
    debug_wait = payload.toInt();
    mqtt_publish("status/debug_wait", String(debug_wait, DEC));
  })
  ELSEWHEN("get/debug_lines", {
    mqtt_publish("status/debug_lines", TRUTH(debug_lines));
  })
  ELSEWHEN("set/debug_lines", {
    LEAF_NOTICE("Set debug_lines");
    bool lines = false;
    if (payload == "on") lines=true;
    else if (payload == "true") lines=true;
    else if (payload == "lines") lines=true;
    else if (payload == "1") lines=true;
    debug_lines = lines;
    mqtt_publish("status/debug_lines", TRUTH(debug_lines));
  })
  ELSEWHEN("get/debug_flush", {
    mqtt_publish("status/debug_flush", TRUTH(debug_flush));
  })
  ELSEWHEN("set/debug_flush", {
    LEAF_NOTICE("Set debug_flush");
    bool flush = false;
    if (payload == "on") flush=true;
    else if (payload == "true") flush=true;
    else if (payload == "flush") flush=true;
    else if (payload == "1") flush=true;
    debug_flush = flush;
    mqtt_publish("status/debug_flush", TRUTH(debug_flush));
  })
  else {
    handled = Leaf::mqtt_receive(type, name, topic, payload);
    if (!handled) {
      LEAF_DEBUG("No handler for backplane %s topic [%s]", device_id, topic);
    }
  }

  LEAF_BOOL_RETURN(handled);
}


#endif
// Local Variables:
// mode: C++
// c-basic-offset: 2
// End:
