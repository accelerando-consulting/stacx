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

#define PUBSUB_SSL_ENABLE true
#define PUBSUB_SSL_DISABLE false
#define PUBSUB_DEVICE_TOPIC_ENABLE true
#define PUBSUB_DEVICE_TOPIC_DISABLE false


class AbstractPubsubLeaf : public Leaf
{
public:

  AbstractPubsubLeaf(String name, String target="", bool use_ssl = false, bool use_device_topic=true)
    : Leaf("pubsub", name)
  {
    do_heartbeat = false;
    this->tap_targets = target;
    this->pubsub_use_ssl = use_ssl;
    this->pubsub_use_device_topic = use_device_topic ;
    this->impersonate_backplane = true;
    this->pubsubLeaf = this;
  }

  virtual void setup();
  virtual void start();
  virtual void pubsubScheduleReconnect();
  virtual bool isConnected() { return pubsub_connected; }
  virtual void pubsubSetConnected(bool state=true) {
    LEAF_NOTICE("pubsubSetConnected %s", TRUTH_lc(state)); 
    pubsub_connected=state;
  }
  virtual bool isAutoConnect() { return pubsub_autoconnect; }
  void pubsubSetReconnectDue() {pubsub_reconnect_due=true;};
  virtual void pubsubOnConnect(bool do_subscribe=true){
    LEAF_ENTER(L_NOTICE);
    pubsubSetConnected(true);
    pubsub_connecting = false;
    ++pubsub_connect_count;
    idle_state(ONLINE, HERE);
    ACTION("PUBSUB conn");
    publish("_pubsub_connect",String(1));

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
    LEAF_ENTER(L_NOTICE);
    idle_state(WAIT_PUBSUB, HERE);
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
    ACTION("PUBSUB try");
    idle_state(TRY_PUBSUB,HERE);//signal attempt in progress
    pubsub_connecting = true;
    return true;
  }
  virtual void pubsubDisconnect(bool deliberate=true){
    LEAF_ENTER_BOOL(L_NOTICE, deliberate);
    if (!deliberate && pubsub_autoconnect) pubsubScheduleReconnect();
    LEAF_VOID_RETURN;
  };
  virtual uint16_t _mqtt_publish(String topic, String payload, int qos=0, bool retain=false)=0;
  virtual void _mqtt_subscribe(String topic, int qos=0)=0;

  virtual void _mqtt_unsubscribe(String topic)=0;

  virtual void _mqtt_receive(String topic, String payload, int flags = 0);
  virtual void initiate_sleep_ms(int ms)=0;
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
  int pubsub_keepalive_sec = 120;

  bool pubsub_use_device_topic = true;
  bool pubsub_autoconnect = true;
  bool pubsub_ip_autoconnect = true;
  bool pubsub_connected = false;
  bool pubsub_connecting = false;
  bool pubsub_session_present = false;
  bool pubsub_use_clean_session = false;
  bool pubsub_use_ssl = false;
  bool pubsub_use_status = USE_STATUS;
  bool pubsub_use_event = USE_EVENT;
  bool pubsub_use_set = USE_SET;
  bool pubsub_use_get = USE_GET;
  bool pubsub_use_cmd = USE_CMD;
  bool pubsub_use_flat_topic = USE_FLAT_TOPIC;
  bool pubsub_use_wildcard_topic = USE_WILDCARD_TOPIC;
  bool pubsub_warn_noconn = false;
  
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
  LEAF_ENTER(L_INFO);

  Leaf::setup();
  run = getBoolPref("pubsub_enable", run, "Enable pub-sub client");

  pubsub_subscriptions = new SimpleMap<String,int>(_compareStringKeys);

  // TODO: these should go somewhere else I think.  maybe the abstract app leaf?
  blink_enable = getIntPref("blink_enable", 1, "Enable the device identification blink");
  debug_level = getIntPref("debug_level", DEBUG_LEVEL, "Debug trace level (0=ALERT,1=WARN,2=NOTICE,3=INFO,4=DEBUG");

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

  getPref("pubsub_host", &pubsub_host, "Host to which publish-subscribe client connects");
  getIntPref("pubsub_port", &pubsub_port, "Port to which publish-subscribe client connects");
  getPref("pubsub_user", &pubsub_user, "Pub-sub server username");
  getPref("pubsub_pass", &pubsub_pass, "Pub-sub server password");
  getIntPref("pubsub_keepalive_sec", &pubsub_keepalive_sec, "Keepalive value for MQTT");
  getIntPref("pubsub_connect_timeout_ms", &pubsub_connect_timeout_ms, "MQTT connect timeout in milliseconds");
  getBoolPref("pubsub_autoconnect", &pubsub_autoconnect, "Automatically connect to pub-sub when IP connects");
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
      LEAF_NOTICE("Connecting");
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

void pubsubReconnectTimerCallback(AbstractPubsubLeaf *leaf) { leaf->pubsubSetReconnectDue(); }

void AbstractPubsubLeaf::pubsubScheduleReconnect() 
{
  LEAF_ENTER(L_NOTICE);
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
  const char *topic = Topic.c_str();

  LEAF_NOTICE("AbstractPubsubLeaf RECV %s %s", Topic.c_str(), Payload.c_str());

  bool handled = false;
  bool isShell = false;
  
  if (flags & PUBSUB_SHELL) {
    isShell = true;
    LEAF_INFO("Shell command [%s] <= [%s]", topic, Payload.c_str());
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
	LEAF_WARN("Cannot find device header in topic %s", topic);
	// it might be an external topic subscribed to by a leaf
	break;
      }

      pos = Topic.indexOf('/', lastPos);
      if (pos < 0) {
	LEAF_ALERT("Cannot find device id in topic %s", topic);
	break;
      }
      device_target = Topic.substring(lastPos, pos);
      //LEAF_DEBUG("Parsed device ID [%s] from topic at %d:%d", device_target.c_str(), lastPos, pos)      lastPos = pos+1;
;
      pos = Topic.indexOf('/', lastPos);
      if (pos < 0) {
	LEAF_ALERT("Cannot find device type in topic %s", topic);
	break;
      }
      device_type = Topic.substring(lastPos, pos);
      //LEAF_DEBUG("Parsed device type [%s] from topic at %d:%d", device_type.c_str(), lastPos, pos)
      lastPos = pos+1;

      pos = Topic.indexOf('/', lastPos);
      if (pos < 0) {
	LEAF_ALERT("Cannot find device name in topic %s", topic);
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

      if (hasPriority()) {
	device_topic.remove(0, device_topic.indexOf('/')+1);
      }
      if (pubsub_use_flat_topic) {
	device_topic.replace("-", "/");
      }
    }

    LEAF_NOTICE("Topic parse device_name=%s device_type=%s device_target=%s device_id=%s device_topic=%s",
	      device_name.c_str(), device_type.c_str(), device_target.c_str(), device_id, device_topic.c_str());
    if ( ((device_type=="*") || (device_type == "backplane")) &&
	 ((device_target == "*") || (device_target == device_id))
      )
    {
      LEAF_INFO("Testing backplane patterns with device_type=%s device_target=%s device_id=%s device_topic=%s",
		 device_type.c_str(), device_target.c_str(), device_id, device_topic.c_str());

      handled=true; // ultimate else of this if-chain will set false if reached
      if ((device_topic == "cmd/restart") || (device_topic == "cmd/reboot")) {
	for (int i=0; leaves[i]; i++) {
	  leaves[i]->pre_reboot();
	}
	reboot();
      }
      else if (device_topic == "cmd/setup") {
	LEAF_ALERT("Opening IP setup mode");
	if (ipLeaf) ipLeaf->ipConfig();
	LEAF_ALERT("IP setup mode done");
      }
      else if (device_topic == "cmd/pubsub_connect") {
	LEAF_ALERT("Doing pubsub connect");
	this->pubsubConnect();
      }
      else if (device_topic == "cmd/pubsub_disconnect") {
	LEAF_ALERT("Doing pubsub disconnect");
	this->pubsubDisconnect((Payload=="1"));
      }
      else if (device_topic == "cmd/pubsub_clean") {
	LEAF_ALERT("Doing MQTT clean session connect");
	this->pubsubDisconnect(true);
	bool was = pubsub_use_clean_session;
	pubsub_use_clean_session = true;
	this->pubsubConnect();
	pubsub_use_clean_session = was;
      }

#ifdef BUILD_NUMBER
      else if (device_topic == "get/build") {
	mqtt_publish("status/build", String(BUILD_NUMBER,10));
      }
#endif
      else if (device_topic == "get/uptime") {
	mqtt_publish("status/uptime", String(millis()/1000));
      }
      else if (device_topic == "cmd/update") {
#ifdef DEFAULT_UPDATE_URL
	if ((Payload=="") || (Payload.indexOf("://")<0)) {
	  Payload = DEFAULT_UPDATE_URL;
	}
#endif
	LEAF_ALERT("Doing HTTP OTA update from %s", Payload.c_str());
	if (ipLeaf) ipLeaf->pullUpdate(Payload);  // reboots if success
	LEAF_ALERT("HTTP OTA update failed");
      }
      else if (device_topic == "cmd/rollback") {
	LEAF_ALERT("Doing OTA rollback");
	if (ipLeaf) ipLeaf->rollbackUpdate(Payload);  // reboots if success
	LEAF_ALERT("HTTP OTA rollback failed");
      }
#ifdef ESP32
      else if (device_topic == "cmd/bootpartition") {
	const esp_partition_t *part = esp_ota_get_boot_partition();
	mqtt_publish("status/bootpartition", part->label);
      }
      else if (device_topic == "cmd/nextpartition") {
	const esp_partition_t *part = esp_ota_get_next_update_partition(NULL);
	mqtt_publish("status/nextpartition", part->label);
      }
#endif
      else if (device_topic == "cmd/ping") {
	LEAF_INFO("RCVD PING %s", Payload.c_str());
	mqtt_publish("status/ack", Payload);
      }
      else if (device_topic == "cmd/post") {
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
      }
      else if (device_topic == "cmd/ip") {
	if (ipLeaf) {  
	  mqtt_publish("status/ip", ipLeaf->ipAddressString());
	}
	else {
	  LEAF_ALERT("No IP leaf");
	}
      }
      else if (device_topic == "cmd/subscriptions") {
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
      }
      else if (device_topic == "cmd/format") {
	// FIXME: leafify
	//_writeConfig(true);
      }
      else if (device_topic == "cmd/leaf/list") {
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
      }
      else if (device_topic == "cmd/leaf/status") {
	LEAF_INFO("Leaf inventory");
	String inv = "[\n    ";
	for (int i=0; leaves[i]; i++) {
	  Leaf *leaf = leaves[i];
	  if (i) {
	    inv += ",\n    ";
	  }
	  inv += "{\"leaf\":\"";
	  inv += leaf->describe();
	  inv += "\",\"status\":\"";
	  if (leaf->canRun()) {
	    inv += "RUN";
	  }
	  else {
	    inv += "STOP";
	  }
	  inv += "\"}";
	}
	inv += "\n]";
	mqtt_publish("status/leafstatus", inv);
      }
      else if (device_topic == "cmd/leaf/setup") {
	Leaf *l = get_leaf_by_name(leaves, Payload);
	if (l != NULL) {
	  LEAF_ALERT("Setting up leaf %s", l->describe().c_str());
	  l->setup();
	}
      }
      else if (device_topic == "cmd/leaf/start") {
	Leaf *l = get_leaf_by_name(leaves, Payload);
	if (l != NULL) {
	  LEAF_ALERT("Starting leaf %s", l->describe().c_str());
	  l->start();
	}
      }
      else if (device_topic == "cmd/leaf/stop") {
	Leaf *l = get_leaf_by_name(leaves, Payload);
	if (l != NULL) {
	  LEAF_ALERT("Stopping leaf %s", l->describe().c_str());
	  l->stop();
	}
      }
      else if (device_topic == "cmd/sleep") {
	LEAF_ALERT("cmd/sleep payload %s", Payload.c_str());
	int secs = Payload.toInt();
	LEAF_ALERT("Sleep for %d sec", secs);
	initiate_sleep_ms(secs * 1000);
      }
      else if (device_topic == "set/device_id") {
	LEAF_NOTICE("Updating device ID [%s]", Payload);
	if (Payload.length() > 0) {
	  strlcpy(device_id, Payload.c_str(), sizeof(device_id));
	  setPref("device_id", Payload);
	}
      }
      else if (device_topic == "set/pubsub_host") {
	if (Payload.length() > 0) {
	  pubsub_host=Payload;
	  setPref("pubsub_host", pubsub_host);
	}
      }
      else if (device_topic == "set/pubsub_port") {
	int p = Payload.toInt();
	if ((p > 0) && (p<65536)) {
	  pubsub_port=p;
	  setIntPref("pubsub_port", p);
	}
      }
      else if (device_topic == "set/ts") {
	struct timeval tv;
	struct timezone tz;
	tv.tv_sec = strtoull(Payload.c_str(), NULL, 10);
	tv.tv_usec = 0;
	tz.tz_minuteswest = 0;
	tz.tz_dsttime = 0;
	settimeofday(&tv, &tz);
      }
      else if (device_topic == "set/blink_enable") {
	LEAF_NOTICE("Set blink_enable");
	blink_enable = Payload.toInt();
	mqtt_publish("status/blink_enable", String(blink_enable, DEC));
	setPref("blink_enable", String((int)blink_enable));
      }
      else if (device_topic == "get/debug_level") {
	mqtt_publish("status/debug_level", String(debug_level, DEC));
      }
      else if (device_topic == "set/debug_level") {
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
      }
      else if (device_topic == "get/debug_wait") {
	mqtt_publish("status/debug_wait", String(debug_wait, DEC));
      }
      else if (device_topic == "set/debug_wait") {
	LEAF_NOTICE("Set debug_wait");
	debug_wait = Payload.toInt();
	mqtt_publish("status/debug_wait", String(debug_wait, DEC));
      }
      else if (device_topic == "get/debug_lines") {
	mqtt_publish("status/debug_lines", TRUTH(debug_lines));
      }
      else if (device_topic == "set/debug_lines") {
	LEAF_NOTICE("Set debug_lines");
	bool lines = false;
	if (Payload == "on") lines=true;
	else if (Payload == "true") lines=true;
	else if (Payload == "lines") lines=true;
	else if (Payload == "1") lines=true;
	debug_lines = lines;
	mqtt_publish("status/debug_lines", TRUTH(debug_lines));
      }
      else if (device_topic == "get/debug_flush") {
	mqtt_publish("status/debug_flush", TRUTH(debug_flush));
      }
      else if (device_topic == "set/debug_flush") {
	LEAF_NOTICE("Set debug_flush");
	bool flush = false;
	if (Payload == "on") flush=true;
	else if (Payload == "true") flush=true;
	else if (Payload == "flush") flush=true;
	else if (Payload == "1") flush=true;
	debug_flush = flush;
	mqtt_publish("status/debug_flush", TRUTH(debug_flush));
      }
      else {
	if (pubsub_use_device_topic) {
	  LEAF_DEBUG("No handler for backplane %s topic [%s]", device_id, topic);
	}
	handled=false;
      }
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
	LEAF_INFO("No leaf handled topic [%s]", topic);
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
    LEAF_ALERT("Nobody handled topic %s", topic);
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
