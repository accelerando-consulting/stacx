#pragma "once"

// Wemos d1 mini (esp8266) exposes gpios up to gpio17 (aka A0)
// For ESP32 you may need to set max pin as high as 39
#if defined(ESP8266)
#define MAX_PIN 17
#define pinmask_t uint32_t
#define NO_PINS ((pinmask_t)0L)
#else
#define MAX_PIN 39
#define pinmask_t uint64_t
#define NO_PINS ((pinmask_t)0LL)
#endif

#define NO_TAPS ""

#define LEAF_STOP false
#define LEAF_RUN true

#define LEAF_USE_SSL true
#define LEAF_NO_SSL false

#define LEAF_USE_DEVICE_TOPIC true
#define LEAF_NO_DEVICE_TOPIC false

#define LEAF_PIN(n) ((n<0)?0:((pinmask_t)1<<(pinmask_t)n))

#define FOR_PINS(block)  for (int pin = 0; pin <= MAX_PIN ; pin++) { pinmask_t mask = ((pinmask_t)1)<<(pinmask_t)pin; if (pin_mask & mask) block }

#define WHEN(topic_str, block) if (topic==topic_str) { handled=true; block; }
#define WHENPREFIX(topic_str, block) if (topic.startsWith(topic_str)) { handled=true; topic.remove(0,String(topic_str).length()); block; }
#define ELSEWHEN(topic_str, block) else WHEN(topic_str,block)
#define ELSEWHENPREFIX(topic_str, block) else WHENPREFIX(topic_str,block)
#define WHENFROM(source, topic_str, block) if ((name==source) && (topic==topic_str)) { handled=true; block; }
#define ELSEWHENFROM(source, topic_str, block) else WHENFROM(source, topic_str, block)
#define WHENFROMKIND(kind, topic_str, block) if ((type==kind) && (topic==topic_str)) { handled=true; block; }
#define ELSEWHENFROMKIND(kind, topic_str, block) else WHENFROMKIND(kind, topic_str, block)


//
// Forward delcarations for the MQTT base functions (TODO: make this a class)
//
class Leaf;

class Tap
{
public:
  String alias;
  Leaf *target;
  Tap(String alias, Leaf *target)
  {
    this->alias=alias;
    this->target=target;
  }
};

int _compareStringKeys(String &a, String &b) {
  if (a == b) return 0;      // a and b are equal
  else if (a > b) return 1;  // a is bigger than b
  else return -1;            // a is smaller than b
}


//
//@******************************* class Leaf *********************************
//
// An abstract superclass for all sensor/actuator types.
//
// Takes care of pin resources, naming, and heartbeat.
//
class AbstractIpLeaf;
class AbstractPubsubLeaf;
class StorageLeaf;

enum leaf_value_acl { GET_SET, GET_ONLY, SET_ONLY };

class Leaf
{
protected:
  AbstractIpLeaf *ipLeaf = NULL;
  AbstractPubsubLeaf *pubsubLeaf = NULL;
  unsigned long last_heartbeat= 0;
  StorageLeaf *prefsLeaf = NULL;
  String tap_targets;
  SimpleMap<String,String> *cmd_descriptions;
  SimpleMap<String,String> *set_descriptions;
  SimpleMap<String,String> *get_descriptions;
public:
  static const bool PIN_NORMAL=false;
  static const bool PIN_INVERT=true;
  

  Leaf(String t, String name, pinmask_t pins=0);
  virtual void setup();
  virtual void loop();
  virtual void heartbeat(unsigned long uptime);
  virtual void mqtt_connect();
  virtual void mqtt_do_subscribe();
  virtual void start();
  virtual void stop();
  virtual void pre_sleep(int duration=0) {};
  virtual void post_sleep() {};
  virtual void pre_reboot() {};

  virtual void mqtt_disconnect() {};
  virtual bool wants_topic(String type, String name, String topic);
  virtual bool wants_raw_topic(String topic) { return false ; }
  virtual bool mqtt_receive(String type, String name, String topic, String payload);
  virtual bool mqtt_receive_raw(String topic, String payload) {return false;};
  virtual void status_pub() {};
  virtual bool parsePayloadBool(String payload, bool default_value = false) ;
  void message(Leaf *target, String topic, String payload="1", codepoint_t where=undisclosed_location);
  void message(String target, String topic, String payload="1", codepoint_t where=undisclosed_location);
  void publish(String topic, String payload, int level = L_DEBUG, codepoint_t where=undisclosed_location);
  void publish(String topic, uint16_t payload, int level = L_DEBUG, codepoint_t where=undisclosed_location) ;
  void publish(String topic, float payload, int decimals=1, int level=L_DEBUG, codepoint_t where=undisclosed_location);
  void publish(String topic, bool flag, int level=L_DEBUG, codepoint_t where=undisclosed_location);
  void mqtt_publish(String topic, String payload, int qos = 0, bool retain = false, int level=L_DEBUG, codepoint_t where=undisclosed_location);
  void register_mqtt_cmd(String cmd, String description="",codepoint_t where=undisclosed_location);
  void register_mqtt_value(String cmd, String description="",enum leaf_value_acl acl=GET_SET,codepoint_t where=undisclosed_location);
  void mqtt_subscribe(String topic, int qos = 0, int level=L_INFO, codepoint_t where=undisclosed_location);
  void mqtt_subscribe(String topic, codepoint_t where=undisclosed_location);
  String get_name() { return leaf_name; }
  String get_type() { return leaf_type; }
  virtual const char *get_name_str() { return leaf_name.c_str(); }
  String describe() { return leaf_type+"/"+leaf_name; }
  bool canRun() { return run; }
  bool canStart() { return run && !inhibit_start; }
  void inhibitStart() { inhibit_start=true; }
  void permitStart(bool start=false) { inhibit_start=false; if (start && canRun() && !isStarted()) this->start(); }
  bool isStarted() { return started; }
  void preventRun() { run = false; }
  void permitRun() { run = true; permitStart(); }
  bool hasUnit() 
  {
    return (leaf_unit.length()>0);
  }
  String getUnit() 
  {
    return leaf_unit;
  }
  Leaf *setUnit(String u) {
    leaf_unit=u;
    impersonate_backplane=true;
    return this;
  }
  Leaf *setMute() {
    leaf_mute=true;
    return this;
  }
  Leaf *inhibit() {
    preventRun();
    return this;
  }
  Leaf *usePriority(String default_priority="normal") {
    leaf_priority = default_priority;
    return this;
  }
  Leaf *setInternal() {
    leaf_mute=true;
    impersonate_backplane=true;
    return this;
  }
  void setComms(AbstractIpLeaf *ip, AbstractPubsubLeaf *pubsub) 
  {
    if (ip) this->ipLeaf = ip;
    if (pubsub) this->pubsubLeaf = pubsub;
  }
  AbstractIpLeaf *getIpComms() { return ipLeaf; }
  AbstractPubsubLeaf *getPubsubComms() { return pubsubLeaf; }
  
  bool hasPriority() { return (leaf_priority.length() > 0); }
  String getPriority() { return leaf_priority; }

  static void wdtReset() 
  {
#if USE_WDT
#ifdef ESP8266
      ESP.wdtFeed();
#else
      esp_task_wdt_reset();
#endif
#endif
  }

  void install_taps(String target);
  void tap(String publisher, String alias, String type="");
  Leaf *tap_type(String type);
  void add_tap(String alias, Leaf *subscriber);
  Leaf *get_tap(String alias);
  static Leaf *get_leaf(Leaf **leaves, String type, String name);
  static Leaf *get_leaf_by_name(Leaf **leaves, String name);
  static Leaf *get_leaf_by_type(Leaf **leaves, String name);

  void describe_taps(void);
  void describe_output_taps(void);

  String getPref(String key, String default_value="", String description="");
  bool getPref(String key, String *value, String description="");

  String getStrPref(String key, String default_value="", String description="") 
  {
    return getPref(key, default_value, description);
  }
  bool getStrPref(String key, String *value, String description="") 
  {
    return getPref(key, value, description);
  }
  int getIntPref(String key, int default_value, String description="");
  bool getIntPref(String key, int *value, String description="");
  bool getULongPref(String key, unsigned long *value, String description="");
  bool getBoolPref(String key, bool *value, String description="");
  bool getBoolPref(String key, bool default_value, String description="");
  float getFloatPref(String key, float default_value,String description="");
  bool getFloatPref(String key, float *value,String description="");
  double getDoublePref(String key, double default_value,String description="");
  void setPref(String key, String value);
  void setBoolPref(String key, bool value);
  void setIntPref(String key, int value);
  void setULongPref(String key, unsigned long value);
  void setFloatPref(String key, float value);
  void setDoublePref(String key, double value);

protected:
  void enable_pins_for_input(bool pullup=false);
  void enable_pins_for_output() ;
  void set_pins();
  void clear_pins();
  Leaf *find(String find_name, String find_type="");
  Leaf *find_type(String find_type);
  void reboot(void);

  bool setup_done = false;
  bool started = false;
  bool run = true;
  bool inhibit_start = false;
  bool impersonate_backplane = false;
  const char *TAG=NULL;
  String leaf_type;
  String leaf_name;
  String base_topic;
  String leaf_unit="";
  bool leaf_mute=false;
  String leaf_priority = "";
  pinmask_t pin_mask = 0;
  bool pin_invert = false;
  bool do_heartbeat = false;
  unsigned long heartbeat_interval_seconds = ::heartbeat_interval_seconds;
  bool do_presence = false;
  bool do_status = true;

private:
  SimpleMap<String,Tap*> *taps = NULL;
  SimpleMap<String,Leaf*> *tap_sources = NULL;
};

extern Leaf *leaves[];

#include "abstract_ip.h"
#include "abstract_pubsub.h"

Leaf::Leaf(String t, String name, pinmask_t pins)
{
  LEAF_ENTER(L_INFO);
  leaf_type = t;
  leaf_name = name;
  TAG = leaf_name.c_str();
  pin_mask = pins;
  taps = new SimpleMap<String,Tap*>(_compareStringKeys);
  tap_sources = new SimpleMap<String,Leaf*>(_compareStringKeys);
  cmd_descriptions = new SimpleMap<String,String>(_compareStringKeys);
  get_descriptions = new SimpleMap<String,String>(_compareStringKeys);
  set_descriptions = new SimpleMap<String,String>(_compareStringKeys);
  LEAF_LEAVE;
}

void Leaf::start(void)
{
  LEAF_ENTER(L_NOTICE);
  ACTION("START %s", leaf_name.c_str());
  if (!canRun() || inhibit_start) {
    LEAF_NOTICE("Starting leaf from stopped state");
    // This leaf is being started from stopped state
    run = true;
    inhibit_start = false;
    if (!setup_done) {
      LEAF_NOTICE("Executing setup");
      this->setup();
    }
  }
  started = true;
  // this can also get called as a first-time event after setup,
  // in which case it is mostly a no-op (but may do stuff in subclasses)
  LEAF_LEAVE;
}

void Leaf::stop(void)
{
  LEAF_ENTER(L_DEBUG);
  started = run = false;
  LEAF_LEAVE;
}

void Leaf::reboot(void)
{
#ifdef ESP8266
	ESP.reset();
#else
	ESP.restart();
#endif
}



Leaf *Leaf::get_leaf(Leaf **leaves, String type, String name)
{
  for (int i = 0; leaves[i]; i++) {
    if (leaves[i]->leaf_type == type && leaves[i]->leaf_name == name) return leaves[i];
  }
  return NULL;
}

Leaf *Leaf::get_leaf_by_type(Leaf **leaves, String type)
{
  for (int i = 0; leaves[i]; i++) {
    if (leaves[i]->leaf_type == type) return leaves[i];
  }
  return NULL;
}

Leaf *Leaf::get_leaf_by_name(Leaf **leaves, String key)
{
  int slashPos = key.indexOf('/');
  if (slashPos < 0) {
    // Just a name, no type
    for (int i = 0; leaves[i]; i++) {
      if (leaves[i]->leaf_name == key) return leaves[i];
    }
  }
  else {
    // we have "type/name"
    String type = key.substring(0,slashPos);
    String name = key.substring(slashPos+1);
    for (int i = 0; leaves[i]; i++) {
      if ((leaves[i]->leaf_type == type) && (leaves[i]->leaf_name == name)) {
	return leaves[i];
      }
    }
  }
  return NULL;
}

void Leaf::setup(void)
{
  LEAF_ENTER(L_DEBUG);
  ACTION("SETUP %s", leaf_name.c_str());

  // Find and tap the default IP and PubSub leaves, if any.   This relies on
  // these leaves being declared before any of their users.
  //
  // Note: don't try and tap yourself!
  LEAF_DEBUG("Install standard taps");
  if (leaf_name == "prefs") { 
    prefsLeaf = (StorageLeaf *)this;
  }
  else {
    LEAF_DEBUG("tap storage");
    prefsLeaf = (StorageLeaf *)tap_type("storage");
    if (prefsLeaf == NULL) {
      LEAF_ALERT("Did not find prefs leaf");
    }
  }
  
  if (leaf_type == "ip") { 
    ipLeaf = (AbstractIpLeaf *)this;
  }
  else {
    LEAF_DEBUG("tap IP");
    ipLeaf = (AbstractIpLeaf *)tap_type("ip");
    if (ipLeaf == NULL) {
      LEAF_ALERT("Did not find IP leaf");
    }
  }
  
  if (leaf_type == "pubsub") {
    pubsubLeaf = (AbstractPubsubLeaf *)this;
  }
  else{
    LEAF_DEBUG("tap pubsub");
    pubsubLeaf = (AbstractPubsubLeaf *)tap_type("pubsub");
    if (pubsubLeaf == NULL) {
      LEAF_ALERT("Did not find pubsub leaf");
    }
  }

  if (tap_targets) {
    install_taps(tap_targets);
    tap_targets="";
  }
    
  LEAF_DEBUG("Configure mqtt behaviour");
  if (!pubsubLeaf || pubsubLeaf->pubsubUseDeviceTopic()) {
    if (impersonate_backplane) {
      LEAF_INFO("Leaf %s will impersonate the backplane", leaf_name.c_str());
      base_topic = _ROOT_TOPIC + "devices/" + device_id + String("/");
    } else {
      //LEAF_DEBUG("Leaf %s uses a device-type based topic", leaf_name.c_str());
      base_topic = _ROOT_TOPIC + "devices/" + device_id + String("/") + leaf_type + String("/") + leaf_name + String("/");
    }
  }
  else {
    //LEAF_DEBUG("Leaf %s uses a device-id based topic", leaf_name.c_str());
    base_topic = _ROOT_TOPIC + device_id + String("/");
    if (hasUnit()) {
      base_topic = base_topic + leaf_unit + "/";
    }
  }
  
  LEAF_DEBUG("Configure pins");
  if (pin_mask != NO_PINS) {
#if defined(ESP8266)
    LEAF_DEBUG("Pin mask for %s/%s (%s) is %08x", leaf_type.c_str(), leaf_name.c_str(), base_topic.c_str(), pin_mask);
#else
    LEAF_INFO("Pin mask for %s/%s %s is %08x%08x", leaf_type.c_str(), leaf_name.c_str(),
		base_topic.c_str(), (unsigned long)((uint64_t)pin_mask>>32), (unsigned long)pin_mask);
#endif
  }
  else {
    LEAF_NOTICE("Created leaf %s/%s with base topic %s", leaf_type.c_str(), leaf_name.c_str(), base_topic.c_str());
  }

  setup_done = true;
  LEAF_LEAVE;
}

void Leaf::register_mqtt_cmd(String cmd, String description, codepoint_t where) 
{
  cmd_descriptions->put(cmd, description);
  mqtt_subscribe(String("cmd/")+cmd, CODEPOINT(where));
}

void Leaf::register_mqtt_value(String value, String description, enum leaf_value_acl acl,codepoint_t where) 
{
  if (acl==GET_ONLY || acl==GET_SET) {
    get_descriptions->put(value, description);
    mqtt_subscribe(String("get/")+value, CODEPOINT(where));
  }
  if (acl==SET_ONLY || acl==GET_SET) {
    set_descriptions->put(value, description);
    mqtt_subscribe(String("set/")+value, CODEPOINT(where));
  }
}

void Leaf::mqtt_do_subscribe() {
  //LEAF_DEBUG("mqtt_do_subscribe base_topic=%s", base_topic.c_str());
  if (do_status) {
    register_mqtt_cmd("status", "invoke the receiving leaf's status handler", HERE);
  }
}

void Leaf::enable_pins_for_input(bool pullup)
{
  FOR_PINS({
      LEAF_NOTICE("%s uses pin %d as INPUT%s", base_topic.c_str(), pin, pullup?"_PULLUP":"");
      pinMode(pin, pullup?INPUT_PULLUP:INPUT);
    })
}

void Leaf::enable_pins_for_output()
{
  FOR_PINS({
      LEAF_NOTICE("%s uses pin %d as OUTPUT", base_topic.c_str(), pin);
#ifdef ESP32
      if (!digitalPinIsValid(pin)) {
	LEAF_ALERT("Pin %d is not a valid pin on this architecture");
      }
      if (!digitalPinCanOutput(pin)) {
	LEAF_ALERT("Pin %d is not an output pin");
      }
#endif
      
      pinMode(pin, OUTPUT);
    })
}

void Leaf::set_pins()
{
  FOR_PINS({
      //LEAF_DEBUG("%s sets pin %d HIGH", base_topic.c_str(), pin);
      digitalWrite(pin, pin_invert?LOW:HIGH);
    })
}

void Leaf::clear_pins()
{
  FOR_PINS({
      //LEAF_DEBUG("%s sets pin %d LOW", base_topic.c_str(), pin);
      digitalWrite(pin, pin_invert?HIGH:LOW);
    })
}

void Leaf::loop()
{
  unsigned long now = millis();

  if (do_heartbeat && (now > (last_heartbeat + heartbeat_interval_seconds*1000))) {
    last_heartbeat = now;
    //LEAF_DEBUG("time to publish heartbeat");
    this->heartbeat(now/1000);
  }
}

void Leaf::heartbeat(unsigned long uptime)
{
    mqtt_publish("status/heartbeat", String(uptime, DEC), 0, false);
}

void Leaf::mqtt_connect()
{
  LEAF_ENTER(L_DEBUG);

  if (pubsubLeaf && do_presence) mqtt_publish("status/presence", String("online"), 0, false);
  LEAF_LEAVE;
}

bool Leaf::wants_topic(String type, String name, String topic)
{
  if (topic=="cmd/help" && cmd_descriptions->size()) return true; // this module offers help
  if (topic.startsWith("cmd/") && cmd_descriptions->has(topic.substring(4))) return true; // this is a registered command
  
  return ((type=="*" || type == leaf_type) && (name=="*" || name == leaf_name)); // this message addresses this leaf

  // superclass should handle other wants, and also call this parent method 
}

bool Leaf::mqtt_receive(String type, String name, String topic, String payload)
{
  LEAF_DEBUG("Message for %s as %s: %s <= %s", base_topic.c_str(), name.c_str(), topic.c_str(), payload.c_str());
  bool handled = false;
  String key;
  String desc;
  
  WHEN("cmd/help", {
      if (payload=="" || payload=="cmd") {
	for (int i=0; i < cmd_descriptions->size(); i++) {
	  key = cmd_descriptions->getKey(i);
	  desc = cmd_descriptions->getData(i);
	  mqtt_publish("status/help/cmd/"+key, desc+" ("+describe()+")", 0, false, L_INFO, HERE);
	}
      }
      // not an else-case
      if (payload=="" || payload=="set") {
	for (int i=0; i < set_descriptions->size(); i++) {
	  key = set_descriptions->getKey(i);
	  desc = set_descriptions->getData(i);
	  mqtt_publish("status/help/set/"+key, desc+" ("+describe()+")", 0, false, L_INFO, HERE);
	}
      }
      // not an else-case
      if (payload=="" || payload=="get") {
	for (int i=0; i < cmd_descriptions->size(); i++) {
	  key = get_descriptions->getKey(i);
	  desc = get_descriptions->getData(i);
	  mqtt_publish("status/help/get/"+key, desc+" ("+describe()+")", 0, false, L_INFO, HERE);
	}
      }
    })
  WHEN("cmd/status",{
      if (this->do_status || payload.toInt()) {
	LEAF_NOTICE("Responding to cmd/status");
	this->status_pub();
      }
    })
  ELSEWHEN("set/do_status",{
      do_status = payload.toInt();
    });
  return handled;
}

bool Leaf::parsePayloadBool(String payload, bool default_value) 
{
  if (payload == "") return default_value;
  return (payload == "on")||(payload == "true")||(payload == "high")||(payload == "1");
}

void Leaf::message(Leaf *target, String topic, String payload, codepoint_t where)
{
  //LEAF_ENTER(L_DEBUG);
  LEAF_INFO_AT(CODEPOINT(where), "Message %s => %s: %s <= [%s]",
	      this->leaf_name.c_str(),
	      target->leaf_name.c_str(), topic.
	      c_str(),
	      payload.c_str());
  target->mqtt_receive(this->leaf_type, this->leaf_name, topic, payload);
  //LEAF_LEAVE;
}

void Leaf::message(String target, String topic, String payload, codepoint_t where)
{
  // Send the publish to any leaves that have "tapped" into this leaf with a
  // particular alias name
  Leaf *target_leaf = get_tap(target);
  if (target_leaf) {
    message(target_leaf, topic, payload, where);
  }
  else {
    LEAF_ALERT_AT(CODEPOINT(where), "Cant find target leaf \"%s\" for message", target.c_str());
  }
}

void Leaf::publish(String topic, String payload, int level, codepoint_t where)
{
  LEAF_ENTER_STR(L_DEBUG, topic);

  // Send the publish to any leaves that have "tapped" into this leaf
  for (int t = 0; t < this->taps->size(); t++) {
    String target_name = this->taps->getKey(t);
    Tap *tap = this->taps->getData(t);
    Leaf *target = tap->target;
    String alias = tap->alias;

    __LEAF_DEBUG_AT__((where.file?where:HERE), level, "TPUB %s(%s) => %s %s %s",
		      this->leaf_name.c_str(), alias.c_str(),
		      target->get_name().c_str(), topic.c_str(), payload.c_str());

    target->mqtt_receive(leaf_type, alias, topic, payload);
  }
  LEAF_LEAVE;
}

void Leaf::publish(String topic, uint16_t payload, int level, codepoint_t where)
{
  publish(topic, String((int)payload), level, where);
}

void Leaf::publish(String topic, float payload, int decimals, int level, codepoint_t where)
{
  publish(topic, String(payload,decimals), level, where);
}

void Leaf::publish(String topic, bool flag, int level, codepoint_t where)
{
  publish(topic, String(flag?"1":"0"), level, where);
}

void Leaf::mqtt_subscribe(String topic, int qos, int level, codepoint_t where)
{
  __LEAF_DEBUG_AT__(CODEPOINT(where), level, "mqtt_subscribe(%s) QOS=%d", topic.c_str(), qos);
  
  if (pubsubLeaf == NULL) return;
  if (topic.startsWith("cmd/") && !use_cmd) return;
  if (topic.startsWith("get/") && !use_get) return;
  if (topic.startsWith("set/") && !use_set) return;

  if (use_wildcard_topic) {
    if ((topic.indexOf('#')<0) && (topic.indexOf('+')<0)) {
      INFO("Suppress subscribe due to wildcard: %s", topic.c_str());
      return;
    }
  }

  if (use_flat_topic) {
      // for servers like sensabhub, change topic from status/foo to
      // status-foo
      String flat_topic = topic;
      flat_topic.replace("/","-");
      pubsubLeaf->_mqtt_subscribe(base_topic + flat_topic, qos, CODEPOINT(where));
  }
  else {
    pubsubLeaf->_mqtt_subscribe(base_topic + topic, qos, CODEPOINT(where));
  }
}

void Leaf::mqtt_subscribe(String topic, codepoint_t where) 
{
  mqtt_subscribe(topic, 0, L_INFO, where);
}




void Leaf::mqtt_publish(String topic, String payload, int qos, bool retain, int level, codepoint_t where)
{
  LEAF_ENTER(L_DEBUG);

  bool is_muted = leaf_mute;
  if (topic.startsWith("status/help/")) {
    is_muted=false;
  }
  

  // Publish to the MQTT server (unless this leaf is "muted", i.e. performs local publish only)
  if (pubsubLeaf && !is_muted) {
    if (!pubsub_loopback && topic.startsWith("status/") && !use_status) {
      LEAF_NOTICE("Status publish disabled for %s", topic.c_str());
    }
    else if (!pubsub_loopback && topic.startsWith("event/") && !use_event) {
      LEAF_NOTICE("Event publish disabled for %s", topic.c_str());
    }
    else {
      if (use_flat_topic) {
	// for servers like sensabhub, change topic from status/foo to
	// status-foo
	String flat_topic = topic;
	flat_topic.replace("/","-");
	__LEAF_DEBUG_AT__((where.file?where:HERE), level, "PUB [%s] <= [%s]", topic.c_str(), payload.c_str());
	pubsubLeaf->_mqtt_publish(base_topic + flat_topic, payload, qos, retain);
      }
      else {
	if (hasPriority() && topic.length()) {
	  if (topic.startsWith("status/")) {
	    topic = "admin/" + topic;
	  }
	  else if (topic.startsWith("change/")) {
	    topic = "alert/" + topic;
	    LEAF_WARN("ALERT PUB [%s] <= [%s]", topic.c_str(), payload.c_str());
	  }
	  else {
	    topic = leaf_priority + "/" + topic;
	  }
	}
	if (topic.length()) {
	  topic = base_topic + topic;
	}
	else {
	  topic = base_topic.substring(0,base_topic.length()-1);
	}
	__LEAF_DEBUG_AT__((where.file?where:HERE), level, "PUB [%s] <= [%s]", topic.c_str(), payload.c_str());
	pubsubLeaf->_mqtt_publish(topic, payload, qos, retain);
      }
    }
  }
  else {
    if (!leaf_mute && !::pubsub_loopback) {
      LEAF_WARN("No pubsub leaf");
    }

    if (pubsub_loopback && pubsubLeaf) {
      // this message won't be sent to MQTT, but record it for shell
      LEAF_NOTICE("Storing loopback publish [%s] <= [%s]", topic.c_str(), payload.c_str());

      pubsubLeaf->storeLoopback(topic, payload);
    }
  }

  // Send the publish to any leaves that have "tapped" into this leaf
  publish(topic, payload);

  //LEAF_LEAVE;
}

extern Leaf *leaves[]; // you should define and null-terminate this array in your main.ino

Leaf *Leaf::find(String find_name, String find_type)
{
  Leaf *result = NULL;
  //LEAF_ENTER(L_DEBUG);

  // Find a leaf with a given name, and return a pointer to it
  for (int s=0; leaves[s]; s++) {
    if (leaves[s]->leaf_name == find_name) {
      if ((find_type=="") || (leaves[s]->leaf_type==find_type)) {
		result = leaves[s];
	break;
      }
    }
  }
  return(result);
}

Leaf *Leaf::find_type(String find_type)
{
  Leaf *result = NULL;
  //LEAF_ENTER(L_DEBUG);

  // Find a leaf with a given type, and return a pointer to it
  for (int s=0; leaves[s]; s++) {
    if (leaves[s]->leaf_type == find_type) {
      result = leaves[s];
      break;
    }
  }
  return(result);
}

void Leaf::install_taps(String target)
{
  if (target.length() == 0) return;
  //LEAF_ENTER(L_DEBUG);
  LEAF_INFO("Leaf %s has taps [%s]", this->leaf_name.c_str(), target.c_str());
  String t = target;
  int pos ;
  do {
    String target_name;
    String target_alias;
    String target_type;

    // a target specifier is [type@]name[=alias]

    if ((pos = t.indexOf(',')) > 0) {
      target_name = t.substring(0, pos);
      t.remove(0,pos+1);
    }
    else {
      target_name = t;
      t="";
    }

    if ((pos = target_name.indexOf('@')) > 0) {
      target_type = target_name.substring(0, pos);
      target_name.remove(0,pos+1);
    }
    else {
      target_type = "";
    }
 
    if ((pos = target_name.indexOf('=')) > 0) {
      target_alias = target_name.substring(0, pos);
      target_name.remove(0,pos+1);
    }
    else {
      target_alias = target_name;
    }

    this->tap(target_name, target_alias, target_type);

  } while (t.length() > 0);
  //LEAF_LEAVE;
}

void Leaf::add_tap(String alias, Leaf *subscriber)
{
  //LEAF_ENTER(L_DEBUG);
  taps->put(subscriber->leaf_name, new Tap(alias,subscriber));
  //LEAF_LEAVE;
}

void Leaf::tap(String publisher, String alias, String type)
{
  //LEAF_ENTER(L_DEBUG);
  LEAF_INFO("Leaf %s taps into %s@%s (as %s)",
	    this->leaf_name.c_str(),
	    (type=="")?"any":type.c_str(),
	    publisher.c_str(),
	    alias.c_str());

  Leaf *target = find(publisher, type);
  if (target) {
    target->add_tap(alias, this);
    this->tap_sources->put(alias, target);
  }
  else {
    LEAF_INFO("Did not find target specifier");
  }
  
  //LEAF_LEAVE;
}

Leaf * Leaf::tap_type(String type)
{
  LEAF_ENTER(L_DEBUG);
  LEAF_DEBUG("search for leaf of type [%s]", type.c_str());

  Leaf *target = find_type(type);
  if (target) {
    LEAF_DEBUG("Leaf [%s] taps [%s]", this->describe().c_str(), target->describe().c_str());
    target->add_tap(leaf_name, this);
    this->tap_sources->put(target->get_name(), target);
  }
  else {
    LEAF_DEBUG("No match");
  }
  LEAF_LEAVE;
  return target;
}


Leaf *Leaf::get_tap(String alias)
{
  Leaf *result = tap_sources->get(alias);
  if (result == NULL) {
    LEAF_ALERT("Leaf %s is unable to find requested tap source %s", this->leaf_name.c_str(), alias.c_str());
  }

  return result;
}

void Leaf::describe_taps(void)
{
  LEAF_DEBUG("Leaf %s has %d tap sources: ", this->leaf_name.c_str(), this->tap_sources->size());
  int source_count = this->tap_sources?this->tap_sources->size():0;
  for (int t = 0; t < this->tap_sources->size(); t++) {
    String alias = this->tap_sources->getKey(t);
    Leaf *target = this->tap_sources->getData(t);
    LEAF_DEBUG("   Tap %s <= %s(%s)",
	   this->leaf_name.c_str(), target->leaf_name.c_str(), alias.c_str());
  }
}

void Leaf::describe_output_taps(void)
{
  LEAF_ENTER(L_DEBUG);
  LEAF_DEBUG("Leaf %s has %d tap outputs: ", this->leaf_name.c_str(), this->taps->size());
  for (int t = 0; t < this->taps->size(); t++) {
    String target_name = this->taps->getKey(t);
    Tap *tap = this->taps->getData(t);
    String alias = tap->alias;
    LEAF_DEBUG("   Tap %s => %s as %s",
	   this->leaf_name.c_str(), target_name.c_str(), alias.c_str());
  }
  LEAF_LEAVE;
}

String Leaf::getPref(String key, String default_value, String description)
{
  String result = "";
  if (!prefsLeaf) {
    LEAF_ALERT("Cannot get %s, no preferences leaf", key.c_str());
    result = default_value;
  }
  else {
    result = prefsLeaf->get(key, default_value);
  }
  return result;
}

bool Leaf::getPref(String key, String *value, String description)
{
  String result = "";
  if (!prefsLeaf) {
    LEAF_ALERT("Cannot get %s, no preferences leaf", key.c_str());
    return false;
  }
  if (value) {
    *value= prefsLeaf->get(key, *value, description);
  }
  return true;
}



int Leaf::getIntPref(String key, int default_value, String description)
{
  if (!prefsLeaf) {
    LEAF_ALERT("Cannot get %s, no preferences leaf", key.c_str());
    return default_value;
  }
  return prefsLeaf->getInt(key, default_value, description);
}

bool Leaf::getIntPref(String key, int *value, String description)
{
  if (!prefsLeaf) {
    LEAF_ALERT("Cannot get %s, no preferences leaf", key.c_str());
    return false;
  }
  if (value) {
    *value = prefsLeaf->getInt(key, *value, description);
  }
  return true;
}

bool Leaf::getULongPref(String key, unsigned long *value, String description)
{
  if (!prefsLeaf) {
    LEAF_ALERT("Cannot get %s, no preferences leaf", key.c_str());
    return false;
  }
  if (value) {
    *value = prefsLeaf->getULong(key, *value, description);
  }
  return true;
}

bool Leaf::getBoolPref(String key, bool default_value, String description)
{
  if (!prefsLeaf) {
    LEAF_ALERT("Cannot get %s, no preferences leaf", key.c_str());
    return default_value;
  }
  if (description) {
    prefsLeaf->set_description(key, description);
    prefsLeaf->set_default(key, TRUTH_lc(default_value));
  }
  String pref = prefsLeaf->get(key);
  bool value = default_value;
  if (!pref.length()) return value;
  if ((pref == "on") || (pref=="true") || (pref=="1") || pref.startsWith("enable")) {
    value = true;
  }
  if ((pref == "off") || (pref=="false") || (pref=="0") || pref.startsWith("disable")) {
    value = false;
  }
  return value;
}

bool Leaf::getBoolPref(String key, bool *value, String description)
{
  if (!prefsLeaf) {
    LEAF_ALERT("Cannot get %s, no preferences leaf", key.c_str());
    return false;
  }
  if (description) {
    prefsLeaf->set_description(key, description);
    prefsLeaf->set_default(key, TRUTH_lc(*value));
  }
  String pref = prefsLeaf->get(key, "", "");
  if (!pref.length()) return false;
  if (value) {
    if ((pref == "on") || (pref=="true") || (pref=="1") || pref.startsWith("enable")) {
      *value = true;
    }
    else if ((pref == "off") || (pref=="false") || (pref=="0") || pref.startsWith("disable")) {
      *value = false;
    }
    else {
      LEAF_ALERT("Cannot parse [%s] as boolean for [%s]", pref.c_str(), key.c_str());
      return false;
    }
  }
  return true;
}

float Leaf::getFloatPref(String key, float default_value, String description)
{
  if (!prefsLeaf) {
    LEAF_ALERT("Cannot get %s, no preferences leaf", key.c_str());
    return default_value;
  }
  return prefsLeaf->getFloat(key, default_value, description);
}

bool Leaf::getFloatPref(String key, float *value, String description)
{
  if (!prefsLeaf) {
    LEAF_ALERT("Cannot get %s, no preferences leaf", key.c_str());
    return false;
  }
  if (value) *value = prefsLeaf->getFloat(key, *value, description);
  return true;
}

double Leaf::getDoublePref(String key, double default_value, String description)
{
  if (!prefsLeaf) {
    LEAF_ALERT("Cannot get %s, no preferences leaf", key.c_str());
    return default_value;
  }
  return prefsLeaf->getDouble(key, default_value, description);
}

void Leaf::setPref(String key, String value)
{
  if (!prefsLeaf) {
    LEAF_ALERT("Cannot save %s, no preferences leaf", key.c_str());
  }
  else {
    prefsLeaf->put(key, value);
  }
}

void Leaf::setBoolPref(String key, bool value)
{
  if (!prefsLeaf) {
    LEAF_ALERT("Cannot save %s, no preferences leaf", key.c_str());
  }
  else {
    prefsLeaf->put(key, value?"on":"off");
  }
}

void Leaf::setIntPref(String key, int value)
{
  if (!prefsLeaf) {
    LEAF_ALERT("Cannot save %s, no preferences leaf", key.c_str());
  }
  else {
    prefsLeaf->putInt(key, value);
  }
}

void Leaf::setULongPref(String key, unsigned long value)
{
  if (!prefsLeaf) {
    LEAF_ALERT("Cannot save %s, no preferences leaf", key.c_str());
  }
  else {
    prefsLeaf->putInt(key, value);
  }
}

void Leaf::setFloatPref(String key, float value)
{
  if (!prefsLeaf) {
    LEAF_ALERT("Cannot save %s, no preferences leaf", key.c_str());
  }
  else {
    if (isnan(value)) {
      prefsLeaf->put(key, "");
    }
    else {
      prefsLeaf->putFloat(key, value);
    }
  }
}

void Leaf::setDoublePref(String key, double value)
{
  if (!prefsLeaf) {
    LEAF_ALERT("Cannot save %s, no preferences leaf", key.c_str());
  }
  else {
    if (isnan(value)) {
      prefsLeaf->put(key, "");
    }
    else {
      prefsLeaf->putDouble(key, value);
    }
  }
}



// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
