//@**************************** class AbstractPubsub ******************************
//
// This class encapsulates a publish-subscribe mechanism such as ESP32's EspAsyncMQTT,
// the MQTT AT-command interface provided by a cellular modem, or LoRAWAN.
//
#ifndef _LEAF_PUBSUB_ABSTRACT
#define _LEAF_PUBSUB_ABSTRACT

#include "abstract_storage.h"

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
#define PUBSUB_BROKER_KEEPALIVE_SEC 330
#endif

#define PUBSUB_SSL_ENABLE true
#define PUBSUB_SSL_DISABLE false
#define PUBSUB_DEVICE_TOPIC_ENABLE true
#define PUBSUB_DEVICE_TOPIC_DISABLE false


class AbstractPubsubLeaf : public Leaf
{
public:

  AbstractPubsubLeaf(String name, String target="", bool use_ssl = false, bool use_device_topic=true)
    : Leaf("pubsub", name)
    , TraitDebuggable(name)
  {
    do_heartbeat = false;
    this->tap_targets = target;
    this->pubsub_use_ssl = use_ssl;
    this->pubsub_use_device_topic = use_device_topic ;
    this->impersonate_backplane = true;
  }

  virtual void setup();
  virtual void start();
  virtual void loop();
  virtual void pubsubScheduleReconnect();
  virtual bool isConnected() { return pubsub_connected; }
  virtual void pubsubSetConnected(bool state=true) {
    LEAF_NOTICE("pubsubSetConnected %s", TRUTH_lc(state)); 
    pubsub_connected=state;
  }
  virtual bool isAutoConnect() { return pubsub_autoconnect; }
  void pubsubSetReconnectDue() {pubsub_reconnect_due=true;};
  virtual void mqtt_do_subscribe() {
    Leaf::mqtt_do_subscribe();
    register_mqtt_cmd("restart", "reboot this device");
    register_mqtt_cmd("reboot", "reboot this device");
    register_mqtt_cmd("setup", "enter wifi setup mode");
    register_mqtt_cmd("pubsub_connect", "initiate (re-) connection to pubsub broker");
    register_mqtt_cmd("pubsub_disconnect", "close any connection to pubsub broker");
    register_mqtt_cmd("pubsub_clean", "disconnect and reestablish a clean session to pubsub broker");
    register_mqtt_cmd("update", "Perform a firmware update from the payload URL");
    register_mqtt_cmd("wifi_update", "Perform a firmware update from the payload URL, using wifi only");
    register_mqtt_cmd("lte_update", "Perform a firmware update from the payload URL, using LTE only");
    register_mqtt_cmd("rollback", "Roll back the last firmware update");
    register_mqtt_cmd("bootpartition", "Publish the currently active boot partition");
    register_mqtt_cmd("nextpartition", "Switch to the next available boot partition");
    register_mqtt_cmd("ping", "Return the supplied payload to status/ack");
    register_mqtt_cmd("post", "Flash a power-on-self-test blink code");
    register_mqtt_cmd("ip", "Publish current ip address to status/ip");
    register_mqtt_cmd("subscriptions", "Publish the currently subscribed topics");
    register_mqtt_cmd("leaf/list", "List active stacx leaves");
    register_mqtt_cmd("leaf/status", "List status of active stacx leaves");
    register_mqtt_cmd("leaf/setup", "Run the setup method of the named leaf");
    register_mqtt_cmd("leaf/inhibit", "Disable the named leaf");
    register_mqtt_cmd("leaf/disable", "Disable the named leaf");
    register_mqtt_cmd("leaf/enable", "Enable the named leaf");
    register_mqtt_cmd("leaf/start", "Start the named leaf");
    register_mqtt_cmd("leaf/stop", "Stop the named leaf");
    register_mqtt_cmd("sleep", "Enter lower power mode (optional value in seconds)");
  }
    
  virtual void pubsubOnConnect(bool do_subscribe=true){
    LEAF_ENTER_BOOL(L_INFO, do_subscribe);
    pubsubSetConnected(true);
    pubsub_connecting = false;
    ++pubsub_connect_count;
    ipLeaf->ipCommsState(ONLINE, HERE);
    ACTION("PUBSUB conn");
    publish("_pubsub_connect",String(1));

    register_mqtt_cmd("help", "publish help information (a subset if payload='pref' or 'cmd')");

    if (hasPriority() && (getPriority()=="service")) {
      LEAF_NOTICE("Service priority leaf does not enable leaf subscriptions");
      return;
    }

    if (do_subscribe) {
      LEAF_INFO("Set up leaf subscriptions");
      for (int i=0; leaves[i]; i++) {
	Leaf *leaf = leaves[i];
	LEAF_INFO("Initiate subscriptions for %s", leaf->describe().c_str());
	leaf->mqtt_do_subscribe();
      }
      LEAF_LEAVE;
    }
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
    ACTION("PUBSUB disc");
    for (int i=0; leaves[i]; i++) {
      if (leaves[i]->canRun()) {
	leaves[i]->mqtt_disconnect();
      }
    }
    LEAF_VOID_RETURN;
  }
  bool pubsubUseDeviceTopic(){return pubsub_use_device_topic;}

  virtual bool pubsubConnect(void){
    LEAF_ENTER(L_INFO);
    ACTION("PUBSUB try");
    ipLeaf->ipCommsState(TRY_PUBSUB,HERE);//signal attempt in progress
    pubsub_connecting = true;
    LEAF_BOOL_RETURN(true);
  }
  virtual void pubsubDisconnect(bool deliberate=true){
    LEAF_ENTER_BOOL(L_INFO, deliberate);
    if (!deliberate && pubsub_autoconnect) pubsubScheduleReconnect();
    LEAF_VOID_RETURN;
  };
  virtual uint16_t _mqtt_publish(String topic, String payload, int qos=0, bool retain=false)=0;
  virtual void _mqtt_subscribe(String topic, int qos=0, codepoint_t where=undisclosed_location)=0;

  virtual void _mqtt_unsubscribe(String topic)=0;

  virtual void _mqtt_receive(String topic, String payload, int flags = 0);
  virtual void initiate_sleep_ms(int ms);
  virtual String getLoopbackBuffer() { return pubsub_loopback_buffer; }
  virtual void clearLoopbackBuffer()   { pubsub_loopback_buffer = ""; }
  virtual void enableLoopback() { LEAF_INFO("enableLoopback"); pubsub_loopback = ::pubsub_loopback = true; clearLoopbackBuffer(); }
  virtual void cancelLoopback() { LEAF_INFO("disableLoopback"); pubsub_loopback = ::pubsub_loopback = false; }
  virtual void storeLoopback(String topic, String payload) { pubsub_loopback_buffer += topic + ' ' + payload + '\n'; }
  virtual bool isLoopback() { return pubsub_loopback; }
  virtual void pubsubSetSessionPresent(bool p) { pubsub_session_present = p; };
  
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
  int pubsub_broker_keepalive_sec = PUBSUB_BROKER_KEEPALIVE_SEC;
  unsigned long last_broker_heartbeat = 0;

  int pubsub_keepalive_sec = 120;

  bool pubsub_use_device_topic = true;
  bool pubsub_autoconnect = true;
  bool pubsub_ip_autoconnect = true;
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
  bool pubsub_onconnect_wake = true;
  bool pubsub_onconnect_time = false;
  
  bool pubsub_use_ssl_client_cert = false;
  bool pubsub_loopback = false;
  int pubsub_connect_timeout_ms = 10000;
  int pubsub_connect_count = 0;
  uint32_t pubsub_connect_time = 0;
  uint32_t pubsub_disconnect_time = 0;
  int pubsub_reconnect_interval_sec = PUBSUB_RECONNECT_SECONDS;
  Ticker pubsub_reconnect_timer;
  bool pubsub_reconnect_due = false;
  SimpleMap<String,int> *pubsub_subscriptions = NULL;
  String pubsub_loopback_buffer;
};

void AbstractPubsubLeaf::setup(void)
{
  Leaf::setup();
  LEAF_ENTER(L_INFO);
  run = getBoolPref("pubsub_enable", run, "Enable pub-sub client");

#ifndef ESP8266
  pubsub_subscriptions = new SimpleMap<String,int>(_compareStringKeys);
#endif

  use_get = pubsub_use_get = getBoolPref("pubsub_use_get", use_get, "Subscribe to get topics");
  use_set = pubsub_use_set = getBoolPref("pubsub_use_set", use_set, "Subscribe to set topics");
  use_cmd = pubsub_use_cmd = getBoolPref("pubsub_use_cmd", use_cmd, "Subscribe to command topics");
  use_flat_topic = pubsub_use_flat_topic = getBoolPref("pubsub_use_flat_topic", use_flat_topic, "Use verb-noun not verb/noun in topics");
  use_wildcard_topic = pubsub_use_wildcard_topic = getBoolPref("pubsub_use_wildcard_topic", use_wildcard_topic, "Subscribe using wildcards");
  use_status = pubsub_use_status = pubsub_use_status = getBoolPref("pubsub_use_status", use_status, "Publish status messages");
  use_event = pubsub_use_event = getBoolPref("pubsub_use_event", use_event, "Publish event messages");
  pubsub_use_ssl = getBoolPref("pubsub_use_ssl", pubsub_use_ssl, "Use SSP for publish");
  pubsub_use_ssl_client_cert = getBoolPref("pubsub_use_ssl_client_cert", pubsub_use_ssl_client_cert, "Use a client certificate for SSL");
  pubsub_use_clean_session = getBoolPref("pubsub_use_clean_session", pubsub_use_clean_session, "Enable MQTT Clean Session");

  registerValue(HERE, "pubsub_onconnect_ip", VALUE_KIND_BOOL, &pubsub_onconnect_ip, "Publish device's IP address upon connection");
  registerValue(HERE, "pubsub_onconnect_wake", VALUE_KIND_BOOL, &pubsub_onconnect_wake, "Publish device's wake reason upon connection");
  

  getPref("pubsub_host", &pubsub_host, "Host to which publish-subscribe client connects");
  getIntPref("pubsub_port", &pubsub_port, "Port to which publish-subscribe client connects");
  getPref("pubsub_user", &pubsub_user, "Pub-sub server username");
  getPref("pubsub_pass", &pubsub_pass, "Pub-sub server password");
  getIntPref("pubsub_keepalive_sec", &pubsub_keepalive_sec, "Keepalive value for MQTT");
  getIntPref("pubsub_connect_timeout_ms", &pubsub_connect_timeout_ms, "MQTT connect timeout in milliseconds");
  getBoolPref("pubsub_autoconnect", &pubsub_autoconnect, "Automatically connect to pub-sub when IP connects");
  getBoolPref("pubsub_reuse_connection", &pubsub_reuse_connection, "If pubsub is already connected at boot time, re-use connection");
  getBoolPref("pubsub_ip_autoconnect", &pubsub_ip_autoconnect, "Automatically connect IP layer when needing to publish");
  getBoolPref("pubsub_warn_noconn", &pubsub_warn_noconn, "Log a warning if unable to publish due to no connection");


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

void AbstractPubsubLeaf::initiate_sleep_ms(int ms)
{
  LEAF_NOTICE("Prepare for deep sleep");

  mqtt_publish("event/sleep",String(millis()/1000));

  // Apply sleep in reverse order, highest level leaf first
  int leaf_index;
  for (leaf_index=0; leaves[leaf_index]; leaf_index++);
  for (leaf_index--; leaf_index<=0; leaf_index--) {
    LEAF_NOTICE("Call pre_sleep for leaf %d: %s", leaf_index, leaves[leaf_index]->describe().c_str());
    leaves[leaf_index]->pre_sleep(ms/1000);
  }

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
#ifndef ARDUINO_ESP32C3_DEV
  esp_sleep_enable_ext0_wakeup((gpio_num_t)0, 0);
#endif

  esp_deep_sleep_start();
#else
  //FIXME sleep not implemented on esp8266
#endif
}


void AbstractPubsubLeaf::loop() 
{
  if (pubsub_reconnect_due) {
    LEAF_NOTICE("Pub-sub reconnection attempt is due");
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
}

void pubsubReconnectTimerCallback(AbstractPubsubLeaf *leaf) { leaf->pubsubSetReconnectDue(); }

void AbstractPubsubLeaf::pubsubScheduleReconnect() 
{
  LEAF_ENTER(L_INFO);
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

void AbstractPubsubLeaf::_mqtt_receive(String Topic, String Payload, int flags)
{
  LEAF_ENTER(L_DEBUG);

  LEAF_INFO("AbstractPubsubLeaf RECV %s <= %s %s", this->describe().c_str(), Topic.c_str(), Payload.c_str());

  bool handled = false;
  bool isShell = false;
  
  if (flags & PUBSUB_SHELL) {
    isShell = true;
    LEAF_INFO("Shell command [%s] <= [%s]", Topic.c_str(), Payload.c_str());
    if (flags & PUBSUB_LOOPBACK) {
      enableLoopback();
    }
  }

  do {
    int pos, lastPos;
    String device_type;
    String device_name;
    String device_target;
    String device_topic;

    // Parse the device address from the topic.
    // When the shell is used to inject fake messages (pubsub_loopback) we do not do this
    if (pubsub_use_device_topic && !isShell) {
      //LEAF_DEBUG("Parsing device topic...");
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
      //LEAF_DEBUG("Parsed device ID [%s] from topic at %d:%d", device_target.c_str(), lastPos, pos)      lastPos = pos+1;
;
      pos = Topic.indexOf('/', lastPos);
      if (pos < 0) {
	LEAF_ALERT("Cannot find device type in topic %s", Topic.c_str());
	break;
      }
      device_type = Topic.substring(lastPos, pos);
      //LEAF_DEBUG("Parsed device type [%s] from topic at %d:%d", device_type.c_str(), lastPos, pos)
      lastPos = pos+1;

      pos = Topic.indexOf('/', lastPos);
      if (pos < 0) {
	LEAF_ALERT("Cannot find device name in topic %s", Topic.c_str());
	break;
      }

      device_name = Topic.substring(lastPos, pos);
      //LEAF_DEBUG("Parsed device name [%s] from topic at %d:%d", device_name.c_str(), lastPos, pos);

      device_topic = Topic.substring(pos+1);
      //LEAF_DEBUG("Parsed device topic [%s] from topic", device_topic.c_str());
    }
    else { // !pubsub_use_device_topic
      // we are using simplified topics that do not address devices by type+name
      device_type="*";
      device_name="*";
      device_target="*";
      device_topic = Topic;
      if (device_topic.startsWith(base_topic)) {
	LEAF_INFO("Snip base topic [%s] from [%s]", base_topic.c_str(), device_topic.c_str());
	device_topic.remove(0, base_topic.length());
      }
      if (device_topic.startsWith("/")) {
	// we must have an empty app_topic
	device_topic.remove(0, 1);
      }

      if (hasPriority() && !device_topic.startsWith("_")) {
	device_topic.remove(0, device_topic.indexOf('/')+1);
        LEAF_INFO("Snip priority from device_topic => [%s]",
                  device_topic.c_str());
      }
      if (pubsub_use_flat_topic) {
	device_topic.replace("-", "/");
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

      String topic = device_topic;
      
      handled=true; // ultimate else of this if-chain will set false if reached   

      WHENEITHER("cmd/restart", "cmd/reboot",{
        for (int i=0; leaves[i]; i++) {
          leaves[i]->pre_reboot();
	}
	reboot();
      })
      ELSEWHEN("cmd/setup",{
        LEAF_ALERT("Opening IP setup mode");
	if (ipLeaf) ipLeaf->ipConfig();
	  LEAF_ALERT("IP setup mode done");
      })
      ELSEWHEN("cmd/pubsub_connect", {
        LEAF_ALERT("Doing pubsub connect");
        pubsubConnect();
      })
      ELSEWHEN("cmd/pubsub_disconnect",{
        LEAF_ALERT("Doing pubsub disconnect");
        pubsubDisconnect((Payload=="1"));
      })
      ELSEWHEN("cmd/pubsub_clean", {
        LEAF_ALERT("Doing MQTT clean session connect");
        this->pubsubDisconnect(true);
        bool was = pubsub_use_clean_session;
        pubsub_use_clean_session = true;
        pubsubConnect();
        pubsub_use_clean_session = was;
      })
#ifdef BUILD_NUMBER
      ELSEWHEN("get/build", {
        mqtt_publish("status/build", String(BUILD_NUMBER,10));
      })
#endif
      ELSEWHEN("get/uptime", {
        mqtt_publish("status/uptime", String(millis()/1000));
      })
      ELSEWHEN("cmd/update", {
#ifdef DEFAULT_UPDATE_URL
        if ((Payload=="") || (Payload.indexOf("://")<0)) {
          Payload = DEFAULT_UPDATE_URL;
        }
#endif
        LEAF_ALERT("Doing HTTP OTA update from %s", Payload.c_str());
        if (ipLeaf) ipLeaf->ipPullUpdate(Payload);  // reboots if success
        LEAF_ALERT("HTTP OTA update failed");
      })
      ELSEWHEN("cmd/wifi_update", {
#ifdef DEFAULT_UPDATE_URL
        if ((Payload=="") || (Payload.indexOf("://")<0)) {
          Payload = DEFAULT_UPDATE_URL;
        }
#endif
        AbstractIpLeaf *wifi = (AbstractIpLeaf *)find("wifi","ip");
        if (wifi && wifi->isConnected()) {
          LEAF_ALERT("Doing HTTP/wifi OTA update from %s", Payload.c_str());
          wifi->ipPullUpdate(Payload);  // reboots if success
          LEAF_ALERT("HTTP OTA update failed");
        }
        else {
          LEAF_ALERT("WiFi leaf not ready");
        }
        
      })
      ELSEWHEN("cmd/lte_update", {
#ifdef DEFAULT_UPDATE_URL
        if ((Payload=="") || (Payload.indexOf("://")<0)) {
          Payload = DEFAULT_UPDATE_URL;
        }
#endif
        AbstractIpLeaf *lte = (AbstractIpLeaf *)find("lte","ip");
        if (lte && lte->isConnected()) {
          LEAF_ALERT("Doing HTTP/lte OTA update from %s", Payload.c_str());
          lte->ipPullUpdate(Payload);  // reboots if success
          LEAF_ALERT("HTTP OTA update failed");
        }
        else {
          LEAF_ALERT("LTE leaf not ready");
        }
      })
      ELSEWHEN("cmd/rollback", {
        LEAF_ALERT("Doing OTA rollback");
        if (ipLeaf) ipLeaf->ipRollbackUpdate(Payload);  // reboots if success
        LEAF_ALERT("HTTP OTA rollback failed");
      })
#ifdef ESP32
      ELSEWHEN("cmd/bootpartition", {
        const esp_partition_t *part = esp_ota_get_boot_partition();
        mqtt_publish("status/bootpartition", part->label);
      })
      ELSEWHEN("cmd/nextpartition", {
        const esp_partition_t *part = esp_ota_get_next_update_partition(NULL);
        mqtt_publish("status/nextpartition", part->label);
      })
#endif
      ELSEWHEN("cmd/ping", {
        LEAF_INFO("RCVD PING %s", Payload.c_str());
        mqtt_publish("status/ack", Payload);
      })
#if 0
      ELSEWHEN("cmd/post", {
        LEAF_INFO("RCVD PING %s", Payload.c_str());
        pos = Payload.indexOf(",");
        int code,reps;
        if (pos < 0) {
          code = Payload.toInt();
          reps = 3;
        }
        else {
          code = Payload.substring(0,pos).toInt();
          reps = Payload.substring(pos+1).toInt();
        }
        post_error((enum post_error)code, reps);
      })
#endif
      ELSEWHEN("cmd/ip", {
        if (ipLeaf) {  
          mqtt_publish("status/ip", ipLeaf->ipAddressString());
        }
        else {
          LEAF_ALERT("No IP leaf");
        }
      })
      ELSEWHENAND("cmd/subscriptions",(pubsub_subscriptions != NULL), {
        LEAF_INFO("RCVD SUBSCRIPTIONS %s", Payload.c_str());
        String subs = "[\n    ";
        for (int s = 0; s < pubsub_subscriptions->size(); s++) {
          if (s) {
            subs += ",\n    ";
          }
          subs += '"';
          subs += pubsub_subscriptions->getKey(s);
          subs += '"';
          LEAF_DEBUG("Subscriptions [%s]", subs.c_str());
        }
        subs += "\n]";

        mqtt_publish("status/subscriptions", subs);
      })
      ELSEWHEN("cmd/format", {
        // FIXME: leafify
        //_writeConfig(true);
      })
      ELSEWHEN("cmd/leaf/list", {
        LEAF_INFO("Leaf inventory");
        String inv = "[\n    ";
        for (int i=0; leaves[i]; i++) {
          if (i) {
            inv += ",\n    ";
          }
          inv += '"';
          inv += leaves[i]->describe();
          inv += '"';
          LEAF_DEBUG("Leaf inventory [%s]", inv.c_str());
        }
        inv += "\n]";
        mqtt_publish("status/leaves", inv);
      })
      ELSEWHEN("cmd/leaf/status", {
        LEAF_INFO("Leaf inventory");
        String inv = "[\n    ";
        for (int i=0; leaves[i]; i++) {
          Leaf *leaf = leaves[i];
          String stanza = "{\"leaf\":\"";
          stanza += leaf->describe();
          stanza += "\",\"comms\":\"";
          stanza += leaf->describeComms();
          stanza += "\",\"status\":\"";
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
      ELSEWHEN("cmd/leaf/setup", {
        Leaf *l = get_leaf_by_name(leaves, Payload);
        if (l != NULL) {
          LEAF_ALERT("Setting up leaf %s", l->describe().c_str());
          l->setup();
        }
      })
      ELSEWHENEITHER("cmd/leaf/inhibit","cmd/leaf/disable", {
        Leaf *l = get_leaf_by_name(leaves, Payload);
        if (l != NULL) {
          setBoolPref(String("leaf_enable_")+Payload, false);
        }
      })
      ELSEWHEN("cmd/leaf/enable", {
        Leaf *l = get_leaf_by_name(leaves, Payload);
        if (l != NULL) {
          setBoolPref(String("leaf_enable_")+Payload, true);
        }
      })
      ELSEWHEN("cmd/leaf/start", {
        Leaf *l = get_leaf_by_name(leaves, Payload);
        if (l != NULL) {
          LEAF_ALERT("Starting leaf %s", l->describe().c_str());
          l->start();
        }
      })
      ELSEWHEN("cmd/leaf/stop", {
        Leaf *l = get_leaf_by_name(leaves, Payload);
        if (l != NULL) {
          LEAF_ALERT("Stopping leaf %s", l->describe().c_str());
          l->stop();
        }
      })
#ifdef ESP32
      ELSEWHEN("cmd/memstat", {
        size_t heap_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        size_t heap_largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
        size_t spiram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        size_t spiram_largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
        mqtt_publish("status/heap_free", String(heap_free));
        mqtt_publish("status/heap_largest", String(heap_largest));
        mqtt_publish("status/spiram_free", String(spiram_free));
        mqtt_publish("status/spiram_largest", String(spiram_largest));
      })
#endif
      ELSEWHEN("cmd/sleep", {
        LEAF_ALERT("cmd/sleep payload [%s]", Payload.c_str());
        int secs = Payload.toInt();
        LEAF_ALERT("Sleep for %d sec", secs);
        initiate_sleep_ms(secs * 1000);
      })
      ELSEWHEN("set/device_id", {
        LEAF_NOTICE("Updating device ID [%s]", Payload);
        if (Payload.length() > 0) {
          strlcpy(device_id, Payload.c_str(), sizeof(device_id));
          setPref("device_id", Payload);
        }
      })
      ELSEWHEN("set/pubsub_autoconect", {
        pubsub_autoconnect = parseBool(Payload, pubsub_autoconnect);
        setBoolPref("pubsub_autoconnect", pubsub_autoconnect);
      })
      ELSEWHEN("set/pubsub_host", {
        if (Payload.length() > 0) {
          pubsub_host=Payload;
          setPref("pubsub_host", pubsub_host);
        }
      })
      ELSEWHEN("set/pubsub_port", {
        int p = Payload.toInt();
        if ((p > 0) && (p<65536)) {
          pubsub_port=p;
          setIntPref("pubsub_port", p);
        }
      })
      ELSEWHEN("set/ts", {
        struct timeval tv;
        struct timezone tz;
        tv.tv_sec = strtoull(Payload.c_str(), NULL, 10);
        tv.tv_usec = 0;
        tz.tz_minuteswest = 0;
        tz.tz_dsttime = 0;
        settimeofday(&tv, &tz);
      })
      ELSEWHEN("set/blink_enable", {
        LEAF_NOTICE("Set blink_enable");
        blink_enable = Payload.toInt();
        mqtt_publish("status/blink_enable", String(blink_enable, DEC));
        setPref("blink_enable", String((int)blink_enable));
      })
      ELSEWHEN("get/debug_level", {
        mqtt_publish("status/debug_level", String(debug_level, DEC));
      })
      ELSEWHEN("set/debug_level", {
        LEAF_NOTICE("Set DEBUG");
        if (Payload == "more") {
          debug_level++;
        }
        else if (Payload == "less" && (debug_level > 0)) {
          debug_level--;
        }
        else {
          debug_level = Payload.toInt();
        }
        mqtt_publish("status/debug_level", String(debug_level, DEC));
      })
      ELSEWHEN("get/debug_wait", {
        mqtt_publish("status/debug_wait", String(debug_wait, DEC));
      })
      ELSEWHEN("set/debug_wait", {
        LEAF_NOTICE("Set debug_wait");
        debug_wait = Payload.toInt();
        mqtt_publish("status/debug_wait", String(debug_wait, DEC));
      })
      ELSEWHEN("get/debug_lines", {
        mqtt_publish("status/debug_lines", TRUTH(debug_lines));
      })
      ELSEWHEN("set/debug_lines", {
        LEAF_NOTICE("Set debug_lines");
        bool lines = false;
        if (Payload == "on") lines=true;
        else if (Payload == "true") lines=true;
        else if (Payload == "lines") lines=true;
        else if (Payload == "1") lines=true;
        debug_lines = lines;
        mqtt_publish("status/debug_lines", TRUTH(debug_lines));
      })
      ELSEWHEN("get/debug_flush", {
        mqtt_publish("status/debug_flush", TRUTH(debug_flush));
      })
      ELSEWHEN("set/debug_flush", {
        LEAF_NOTICE("Set debug_flush");
        bool flush = false;
        if (Payload == "on") flush=true;
        else if (Payload == "true") flush=true;
        else if (Payload == "flush") flush=true;
        else if (Payload == "1") flush=true;
        debug_flush = flush;
        mqtt_publish("status/debug_flush", TRUTH(debug_flush));
      })
      else {
        if (pubsub_use_device_topic) {
          LEAF_DEBUG("No handler for backplane %s topic [%s]", device_id, topic);
        }
        handled=false;
      }
      
      // restore the saved full topic after inspecting a reduced device topic
    }

    if (!handled) {
      for (int i=0; leaves[i]; i++) {
	Leaf *leaf = leaves[i];
	//LEAF_INFO("Asking %s if it wants %s",leaf->describe().c_str(), device_topic.c_str());
	if (leaf->canRun() && leaf->wants_topic(device_type, device_name, device_topic)) {
	  LEAF_INFO("Invoke leaf receiver %s <= %s", leaf->describe().c_str(), device_topic.c_str());
	  bool h = leaf->mqtt_receive(device_type, device_name, device_topic, Payload);
	  if (h) {
	    LEAF_DEBUG("Leaf %s handled this topic", leaf->describe().c_str());
	    handled = true;
	  }
	}
      }
      if (!handled) {
	LEAF_INFO("No leaf handled topic [%s]", Topic.c_str());
      }
    }
    
  } while (false); // the while loop is only to allow use of 'break' as an escape

  if (!handled) {
    LEAF_INFO("Trying raw subscriptions");
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
  if (flags & PUBSUB_LOOPBACK) {
    cancelLoopback();
  }

  LEAF_LEAVE;
}


#endif
// Local Variables:
// mode: C++
// c-basic-offset: 2
// End:
