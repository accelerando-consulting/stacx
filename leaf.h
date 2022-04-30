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

#define LEAF_PIN(n) ((pinmask_t)1<<(pinmask_t)n)

#define FOR_PINS(block)  for (int pin = 0; pin <= MAX_PIN ; pin++) { pinmask_t mask = ((pinmask_t)1)<<(pinmask_t)pin; if (pin_mask & mask) block }

#define WHEN(topic_str, block) if (topic==topic_str) { handled=true; block; }
#define ELSEWHEN(topic_str, block) else WHEN(topic_str,block)
#define WHENFOR(target, topic_str, block) if ((name==target) && (topic==topic_str)) { handled=true; block; }
#define ELSEWHENFOR(target, topic_str, block) else WHENFOR(target, topic_str, block)
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


class Leaf
{
protected:
  AbstractIpLeaf *ipLeaf = NULL;
  AbstractPubsubLeaf *pubsubLeaf = NULL;
  StorageLeaf *prefsLeaf = NULL;
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

  void message(Leaf *target, String topic, String payload="1");
  void message(String target, String topic, String payload="1");
  void publish(String topic, String payload);
  void publish(String topic, uint16_t payload) ;
  void publish(String topic, float payload, int decimals=1);
  void publish(String topic, bool flag);
  void mqtt_publish(String topic, String payload, int qos = 0, bool retain = false);
  void mqtt_publish(String topic, String payload, bool retain = false);
  void mqtt_publish(String topic, const char *payload, bool retain = false);
  void mqtt_publish(const char *topic, String payload, bool retain = false);
  void mqtt_publish(const char *topic, const char *payload, bool retain = false);
  void mqtt_subscribe(String topic, int qos = 0);
  String get_name() { return leaf_name; }
  String describe() { return leaf_type+"/"+leaf_name; }
  bool canRun() { return run; }
  bool isStarted() { return started; }
  void preventRun() { run = false; }

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

  String getPref(String key, String default_value="");
  int getIntPref(String key, int default_value);
  float getFloatPref(String key, float default_value);
  double getDoublePref(String key, double default_value);
  void setPref(String key, String value);
  void setIntPref(String key, int value);
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
  bool impersonate_backplane = false;
  String leaf_type;
  String leaf_name;
  String base_topic;
  pinmask_t pin_mask = 0;
  bool pin_invert = false;
  bool do_heartbeat = false;
  unsigned long heartbeat_interval_seconds = HEARTBEAT_INTERVAL_SECONDS;
  bool do_presence = false;
  bool do_status = true;

private:
  unsigned long last_heartbeat = 0;
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
  pin_mask = pins;
  taps = new SimpleMap<String,Tap*>(_compareStringKeys);
  tap_sources = new SimpleMap<String,Leaf*>(_compareStringKeys);
  LEAF_LEAVE;
}

void Leaf::start(void)
{
  LEAF_ENTER(L_TRACE);
  if (!run) {
    // This leaf is being started from stopped state
    if (!setup_done) {
      this->setup();
    }
    run = true;
  }
  started = true;
  // this can also get called as a first-time event after setup,
  // in which case it is mostly a no-op (but may do stuff in subclasses)
  LEAF_LEAVE;
}

void Leaf::stop(void)
{
  //LEAF_ENTER(L_DEBUG);
  run = false;
  //LEAF_LEAVE;
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
  ACTION("SU %s", leaf_name.c_str());

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

    if (leaf_type == "ip") { 
      ipLeaf = (AbstractIpLeaf *)this;
    }
    else {
      LEAF_DEBUG("tap IP");
      ipLeaf = (AbstractIpLeaf *)tap_type("ip");
    }
  
    if (leaf_type == "pubsub") {
      pubsubLeaf = (AbstractPubsubLeaf *)this;
    }
    else{
      LEAF_DEBUG("tap pubsub");
      pubsubLeaf = (AbstractPubsubLeaf *)tap_type("pubsub");
    }

    if (leaf_type == "storage") {
      prefsLeaf = (StorageLeaf *)this;
    }
    else{
      LEAF_DEBUG("tap storage");
      prefsLeaf = (StorageLeaf *)tap_type("storage");
    }
  }
    
  LEAF_DEBUG("Configure mqtt behaviour");
  if (!pubsubLeaf || pubsubLeaf->use_device_topic) {
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
    LEAF_INFO("Created leaf %s/%s with base topic %s", leaf_type.c_str(), leaf_name.c_str(), base_topic.c_str());
  }

  setup_done = true;
  LEAF_LEAVE;
}

void Leaf::mqtt_do_subscribe() {
  //LEAF_DEBUG("mqtt_do_subscribe base_topic=%s", base_topic.c_str());
  if (pubsubLeaf && pubsubLeaf->use_device_topic && use_cmd) mqtt_subscribe("cmd/status");
}

void Leaf::enable_pins_for_input(bool pullup)
{
  FOR_PINS({
      LEAF_INFO("%s claims pin %d as INPUT%s", base_topic.c_str(), pin, pullup?"_PULLUP":"");
      pinMode(pin, pullup?INPUT_PULLUP:INPUT);
    })
}

void Leaf::enable_pins_for_output()
{
  FOR_PINS({
      LEAF_INFO("%s claims pin %d as OUTPUT", base_topic.c_str(), pin);
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
  LEAF_ENTER(L_INFO);

  if (pubsubLeaf && do_presence) mqtt_publish("status/presence", String("online"), 0, false);
  LEAF_LEAVE;
}

bool Leaf::wants_topic(String type, String name, String topic)
{
  return ((type=="*" || type == leaf_type) && (name=="*" || name == leaf_name));
}

bool Leaf::mqtt_receive(String type, String name, String topic, String payload)
{
  LEAF_DEBUG("Message for %s as %s: %s <= %s", base_topic.c_str(), name.c_str(), topic.c_str(), payload.c_str());
  bool handled = false;
  WHEN("cmd/status",{
      if (this->do_status) {
	this->status_pub();
      }
    })
  ELSEWHEN("set/do_status",{
      do_status = payload.toInt();
    });
  return handled;
}

void Leaf::message(Leaf *target, String topic, String payload)
{
  //LEAF_ENTER(L_DEBUG);
  LEAF_INFO("Message %s => %s %s",
	this->leaf_name.c_str(), target->leaf_name.c_str(), topic.c_str());
  target->mqtt_receive(this->leaf_type, this->leaf_name, topic, payload);
  //LEAF_LEAVE;
}

void Leaf::message(String target, String topic, String payload)
{
  // Send the publish to any leaves that have "tapped" into this leaf with a
  // particular alias name
  Leaf *target_leaf = get_tap(target);
  if (target_leaf) {
    message(target_leaf, topic, payload);
  }
  else {
    LEAF_ALERT("Cant find target leaf \"%s\" for message", target.c_str());
  }
}

void Leaf::publish(String topic, String payload)
{
  //LEAF_ENTER(L_DEBUG);

  // Send the publish to any leaves that have "tapped" into this leaf
  for (int t = 0; t < this->taps->size(); t++) {
    String target_name = this->taps->getKey(t);
    Tap *tap = this->taps->getData(t);
    Leaf *target = tap->target;
    String alias = tap->alias;
    LEAF_INFO("Tap publish %s(%s) => %s %s %s",
		this->leaf_name.c_str(), alias.c_str(),
		target->leaf_name.c_str(), topic.c_str(), payload.c_str());
    target->mqtt_receive(this->leaf_type, alias, topic, payload);
  }
  //LEAF_LEAVE;
}

void Leaf::publish(String topic, uint16_t payload)
{
  publish(topic, String((int)payload));
}

void Leaf::publish(String topic, float payload, int decimals)
{
  publish(topic, String(payload,decimals));
}

void Leaf::publish(String topic, bool flag)
{
  publish(topic, String(flag?"1":"0"));
}

void Leaf::mqtt_subscribe(String topic, int qos)
{
  if (pubsubLeaf == NULL) return;
  if (topic.startsWith("cmd/") && !use_cmd) return;
  if (topic.startsWith("get/") && !use_get) return;
  if (topic.startsWith("set/") && !use_set) return;

  if (use_wildcard_topic) {
    if ((topic.indexOf('#')<0) && (topic.indexOf('+')<0)) {
      DEBUG("Suppress subscribe due to wildcard: %s", topic.c_str());
      return;
    }
    
  }

  if (use_flat_topic) {
      // for servers like sensabhub, change topic from status/foo to
      // status-foo
      String flat_topic = topic;
      flat_topic.replace("/","-");
      pubsubLeaf->_mqtt_subscribe(base_topic + flat_topic, qos);
  }
  else {
      pubsubLeaf->_mqtt_subscribe(base_topic + topic, qos);
  }
}




void Leaf::mqtt_publish(String topic, String payload, int qos, bool retain)
{
  //LEAF_ENTER(L_DEBUG);
  LEAF_INFO("PUB %s => [%s]", topic.c_str(), payload.c_str());

  // Send the publish to any leaves that have "tapped" into this leaf
  publish(topic, payload);

  // Publish to the MQTT server
  if (pubsubLeaf) {
    if (!mqttLoopback && topic.startsWith("status/") && !use_status) {
      LEAF_NOTICE("Status publish disabled for %s", topic.c_str());
    }
    else if (!mqttLoopback && topic.startsWith("event/") && !use_event) {
      LEAF_NOTICE("Event publish disabled for %s", topic.c_str());
    }
    else {
      if (use_flat_topic) {
	// for servers like sensabhub, change topic from status/foo to
	// status-foo
	String flat_topic = topic;
	flat_topic.replace("/","-");
	pubsubLeaf->_mqtt_publish(base_topic + flat_topic, payload, qos, retain);
      }
      else {
	pubsubLeaf->_mqtt_publish(base_topic + topic, payload, qos, retain);
      }
    }
  }

  //LEAF_LEAVE;
}

void Leaf::mqtt_publish(String topic, String payload, bool retain)
{
  mqtt_publish(topic, payload, 0, retain);
}

void Leaf::mqtt_publish(String topic, const char * payload, bool retain)
{
  mqtt_publish(topic, String(payload), 0, retain);
}

void Leaf::mqtt_publish(const char *topic, String payload, bool retain)
{
  mqtt_publish(String(topic), payload, 0, retain);
}

void Leaf::mqtt_publish(const char *topic, const char *payload, bool retain)
{
  mqtt_publish(String(topic), String(payload), 0, retain);
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
  LEAF_NOTICE("Leaf %s has taps [%s]", this->leaf_name.c_str(), target.c_str());
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
    target->add_tap(type, this);
    this->tap_sources->put(type, target);
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
  for (int t = 0; t < this->tap_sources->size(); t++) {
    String alias = this->tap_sources->getKey(t);
    Leaf *target = this->tap_sources->getData(t);
    LEAF_DEBUG("   Tap %s <= %s(%s)",
	   this->leaf_name.c_str(), target->leaf_name.c_str(), alias.c_str());
  }
}

void Leaf::describe_output_taps(void)
{
  LEAF_DEBUG("Leaf %s has %d tap outputs: ", this->leaf_name.c_str(), this->taps->size());
  for (int t = 0; t < this->taps->size(); t++) {
    String target_name = this->taps->getKey(t);
    Tap *tap = this->taps->getData(t);
    String alias = tap->alias;
    LEAF_DEBUG("   Tap %s => %s as %s",
	   this->leaf_name.c_str(), target_name.c_str(), alias.c_str());
  }
}

String Leaf::getPref(String key, String default_value)
{
  String result = "";
  if (!prefsLeaf) {
    LEAF_ALERT("Cannot get %s, no preferences leaf", key.c_str());
    return default_value;
  }
  else {
    result = prefsLeaf->get(key, default_value);
  }
  return result;
}

int Leaf::getIntPref(String key, int default_value)
{
  if (!prefsLeaf) {
    LEAF_ALERT("Cannot get %s, no preferences leaf", key.c_str());
    return default_value;
  }
  return prefsLeaf->getInt(key, default_value);
}

float Leaf::getFloatPref(String key, float default_value)
{
  if (!prefsLeaf) {
    LEAF_ALERT("Cannot get %s, no preferences leaf", key.c_str());
    return default_value;
  }
  return prefsLeaf->getFloat(key, default_value);
}

double Leaf::getDoublePref(String key, double default_value)
{
  if (!prefsLeaf) {
    LEAF_ALERT("Cannot get %s, no preferences leaf", key.c_str());
    return default_value;
  }
  return prefsLeaf->getDouble(key, default_value);
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

void Leaf::setIntPref(String key, int value)
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
    prefsLeaf->putFloat(key, value);
  }
}

void Leaf::setDoublePref(String key, double value)
{
  if (!prefsLeaf) {
    LEAF_ALERT("Cannot save %s, no preferences leaf", key.c_str());
  }
  else {
    prefsLeaf->putDouble(key, value);
  }
}



// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
