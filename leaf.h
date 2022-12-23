#pragma "once"

#ifdef ESP32
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif

#ifndef STACX_NO_HELP
#ifdef ESP8266
#define STACX_NO_HELP 1
#else
#define STACX_NO_HELP 0
#endif
#endif

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
#define WHENSUB(topic_str, block) if (topic.indexOf(topic_str)>=0) { handled=true; topic.remove(0,topic.indexOf(topic_str)); block; }
#define ELSEWHEN(topic_str, block) else WHEN(topic_str,block)
#define ELSEWHENPREFIX(topic_str, block) else WHENPREFIX(topic_str,block)
#define ELSEWHENSUB(topic_str, block) else WHENSUB(topic_str,block)
#define WHENFROM(source, topic_str, block) if ((name==source) && (topic==topic_str)) { handled=true; block; }
#define WHENFROMSUB(source, topic_str, block) if ((name==source) && (topic.indexOf(topic_str)>=0)) { handled=true; topic.remove(0,topic.indexOf(topic_str)); block; }
#define ELSEWHENFROM(source, topic_str, block) else WHENFROM(source, topic_str, block)
#define ELSEWHENFROMSUB(source, topic_str, block) else WHENFROMSUB(source, topic_str, block)
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

class TraitDebuggable
{
public:
  int class_debug_level;
  String leaf_name;

  TraitDebuggable(String name, int l=L_USE_DEFAULT) 
  {
    leaf_name = name;
    class_debug_level = l;
  }

  String getName() { return leaf_name; }
  void setName(String name) { leaf_name=name; }
  virtual void setDebugLevel(int l) { class_debug_level = l; }
  virtual int getDebugLevel() { return (class_debug_level==L_USE_DEFAULT)?debug_level:class_debug_level; }
  virtual const char *getNameStr() { return leaf_name.c_str(); }
};

  

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

enum leaf_value_acl { ACL_GET_SET, ACL_GET_ONLY, ACL_SET_ONLY };
enum leaf_value_kind { VALUE_KIND_BOOL, VALUE_KIND_INT, VALUE_KIND_ULONG, VALUE_KIND_FLOAT, VALUE_KIND_DOUBLE, VALUE_KIND_STR };

#define VALUE_CAN_SET(v) ((v->acl==ACL_GET_SET) || (v->acl==ACL_SET_ONLY))
#define VALUE_CAN_GET(v) ((v->acl==ACL_GET_SET) || (v->acl==ACL_GET_ONLY))

#define VALUE_SAVE true
#define VALUE_NO_SAVE false

typedef void (*value_setter_t) (Leaf *, struct leaf_value *, String);


struct leaf_value {
  enum leaf_value_kind kind;
  enum leaf_value_acl acl;
  bool save;
  String description;
  void *value;
  value_setter_t setter;
};
  
class Leaf: virtual public TraitDebuggable
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
  SimpleMap<String,struct leaf_value *> *value_descriptions;
#ifdef ESP32
  bool own_loop = false;
  int loop_stack_size=16384;
  TaskHandle_t leaf_loop_handle = NULL;
#endif

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
  virtual bool hasHelp() 
  {
    return ( (cmd_descriptions && cmd_descriptions->size()) ||
	     (get_descriptions && get_descriptions->size()) ||
	     (set_descriptions && set_descriptions->size()) ||
	     (value_descriptions && value_descriptions->size()));
  }
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
  void register_mqtt_value(String cmd, String description="",enum leaf_value_acl acl=ACL_GET_SET,codepoint_t where=undisclosed_location);
  void registerValue(codepoint_t where, String name, enum leaf_value_kind kind, void *value, String description, enum leaf_value_acl=ACL_GET_SET, bool save=true, value_setter_t setter=NULL);
  void registerLeafValue(codepoint_t where, String name, enum leaf_value_kind kind, void *value, String description, enum leaf_value_acl=ACL_GET_SET, bool save=true, value_setter_t setter=NULL);
  void mqtt_subscribe(String topic, int qos = 0, int level=L_INFO, codepoint_t where=undisclosed_location);
  void mqtt_subscribe(String topic, codepoint_t where=undisclosed_location);
  String get_type() { return leaf_type; }
  String describe() { return leaf_type+"/"+leaf_name; }
  bool canRun() { return run; }
  bool canStart() { return run && !inhibit_start; }
  void inhibitStart() { inhibit_start=true; }
  void permitStart(bool start=false) { inhibit_start=false; if (start && canRun() && !isStarted()) this->start(); }
  bool isStarted() { return started; }
  bool hasOwnLoop() { 
#ifdef ESP32
    return own_loop;
#else
    return false;
#endif
  }
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
  AbstractIpLeaf *getIpComms() { return (leaf_type=="ip")?(AbstractIpLeaf *)this:ipLeaf; }
  AbstractPubsubLeaf *getPubsubComms() { return (leaf_type=="pubsub")?(AbstractPubsubLeaf *)this:pubsubLeaf; }
  
  bool hasPriority() { return (leaf_priority.length() > 0); }
  String getPriority() { return leaf_priority; }
  bool isPriority(String s) { return (leaf_priority==s); }

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

  bool hasTap(String name) { return tap_sources->has(name); }
  bool tappedBy(String name) { return taps->has(name); }
  bool hasActiveTap(String name) {
    if (!hasTap(name)) return false;
    return get_tap(name)->isStarted();
  }
    
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
  bool parseBool(String value, bool default_value=false, bool *valid_r=NULL);

  bool getBoolPref(String key, bool *value, String description="");
  bool getBoolPref(String key, bool default_value, String description="");
  float getFloatPref(String key, float default_value,String description="");
  bool getFloatPref(String key, float *value,String description="");
  double getDoublePref(String key, double default_value,String description="");
  bool getDoublePref(String key, double *value, String description="");
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
  : TraitDebuggable(name)
{
  LEAF_ENTER_STR(L_INFO, String(__FILE__));
  leaf_type = t;
  TAG = leaf_name.c_str();
  pin_mask = pins;
  taps = new SimpleMap<String,Tap*>(_compareStringKeys);
  tap_sources = new SimpleMap<String,Leaf*>(_compareStringKeys);
  cmd_descriptions = new SimpleMap<String,String>(_compareStringKeys);
  get_descriptions = new SimpleMap<String,String>(_compareStringKeys);
  set_descriptions = new SimpleMap<String,String>(_compareStringKeys);
  value_descriptions = new SimpleMap<String,struct leaf_value *>(_compareStringKeys);
  LEAF_LEAVE;
}

#ifdef ESP32
static void leaf_own_loop(void *args)
{
  Leaf *leaf = (Leaf *)args;

  while (1) {
    while (!_stacx_ready || !leaf->isStarted()) {
      //Serial.printf("await start condition for %s\n", leaf->describe().c_str());
      vTaskDelay(500*portTICK_PERIOD_MS);
    }
    WARN("Entering separate loop for %s\n", leaf->describe().c_str());
    while (leaf->canRun()) {
      leaf->loop();
    }
    WARN("Exiting separate loop for %s\n", leaf->describe().c_str());
  }
}
#endif

void Leaf::start(void)
{
#if DEBUG_LINES
  LEAF_ENTER(L_DEBUG)
#else
  LEAF_ENTER_STR(L_NOTICE,String(__FILE__));
#endif
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

#ifdef ESP32
  if (hasOwnLoop() && (leaf_loop_handle==NULL)) {
#if !DEBUG_THREAD
    LEAF_ALERT("DEBUG_THREAD should be set when using own_loop!");
#endif
    char task_name[32];
    LEAF_ALERT("Creating separate loop task for %s", describe().c_str());
    snprintf(task_name, sizeof(task_name), "%s_loop", leaf_name.c_str());
    xTaskCreateUniversal(&leaf_own_loop,      // task code
			 task_name,           // task_name
			 loop_stack_size,     // stack depth
			 this,                // parameters
			 1,                   // priority
			 &leaf_loop_handle,   // task handle
			 ARDUINO_RUNNING_CORE // core id
      );

    // if own_loop is set, concrete subclass must set the started member
  }
  else {
#else
  {
#endif
      started = true;
  }

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
  ACTION("SETUP %s", leaf_name.c_str());
  LEAF_ENTER_STR(L_DEBUG, String(__FILE__));
  LEAF_INFO("Tap targets for %s are %s", describe().c_str(), tap_targets.c_str());

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

  if (tap_targets.length()) {
    LEAF_DEBUG("Install declared taps %s", tap_targets.c_str());
    install_taps(tap_targets);
    //tap_targets="";
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

  getBoolPref("leaf_status_"+leaf_name, &do_status); // no description=invisible
  getBoolPref("leaf_mute"+leaf_name, &leaf_mute); // no description=invisible
  getIntPref("leaf_debug"+leaf_name, &class_debug_level); // no description=invisible

  setup_done = true;
  LEAF_LEAVE;
}

void Leaf::register_mqtt_cmd(String cmd, String description, codepoint_t where) 
{
  LEAF_ENTER_STR(L_DEBUG, cmd);
#if STACX_NO_HELP
  description = ""; // save RAM
#endif
  cmd_descriptions->put(cmd, description);
  LEAF_LEAVE;
}

void Leaf::register_mqtt_value(String value, String description, enum leaf_value_acl acl,codepoint_t where) 
{
#if STACX_NO_HELP
  description = ""; // save RAM
#endif

  if (acl==ACL_GET_ONLY || acl==ACL_GET_SET) {
    get_descriptions->put(value, description);
    //TODO: do move this to mqtt_do_subscribe
    //mqtt_subscribe(String("get/")+value, CODEPOINT(where));
  }
  if (acl==ACL_SET_ONLY || acl==ACL_GET_SET) {
    set_descriptions->put(value, description);
    //TODO: do move this to mqtt_do_subscribe
    //mqtt_subscribe(String("set/")+value, CODEPOINT(where));
  }
}

void Leaf::registerValue(codepoint_t where, String name, enum leaf_value_kind kind, void *value, String description, enum leaf_value_acl acl, bool save, value_setter_t setter) 
{
#if STACX_NO_HELP
  description = ""; // save RAM
#endif

  struct leaf_value *val = (struct leaf_value *)calloc(1, sizeof(struct leaf_value));
  if (!val) {
    LEAF_ALERT("Allocation failed");
    return;
  }
  val->kind =  kind;
  val->value = value;
  val->description = description;
  val->acl = acl;
  val->save = save;
  val->setter = setter;
  value_descriptions->put(name, val);
  register_mqtt_value(name, description, acl, CODEPOINT(where));

  if (!save) {
    return;
  }
  
  switch (kind) {
  case VALUE_KIND_BOOL:
    getBoolPref(name, (bool *)value, description);
    break;
  case VALUE_KIND_INT:
    getIntPref(name, (int *)value, description);
    break;
  case VALUE_KIND_ULONG:
    getULongPref(name, (unsigned long *)value, description);
    break;
  case VALUE_KIND_FLOAT:
    getFloatPref(name, (float *)value, description);
    break;
  case VALUE_KIND_DOUBLE:
    getDoublePref(name, (double *)value, description);
    break;
  case VALUE_KIND_STR:
    getPref(name, (String *)value, description);
  }
}

void Leaf::registerLeafValue(codepoint_t where, String name, enum leaf_value_kind kind, void *value, String description, enum leaf_value_acl acl, bool save, value_setter_t setter) 
{
  registerValue(CODEPOINT(where), leaf_name+"/"+name, kind, value, description, acl, save, setter);
}


void Leaf::mqtt_do_subscribe() {
  //LEAF_DEBUG("mqtt_do_subscribe base_topic=%s", base_topic.c_str());
  if (do_status) {
    register_mqtt_cmd("status", "invoke the receiving leaf's status handler", HERE);
  }
  // TODO: maybe subscribe to the commands in cmd_descriptions here, if WILDCARD is not in use?
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
  LEAF_ENTER(L_TRACE);
  unsigned long now = millis();

  if (do_heartbeat && (now > (last_heartbeat + heartbeat_interval_seconds*1000))) {
    last_heartbeat = now;
    //LEAF_DEBUG("time to publish heartbeat");
    this->heartbeat(now/1000);
  }
  LEAF_LEAVE;
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
  if (topic=="cmd/help" && hasHelp()) return true; // this module offers help
  if (topic.startsWith("cmd/") && cmd_descriptions && cmd_descriptions->has(topic.substring(4))) return true; // this is a registered command
  if (topic.startsWith("get/") && get_descriptions && get_descriptions->has(topic.substring(4))) return true; // this is a registered gettable value
  if (topic.startsWith("set/") && set_descriptions && set_descriptions->has(topic.substring(4))) return true; // this is a registered settable value
  
  return ((type=="*" || type == leaf_type) && (name=="*" || name == leaf_name)); // this message addresses this leaf

  // subclasses should handle other wants, and also call this parent method 
}

bool Leaf::mqtt_receive(String type, String name, String topic, String payload)
{
  LEAF_ENTER(L_DEBUG);
  LEAF_DEBUG("Message for %s as %s: %s <= %s", base_topic.c_str(), name.c_str(), topic.c_str(), payload.c_str());
  bool handled = false;
  String key;
  String desc;

  WHEN("set/debug_level", {
      int l = payload.toInt();
      LEAF_NOTICE("Set debug level for %s to %d", describe().c_str(), l);
      setDebugLevel(l);
    })
  ELSEWHENPREFIX("set/", {
      handled=false;
      if (value_descriptions->has(topic)) {
	struct leaf_value *val = value_descriptions->get(topic);
	if (VALUE_CAN_SET(val)) {
	  handled=true;
	  if (val->setter != NULL) {
	    val->setter(this, val, payload);
	  }
	  else {
	    switch (val->kind) {
	    case VALUE_KIND_BOOL:
	      *(bool *)val->value = parseBool(payload, *(bool *)val->value);
	      if (val->save) setBoolPref(name, *(bool *)val->value);
	      break;
	    case VALUE_KIND_INT:
	      *(int *)val->value = payload.toInt();
	      if (val->save) setIntPref(name, *(int *)val->value);
	      break;
	    case VALUE_KIND_ULONG:
	      *(unsigned long *)val->value = payload.toInt();
	      if (val->save) setULongPref(name, *(unsigned long *)val->value);
	      break;
	    case VALUE_KIND_FLOAT:
	      *(float *)val->value = payload.toFloat();
	      if (val->save) setFloatPref(name, *(float *)val->value);
	      break;
	    case VALUE_KIND_DOUBLE:
	      *(double *)val->value = payload.toDouble();
	      if (val->save) setDoublePref(name, *(double *)val->value);
	      break;
	    case VALUE_KIND_STR:
	      *(String *)val->value = payload;
	      setPref(name, payload);
	    }
	  }
	}
      }
    })
  ELSEWHENPREFIX("get/", {
      handled=false;
      if (value_descriptions->has(topic)) {
	struct leaf_value *val = value_descriptions->get(topic);
	if (VALUE_CAN_GET(val)) {
	  handled = true;
	  switch (val->kind) {
	  case VALUE_KIND_BOOL:
	    mqtt_publish("status/"+topic, TRUTH(*(bool *)val->value));
	    break;
	  case VALUE_KIND_INT:
	    mqtt_publish("status/"+topic, String(*(int *)val->value));
	    break;
	  case VALUE_KIND_ULONG:
	    mqtt_publish("status/"+topic, String(*(unsigned long *)val->value));
	    break;
	  case VALUE_KIND_FLOAT:
	    mqtt_publish("status/"+topic, String(*(float *)val->value));
	    break;
	  case VALUE_KIND_DOUBLE:
	    mqtt_publish("status/"+topic, String(*(double *)val->value));
	  case VALUE_KIND_STR:
	    mqtt_publish("status/"+topic, *(String *)val->value);
	  }
	}
      }
    })
  ELSEWHEN("cmd/help", {
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
  WHEN("cmd/leaf/status",{
      char top[80];
      char msg[80];
      snprintf(top, sizeof(top), "status/leaf/%s", describe().c_str());
      snprintf(msg, sizeof(msg), "canRun=%s started=%s ip=%s pubsub=%s",
	       TRUTH_lc(canRun()), TRUTH_lc(isStarted()), ipLeaf?ipLeaf->describe().c_str():"none", pubsubLeaf?pubsubLeaf->describe().c_str():"none");
      mqtt_publish(top, msg);
    })
  WHEN("cmd/taps",{
      int stash = class_debug_level;
      class_debug_level=L_DEBUG;
      describe_taps();
      describe_output_taps();
      class_debug_level=stash;
    })
  LEAF_BOOL_RETURN(handled);
}

bool Leaf::parsePayloadBool(String payload, bool default_value) 
{
  if (payload == "") return default_value;
  return (payload == "on")||(payload == "true")||(payload == "high")||(payload == "1");
}

void Leaf::message(Leaf *target, String topic, String payload, codepoint_t where)
{
  //LEAF_ENTER(L_DEBUG);
  if (target) {
    LEAF_INFO_AT(CODEPOINT(where), "Message %s => %s: %s <= [%s]",
		 this->leaf_name.c_str(),
		 target->leaf_name.c_str(), topic.
		 c_str(),
		 payload.c_str());
    target->mqtt_receive(this->leaf_type, this->leaf_name, topic, payload);
  }
  else {
    LEAF_ALERT_AT(CODEPOINT(where), "Null target leaf for message");
  }
    
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
  LEAF_ENTER_STR(level, topic);

  // Send the publish to any leaves that have "tapped" into this leaf
  for (int t = 0; t < this->taps->size(); t++) {
    String target_name = this->taps->getKey(t);
    Tap *tap = this->taps->getData(t);
    Leaf *target = tap->target;
    String alias = tap->alias;

    if (topic.startsWith("_")) {
      level++;
    }
    __LEAF_DEBUG_AT__((where.file?where:HERE), level, "TPUB %s(%s) => %s %s %s",
		      this->leaf_name.c_str(), alias.c_str(),
		      target->getName().c_str(), topic.c_str(), payload.c_str());

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
	    if (getPriority()=="normal") {
	      topic = "admin/" + topic;
	    }
	    else {
	      topic = getPriority() + "/" + topic;
	    }
	  }
	  else if (topic.startsWith("change/")) {
	    topic = "alert/" + topic;
	    LEAF_WARN("ALERT PUB [%s] <= [%s]", topic.c_str(), payload.c_str());
	  }
	  else {
	    topic = leaf_priority + "/" + topic;
	  }
	}
	String full_topic;
	if (topic.length()) {
	  full_topic = base_topic + topic;
	}
	else {
	  full_topic = base_topic.substring(0,base_topic.length()-1);
	}
	__LEAF_DEBUG_AT__((where.file?where:HERE), level, "PUB [%s] <= [%s]", full_topic.c_str(), payload.c_str());
	pubsubLeaf->_mqtt_publish(full_topic, payload, qos, retain);
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
  publish(topic, payload, level, CODEPOINT(where));

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
  LEAF_ENTER_STR(L_DEBUG, type);
  LEAF_DEBUG("search for leaf of type [%s]", type.c_str());

  Leaf *target = find_type(type);
  if (target) {
    LEAF_DEBUG("Leaf [%s] taps [%s]", this->describe().c_str(), target->describe().c_str());
    target->add_tap(leaf_name, this);
    this->tap_sources->put(target->getName(), target);
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
  LEAF_ENTER(L_DEBUG);
  LEAF_DEBUG("Leaf %s has %d tap sources: ", this->leaf_name.c_str(), this->tap_sources->size());
  int source_count = this->tap_sources?this->tap_sources->size():0;
  for (int t = 0; t < this->tap_sources->size(); t++) {
    String alias = this->tap_sources->getKey(t);
    Leaf *target = this->tap_sources->getData(t);
    LEAF_DEBUG("   Tap %s <= %s(%s)",
	   this->leaf_name.c_str(), target->leaf_name.c_str(), alias.c_str());
  }
  LEAF_LEAVE;
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
  LEAF_ENTER_STR(L_DEBUG, key);
  if (!prefsLeaf) {
    LEAF_ALERT("Cannot get %s, no preferences leaf", key.c_str());
    LEAF_BOOL_RETURN(false);
  }
  if (value) {
    *value = prefsLeaf->getInt(key, *value, description);
  }
  LEAF_BOOL_RETURN(true);
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

bool Leaf::parseBool(String pref, bool default_value, bool *valid_r) 
{
  bool value = default_value;
  bool valid = false;

  if (pref.length()) {
    if ((pref == "on") || (pref=="true") || (pref=="1") || pref.startsWith("enable")) {
      value = true;
      valid = true;
    }
    if ((pref == "off") || (pref=="false") || (pref=="0") || pref.startsWith("disable")) {
      value = false;
      valid = true;
    }
  }
  
  if (valid_r) *valid_r=valid;
  return value;
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
  return parseBool(prefsLeaf->get(key), default_value);
}

bool Leaf::getBoolPref(String key, bool *value, String description)
{
  LEAF_ENTER_STR(L_DEBUG, key);
  if (!prefsLeaf) {
    LEAF_ALERT("Cannot get %s, no preferences leaf", key.c_str());
    LEAF_BOOL_RETURN(false);
  }
  if (description) {
    prefsLeaf->set_description(key, description);
    prefsLeaf->set_default(key, TRUTH_lc(*value));
  }
  String pref = prefsLeaf->get(key, "", "");
  if (!pref.length()) {
    LEAF_NOTICE("getBoolPref(%s) <= %s (default)", key.c_str(), ABILITY(*value));
    LEAF_BOOL_RETURN(false);
  }
  if (value) {
    if ((pref == "on") || (pref=="true") || (pref=="1") || pref.startsWith("enable")) {
      *value = true;
    }
    else if ((pref == "off") || (pref=="false") || (pref=="0") || pref.startsWith("disable")) {
      *value = false;
    }
    else {
      LEAF_ALERT("Cannot parse [%s] as boolean for [%s]", pref.c_str(), key.c_str());
      LEAF_BOOL_RETURN(false);
    }
  }
  LEAF_NOTICE("getBoolPref(%s) <= %s", key.c_str(), ABILITY(*value));
  LEAF_BOOL_RETURN(true);
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

bool Leaf::getDoublePref(String key, double *value, String description)
{
  if (!prefsLeaf) {
    LEAF_ALERT("Cannot get %s, no preferences leaf", key.c_str());
    return false;
  }
  if (value) *value = prefsLeaf->getDouble(key, *value, description);
  return true;
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
