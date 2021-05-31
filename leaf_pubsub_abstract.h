//@**************************** class AbstractPubsub ******************************
//
// This class encapsulates a publish-subscribe mechanism such as ESP32's EspAsyncMQTT,
// the MQTT AT-command interface provided by a cellular modem, or LoRAWAN.
//
#ifndef _LEAF_PUBSUB_ABSTRACT
#define _LEAF_PUBSUB_ABSTRACT


bool mqttConfigured = false;
bool mqttConnected = false;
bool mqttLoopback = false;

#define PUBSUB_LOOPBACK 1

class AbstractPubsubLeaf : public Leaf
{
public:

  AbstractPubsubLeaf(String name, String target="", bool use_ssl = false, bool use_device_topic=true) : Leaf("pubsub", name) {
    do_heartbeat = false;
    this->use_ssl = use_ssl;
    this->target = target;
    this->use_device_topic = use_device_topic ;
  }

  virtual void setup(void) = 0;
  virtual void loop(void) {
    Leaf::loop();
  }

  virtual void start()
  {
    Leaf::start();

    if (ipLeaf) {
      if (autoconnect && ipLeaf->isConnected()) {
	LEAF_NOTICE("Connecting");
	connect();
      }
      else {
	LEAF_NOTICE("Network layer not connected");
      }
    }
    else {
      LEAF_NOTICE("IP leaf not found");
    }
  }

  virtual bool connect(void){return false;}

  virtual void disconnect(bool deliberate=true){};

  virtual uint16_t _mqtt_publish(String topic, String payload, int qos=0, bool retain=false)=0;
  virtual void _mqtt_subscribe(String topic, int qos=0)=0;
  virtual void _mqtt_unsubscribe(String topic)=0;

  virtual void _mqtt_receive(String topic, String payload, int flags = 0);
  virtual void initiate_sleep_ms(int ms)=0;
  virtual bool isConnected() { return _connected; }
  String getLoopbackBuffer() { return loopback_buffer; }


  bool use_device_topic = true;

protected:
  String target;
  bool autoconnect = true;
  bool _connected = false;
  bool sessionPresent = false;
  bool use_ssl;
  Ticker mqttReconnectTimer;
  SimpleMap<String,int> *mqttSubscriptions = NULL;
  String loopback_buffer;
};

void AbstractPubsubLeaf::setup(void)
{
  LEAF_ENTER(L_NOTICE);

  Leaf::setup();

  this->install_taps(target);
  mqttConnected = false;
  mqttSubscriptions = new SimpleMap<String,int>(_compareStringKeys);

  LEAF_LEAVE;
}



void AbstractPubsubLeaf::_mqtt_receive(String Topic, String Payload, int flags)
{
  ENTER(L_INFO);
  const char *topic = Topic.c_str();

  LEAF_INFO("RECV %s %s", Topic.c_str(), Payload.c_str());

  bool handled = false;

  if (flags & PUBSUB_LOOPBACK) {
    mqttLoopback = true;
    loopback_buffer = "";
  }

  while (1) {
    int pos, lastPos;
    String device_type;
    String device_name;
    String device_topic;

    if (use_device_topic) {
      if (Topic.startsWith(_ROOT_TOPIC+"devices/")) {
	lastPos = _ROOT_TOPIC.length()+strlen("devices/");
      }
      else {
	// the topic does not begin with "devices/"
	DEBUG("Cannot find device header in topic %s", topic);
	// it might be an external topic subscribed to by a leaf
	break;
      }

      pos = Topic.indexOf('/', lastPos);
      if (pos < 0) {
	ALERT("Cannot find device type in topic %s", topic);
	break;
      }
      String device_type = Topic.substring(lastPos, pos);
      LEAF_DEBUG("Parsed device type [%s] from topic at %d:%d", device_type.c_str(), lastPos, pos);

      lastPos = pos+1;
      pos = Topic.indexOf('/', lastPos);
      if (pos < 0) {
	ALERT("Cannot find device name in topic %s", topic);
	break;
      }

      String device_name = Topic.substring(lastPos, pos);
      LEAF_DEBUG("Parsed device name [%s] from topic at %d:%d", device_name.c_str(), lastPos, pos);

      String device_topic = Topic.substring(pos+1);
      LEAF_DEBUG("Parsed device topic [%s] from topic", device_topic.c_str());
    }
    else { // !use_device_topic
      // we are using simplified topics that do not address devices by type+name
      device_type="*";
      device_name="*";
      device_topic = Topic;
      if (device_topic.startsWith(base_topic)) {
	device_topic.remove(0, base_topic.length());
      }

      if (use_flat_topic) {
	device_topic.replace("-", "/");
      }
    }

    if ( ((device_type=="*") || (device_type == "backplane")) &&
	 ((device_name == "*") || (device_name == device_id))
      )
    {
      if ((device_topic == "cmd/restart") || (device_topic == "cmd/reboot")) {
	for (int i=0; leaves[i]; i++) {
	  leaves[i]->pre_reboot();
	}
	reboot();
      }
      else if (device_topic == "cmd/setup") {
	ALERT("Opening WIFI setup portal FIXME NOT IMPLEMENTED");
	//FIXME: leafify
	//_wifiMgr_setup(true);
	ALERT("WIFI setup portal done");
      }
      else if (device_topic == "cmd/pubsub_connect") {
	ALERT("Doing MQTT connect");
	this->connect();
      }
      else if (device_topic == "cmd/pubsub_disconnect") {
	ALERT("Doing MQTT disconnect");
	this->disconnect(true);
      }
      else if (device_topic == "get/build") {
	mqtt_publish("status/build", String(BUILD_NUMBER,10));
      }
      else if (device_topic == "get/uptime") {
	mqtt_publish("status/uptime", String(millis()/1000));
      }
      else if (device_topic == "cmd/update") {
#ifdef DEFAULT_UPDATE_URL
	if ((Payload=="") || (Payload.indexOf("://")<0)) {
	  Payload = DEFAULT_UPDATE_URL;
	}
#endif
	ALERT("Doing HTTP OTA update from %s", Payload.c_str());
	if (ipLeaf) ipLeaf->pull_update(Payload);  // reboots if success
	ALERT("HTTP OTA update failed");
      }
      else if (device_topic == "cmd/rollback") {
	ALERT("Doing HTTP OTA update from %s", Payload.c_str());
	if (ipLeaf) ipLeaf->rollback_update(Payload);  // reboots if success
	ALERT("HTTP OTA rollback failed");
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
	INFO("RCVD PING %s", Payload.c_str());
	mqtt_publish("status/ack", Payload);
      }
      else if (device_topic == "get/build") {
	mqtt_publish("status/build", String(BUILD_NUMBER));
      }
      else if (device_topic == "cmd/status") {
	INFO("RCVD STATUS %s", Payload.c_str());
	mqtt_publish("status/ip", ip_addr_str);
      }
      else if (device_topic == "cmd/subscriptions") {
	INFO("RCVD SUBSCRIPTIONS %s", Payload.c_str());
	String subs = "[\n    ";
	for (int s = 0; s < mqttSubscriptions->size(); s++) {
	  if (s) {
	    subs += ",\n    ";
	  }
	  subs += '"';
	  subs += mqttSubscriptions->getKey(s);
	  subs += '"';
	  DEBUG("Subscriptions [%s]", subs.c_str());
	}
	subs += "\n]";
	mqtt_publish("status/subscriptions", subs);
      }
      else if (device_topic == "cmd/format") {
	// FIXME: leafify
	//_writeConfig(true);
      }
      else if (device_topic == "cmd/leaf/list") {
	INFO("Leaf inventory");
	String inv = "[\n    ";
	for (int i=0; leaves[i]; i++) {
	  if (i) {
	    inv += ",\n    ";
	  }
	  inv += '"';
	  inv += leaves[i]->describe();
	  inv += '"';
	  DEBUG("Leaf inventory [%s]", inv.c_str());
	}
	inv += "\n]";
	mqtt_publish("status/leaves", inv);

      }
      else if (device_topic == "cmd/leaf/status") {
	INFO("Leaf inventory");
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
      else if (device_topic == "cmd/leaf/start") {
	Leaf *l = get_leaf_by_name(leaves, Payload);
	if (l != NULL) {
	  ALERT("Starting leaf %s", l->describe().c_str());
	  l->start();
	}
      }
      else if (device_topic == "cmd/leaf/stop") {
	Leaf *l = get_leaf_by_name(leaves, Payload);
	if (l != NULL) {
	  ALERT("Stopping leaf %s", l->describe().c_str());
	  l->stop();
	}
      }
      else if (device_topic == "cmd/sleep") {
	ALERT("cmd/sleep payload %s", Payload.c_str());
	int secs = Payload.toInt();
	ALERT("Sleep for %d sec", secs);
	initiate_sleep_ms(secs * 1000);
      }
#if 0
      else if (device_topic == "set/name") {
	NOTICE("Updating device ID [%s]", payload);
	// FIXME: leafify
	//_readConfig();
	if (Payload.length() > 0) {
	  strlcpy(device_id, Payload.c_str(), sizeof(device_id));
	}
	// FIXME: leafify
	//_writeConfig();
	ALERT("Rebooting for new config");
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
      else if (device_topic == "set/debug") {
	NOTICE("Set DEBUG");
	if (Payload == "more") {
	  debug++;
	}
	else if (Payload == "less" && (debug > 0)) {
	  debug--;
	}
	else {
	  debug = Payload.toInt();
	}
	mqtt_publish("status/debug", String(debug, DEC));
      }
      else if (device_topic == "set/debug_wait") {
	NOTICE("Set DBGWAIT");
	DBGWAIT = Payload.toInt();
	mqtt_publish("status/debug_wait", String(debug, DEC));
      }
      else if (device_topic == "set/debug_lines") {
	NOTICE("Set debug_lines");
	bool lines = false;
	if (Payload == "on") lines=true;
	else if (Payload == "true") lines=true;
	else if (Payload == "lines") lines=true;
	else if (Payload == "1") lines=true;
	debug_lines = lines;
	mqtt_publish("status/debug_lines", TRUTH(debug_lines));
      }
      else if (device_topic == "set/debug_flush") {
	NOTICE("Set debug_flush");
	bool flush = false;
	if (Payload == "on") flush=true;
	else if (Payload == "true") flush=true;
	else if (Payload == "flush") flush=true;
	else if (Payload == "1") flush=true;
	debug_flush = flush;
	mqtt_publish("status/debug_flush", TRUTH(debug_flush));
      }
      else {
	if (use_device_topic) {
	  ALERT("No handler for backplane %s topic [%s]", device_id, topic);
	}
      }
    }

    for (int i=0; leaves[i]; i++) {
      Leaf *leaf = leaves[i];
      if (leaf->wants_topic(device_type, device_name, device_topic)) {
	LEAF_DEBUG("Invoke leaf receiver %s", leaf->describe().c_str());
	handled |= leaf->mqtt_receive(device_type, device_name, device_topic, Payload);
      }
    }
    if (!handled) {
      DEBUG("No leaf handled topic [%s]", topic);
    }

    break;
  }

  if (!handled) {
    // Leaves can also subscribe to raw topics, so try that
    for (int i=0; leaves[i]; i++) {
      Leaf *leaf = leaves[i];
      if (leaf->wants_raw_topic(Topic)) {
	handled |= leaf->mqtt_receive_raw(Topic, Payload);
      }
    }
  }

  if (flags & PUBSUB_LOOPBACK) {
    mqttLoopback = false;
  }

  LEAVE;
}


#endif
// Local Variables:
// mode: C++
// c-basic-offset: 2
// End:
