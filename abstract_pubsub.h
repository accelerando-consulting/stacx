//@**************************** class AbstractPubsub ******************************
//
// This class encapsulates a publish-subscribe mechanism such as ESP32's EspAsyncMQTT,
// the MQTT AT-command interface provided by a cellular modem, or LoRAWAN.
//
#ifndef _LEAF_PUBSUB_ABSTRACT
#define _LEAF_PUBSUB_ABSTRACT

#include "abstract_storage.h"

bool mqttConfigured = false;
bool mqttConnected = false;
bool mqttLoopback = false;

#define PUBSUB_LOOPBACK 1

class AbstractPubsubLeaf : public Leaf
{
public:

  AbstractPubsubLeaf(String name, String target="", bool use_ssl = false, bool use_device_topic=true) : Leaf("pubsub", name) {
    do_heartbeat = false;
    this->tap_targets = target;
    this->pubsub_use_ssl = use_ssl;
    this->pubsub_use_device_topic = use_device_topic ;
  }

  virtual void setup();
  virtual void start();
  virtual void pubsubScheduleReconnect();
  virtual bool isConnected() { return pubsub_connected; }
  virtual bool isAutoConnect() { return pubsub_autoconnect; }
  void pubsubSetReconnectDue() {pubsub_reconnect_due=true;};
  virtual void pubsubOnConnect(bool do_subscribe=true){
    pubsub_connected=true;
    pubsub_connect_time=millis();
    ++pubsub_connect_count;
  }
  virtual void pubsubOnDisconnect(){pubsub_connected=false;pubsub_disconnect_time=millis();}
  bool pubsubUseDeviceTopic(){return pubsub_use_device_topic;}

  virtual bool pubsubConnect(void){return false;}
  virtual void pubsubDisconnect(bool deliberate=true){if (!deliberate && pubsub_autoconnect) pubsubScheduleReconnect();};
  virtual uint16_t _mqtt_publish(String topic, String payload, int qos=0, bool retain=false)=0;
  virtual void _mqtt_subscribe(String topic, int qos=0)=0;
  virtual void _mqtt_unsubscribe(String topic)=0;

  virtual void _mqtt_receive(String topic, String payload, int flags = 0);
  virtual void initiate_sleep_ms(int ms)=0;
  String getLoopbackBuffer() { return pubsub_loopback_buffer; }



  // deprecated methods
  virtual bool connect(void) {LEAF_ALERT("connect method is deprecated");return pubsubConnect();}
  virtual void disconnect(bool deliberate=true){LEAF_ALERT("disconnect method is deprecated");pubsubDisconnect(deliberate);}

protected:
  String pubsub_host="";
  int pubsub_port = 1883;
  String pubsub_user="";
  String pubsub_pass="";
  String pubsub_lwt_topic="";
  int pubsub_keepalive_sec = 120;

  bool pubsub_use_device_topic = true;
  bool pubsub_autoconnect = true;
  bool pubsub_connected = false;
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

  bool pubsub_use_ssl_client_cert = false;
  bool pubsub_loopback = false;
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
  run = getBoolPref("pubsub_enable", run);

  pubsub_subscriptions = new SimpleMap<String,int>(_compareStringKeys);

  // TODO: these should go somewhere else I think.  maybe the abstract app leaf?
  blink_enable = getIntPref("blink_enable", 1);
  debug_level = getIntPref("debug_level", DEBUG_LEVEL);

  use_get = pubsub_use_get = getBoolPref("use_get", use_get);
  use_set = pubsub_use_set = getBoolPref("use_set", use_set);
  use_cmd = pubsub_use_cmd = getBoolPref("use_cmd", use_cmd);
  use_flat_topic = pubsub_use_flat_topic = getBoolPref("use_flat_topic", use_flat_topic);
  use_wildcard_topic = pubsub_use_wildcard_topic = getBoolPref("use_wildcard_topic", use_wildcard_topic);
  use_status = pubsub_use_status = pubsub_use_status = getBoolPref("use_status", use_status);
  use_event = pubsub_use_event = getBoolPref("use_event", use_event);
  pubsub_use_ssl = getBoolPref("use_ssl", pubsub_use_ssl);
  pubsub_use_ssl_client_cert = getBoolPref("use_cert", pubsub_use_ssl_client_cert);
  pubsub_use_clean_session = getBoolPref("use_clean", pubsub_use_clean_session);

  pubsub_host = getPref("pubsub_host", pubsub_host);
  pubsub_port = getIntPref("pubsub_port", pubsub_port);
  pubsub_user = getPref("pubsub_user", pubsub_user);
  pubsub_pass = getPref("pubsub_pass", pubsub_pass);

  LEAF_LEAVE;
}

void AbstractPubsubLeaf::start()
{
  Leaf::start();

  if (ipLeaf) {
    if (pubsub_autoconnect && ipLeaf->isConnected()) {
      LEAF_NOTICE("Connecting");
      pubsubConnect();
    }
    else {
      LEAF_NOTICE("Network layer not connected");
    }
  }
  else {
    LEAF_NOTICE("IP leaf not found");
  }
}

void pubsubReconnectTimerCallback(AbstractPubsubLeaf *leaf) { leaf->pubsubSetReconnectDue(); }

void AbstractPubsubLeaf::pubsubScheduleReconnect() 
{
  if (pubsub_reconnect_interval_sec == 0) {
    pubsubSetReconnectDue();
  }
  else {
    pubsub_reconnect_timer.once(pubsub_reconnect_interval_sec,
				&pubsubReconnectTimerCallback,
				this);
  }
}

void AbstractPubsubLeaf::_mqtt_receive(String Topic, String Payload, int flags)
{
  LEAF_ENTER(L_INFO);
  const char *topic = Topic.c_str();

  LEAF_INFO("AbstractPubsubLeaf RECV %s %s", Topic.c_str(), Payload.c_str());

  bool handled = false;

  pubsub_autoconnect = getBoolPref("pubsub_autoconnect", pubsub_autoconnect);
  
  if (flags & PUBSUB_LOOPBACK) {
    pubsub_loopback = mqttLoopback = true;
    pubsub_loopback_buffer = "";
  }

  do {
    int pos, lastPos;
    String device_type;
    String device_name;
    String device_target;
    String device_topic;

    // Parse the device address from the topic.
    // When the shell is used to inject fake messages (pubsub_loopback) we do not do this
    if (pubsub_use_device_topic && !pubsub_loopback) {
      //LEAF_DEBUG("Parsing device topic...");
      if (Topic.startsWith(_ROOT_TOPIC+"devices/")) {
	lastPos = _ROOT_TOPIC.length()+strlen("devices/");
      }
      else {
	// the topic does not begin with "devices/"
	LEAF_INFO("Cannot find device header in topic %s", topic);
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
	device_topic.remove(0, base_topic.length());
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
	this->pubsubDisconnect(false);
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
      else if (device_topic == "cmd/status") {
	LEAF_INFO("RCVD STATUS %s", Payload.c_str());
	mqtt_publish("status/ip", ip_addr_str);
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
	if (pubsub_loopback) {
	  LEAF_NOTICE("leaf/list\n%s",inv.c_str());
	}
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
	if (pubsub_loopback) {
	  LEAF_NOTICE("leaf/status\n%s",inv.c_str());
	}
	mqtt_publish("status/leafstatus", inv);
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
#if 0
      else if (device_topic == "set/name") {
	LEAF_NOTICE("Updating device ID [%s]", payload);
	// FIXME: leafify
	//_readConfig();
	if (Payload.length() > 0) {
	  strlcpy(device_id, Payload.c_str(), sizeof(device_id));
	}
	// FIXME: leafify
	//_writeConfig();
	LEAF_ALERT("Rebooting for new config");
	delay(2000);
	reboot();
      }
#endif
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
	mqtt_publish("status/debug_wait", String(DBGWAIT, DEC));
      }
      else if (device_topic == "set/debug_wait") {
	LEAF_NOTICE("Set DBGWAIT");
	DBGWAIT = Payload.toInt();
	mqtt_publish("status/debug_wait", String(DBGWAIT, DEC));
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
	if (leaf->wants_topic(device_type, device_name, device_topic)) {
	  //LEAF_DEBUG("Invoke leaf receiver %s <= %s", leaf->describe().c_str(), device_topic.c_str());
	  bool h = leaf->mqtt_receive(device_type, device_name, device_topic, Payload);
	  if (h) {
	    //LEAF_DEBUG("Leaf %s handled this topic", leaf->describe().c_str());
	    handled = true;
	  }
	}
      }
      if (!handled) {
	LEAF_DEBUG("No leaf handled topic [%s]", topic);
      }
    }
    
  } while (false); // the while loop is only to allow use of 'break' as an escape

  if (!handled) {
    LEAF_INFO("Trying raw subscriptions");
    // Leaves can also subscribe to raw topics, so try that
    for (int i=0; leaves[i]; i++) {
      Leaf *leaf = leaves[i];
      if (leaf->wants_raw_topic(Topic)) {
	handled |= leaf->mqtt_receive_raw(Topic, Payload);
      }
    }
  }

  if (flags & PUBSUB_LOOPBACK) {
    mqttLoopback = pubsub_loopback = false;
  }
  if (!handled) {
    LEAF_ALERT("Nobody handled topic %s", topic);
  }

  LEAF_LEAVE;
}


#endif
// Local Variables:
// mode: C++
// c-basic-offset: 2
// End:
