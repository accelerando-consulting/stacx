#pragma "once"

#ifdef ESP32
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif

#ifndef STACX_USE_HELP
#ifdef ESP8266
#define STACX_USE_HELP 0
#else
#define STACX_USE_HELP 1
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

#define WHEN(topic_str, block) if (topic==(topic_str)) { handled=true; block; }
#define WHENEITHER(topic_str1, topic_str2, block) if ((topic==(topic_str1))||(topic==(topic_str2))) { handled=true; block; }
#define WHENAND(topic_str1, condition, block) if ((topic==(topic_str1))&&(condition)) { handled=true; block; }
#define WHENPREFIX(topic_str, block) if (topic.startsWith(topic_str)) { handled=true; topic.remove(0,String(topic_str).length()); block; }
#define WHENPREFIXAND(topic_str, condition, block) if (topic.startsWith(topic_str)&&(condition)) { handled=true; topic.remove(0,String(topic_str).length()); block; }
#define WHENSUB(topic_str, block) if (topic.indexOf(topic_str)>=0) { handled=true; topic.remove(0,topic.indexOf(topic_str)); block; }
#define WHENFROM(source, topic_str, block) if ((name==(source)) && (topic==(topic_str))) { handled=true; block; }
#define WHENFROMEITHER(source, topic_str1, topic_str_2, block) if ((name==(source)) && ((topic==(topic_str1))||(topic==(topic_str_2)))) { handled=true; block; }
#define WHENFROMSUB(source, topic_str, block) if ((name==(source)) && (topic.indexOf(topic_str)>=0)) { handled=true; topic.remove(0,topic.indexOf(topic_str)); block; }
#define ELSEWHEN(topic_str, block) else WHEN((topic_str),block)
#define ELSEWHENAND(topic_str, condition, block) else WHENAND((topic_str),(condition),block)
#define ELSEWHENEITHER(topic_str1, topic_str2, block) else WHENEITHER((topic_str1),(topic_str2),block)
#define ELSEWHENPREFIX(topic_str, block) else WHENPREFIX((topic_str),block)
#define ELSEWHENPREFIXAND(topic_str, condition, block) else WHENPREFIXAND((topic_str),(condition),block)
#define ELSEWHENSUB(topic_str, block) else WHENSUB((topic_str),block)
#define ELSEWHENFROM(source, topic_str, block) else WHENFROM((source), (topic_str), block)
#define ELSEWHENFROMEITHER(source, topic_str1, topic_str2, block) else WHENFROMEITHER((source), (topic_str1), (topic_str2), block)
#define ELSEWHENFROMSUB(source, topic_str, block) else WHENFROMSUB((source), (topic_str), block)
#define WHENFROMKIND(kind, topic_str, block) if ((type==(kind)) && (topic==(topic_str))) { handled=true; block; }
#define ELSEWHENFROMKIND(kind, topic_str, block) else WHENFROMKIND((kind), (topic_str), block)


//
// Forward delcarations for the MQTT base functions (TODO: make this a class)
//
class Leaf;

//
//@******************************* class Tap *********************************

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
//@**************************** class Debuggable *****************************

class Debuggable
{
public:
  int class_debug_level;
  String leaf_name;

  Debuggable(String name, int l=L_USE_DEFAULT)
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
//@****************************** class Value ********************************

enum leaf_value_kind { VALUE_KIND_BOOL, VALUE_KIND_BYTE, VALUE_KIND_INT, VALUE_KIND_ULONG, VALUE_KIND_FLOAT, VALUE_KIND_DOUBLE, VALUE_KIND_STR };
enum leaf_value_acl { ACL_GET_SET, ACL_GET_ONLY, ACL_SET_ONLY };

#define VALUE_SAVE true
#define VALUE_NO_SAVE false

#define VALUE_SET_DIRECT true
#define VALUE_OVERRIDE_ACL true

// These are shortcut macros for registerValue that implicitly pass the codepoint and type arguments

#define registerBoolValue(name, ...)       registerValue(    HERE, (name), VALUE_KIND_BOOL  , __VA_ARGS__)
#define registerByteValue(name, ...)       registerValue(    HERE, (name), VALUE_KIND_BYTE  , __VA_ARGS__)
#define registerIntValue(name, ...)        registerValue(    HERE, (name), VALUE_KIND_INT   , __VA_ARGS__)
#define registerFloatValue(name, ...)      registerValue(    HERE, (name), VALUE_KIND_FLOAT , __VA_ARGS__)
#define registerStrValue(name, ...)        registerValue(    HERE, (name), VALUE_KIND_STR   , __VA_ARGS__)
#define registerUlongValue(name, ...)      registerValue(    HERE, (name), VALUE_KIND_ULONG , __VA_ARGS__)

#define registerLeafBoolValue(name, ...)   registerLeafValue(HERE, (name), VALUE_KIND_BOOL  , __VA_ARGS__)
#define registerLeafByteValue(name, ...)   registerLeafValue(HERE, (name), VALUE_KIND_BYTE  , __VA_ARGS__)
#define registerLeafIntValue(name, ...)    registerLeafValue(HERE, (name), VALUE_KIND_INT   , __VA_ARGS__)
#define registerLeafFloatValue(name, ...)  registerLeafValue(HERE, (name), VALUE_KIND_FLOAT , __VA_ARGS__)
#define registerLeafStrValue(name, ...)    registerLeafValue(HERE, (name), VALUE_KIND_STR   , __VA_ARGS__)
#define registerLeafUlongValue(name, ...)  registerLeafValue(HERE, (name), VALUE_KIND_ULONG , __VA_ARGS__)

class Value;

typedef bool (*value_setter_t) (Leaf *, String, Value *, String);

#define VALUE_AS(t, val) (*(t *)((val)->value))

static constexpr const char *value_kind_name[VALUE_KIND_STR+1] = {
  "bool","byte","int","ulong","float","double","string"
};

class Value
{
public:

  enum leaf_value_kind kind;
  enum leaf_value_acl acl;
  char acl_str[5]=""; // prws = Persistent Readble Writable (auto)Save
  bool save=true;
  String description;
  String default_value;
  void *value;
  value_setter_t setter;

  Value(enum leaf_value_kind kind, void *value=NULL, enum leaf_value_acl acl=ACL_GET_SET, bool save=true)
  {
    this->kind = kind;
    this->value = value;
    this->acl = acl;
    this->save = save;
    if (value) {
      this->default_value = this->asString();
    }
    makeAclStr();
  }

  void makeAclStr()
  {
    snprintf(acl_str, sizeof(acl_str), "%c%c%c%c",
	     canGet()?'r':'-',
	     canSet()?'w':'-',
	     hasPersistence()?'v':'-',
	     hasAutoSave()?'s':'-');
  }

  void setAutoSave(bool autosave=true)
  {
    save = autosave;
    makeAclStr();
  }

  const char *kindName() { return value_kind_name[this->kind]; }

  bool canSet() { return ((acl==ACL_GET_SET) || (acl==ACL_SET_ONLY)); }
  bool canGet() { return ((acl==ACL_GET_SET) || (acl==ACL_GET_ONLY)); }
  bool hasHelp() { return (description.length() > 0); }
  bool hasPersistence() { return value!=NULL; }
  bool hasAutoSave() { return save; }
  const char *getAcl() { return acl_str; }
  bool hasSetter() { return (setter!=NULL); }



  String asString() {
    if (value == NULL) {
      return "<UNAVAILABLE>";
    }

    switch (kind) {
    case VALUE_KIND_BOOL:
      return TRUTH(VALUE_AS(bool, this));
      break;
    case VALUE_KIND_BYTE:
      return String((int)(VALUE_AS(byte,this)));
      break;
    case VALUE_KIND_INT:
      return String(VALUE_AS(int, this));
      break;
    case VALUE_KIND_ULONG:
      return String(VALUE_AS(unsigned long, this));
      break;
    case VALUE_KIND_FLOAT:
      return String(VALUE_AS(float , this));
      break;
    case VALUE_KIND_DOUBLE:
      return String(VALUE_AS(float , this));
    case VALUE_KIND_STR:
      return VALUE_AS(String , this);
    }
    return "INVALID";
  }
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

#define LEAF_HANDLER(l) LEAF_ENTER_STR((l),topic);bool handled=false;
#define LEAF_HANDLER_END LEAF_BOOL_RETURN(handled)

class Leaf: virtual public Debuggable
{
protected:
  AbstractIpLeaf *ipLeaf = NULL;
  AbstractPubsubLeaf *pubsubLeaf = NULL;
  unsigned long last_heartbeat= 0;
  StorageLeaf *prefsLeaf = NULL;
  String tap_targets;
  SimpleMap<String,String> *cmd_descriptions;
  SimpleMap<String,Value *> *value_descriptions;
#ifdef ESP32
  bool own_loop = false;
  int loop_stack_size=16384;
  TaskHandle_t leaf_loop_handle = NULL;
#endif

public:
  static const bool PIN_NORMAL=false;
  static const bool PIN_INVERT=true;

  Leaf(String t, String name, pinmask_t pins=0, String target=NO_TAPS);
  virtual void setup();
  virtual void loop();
  virtual void heartbeat(unsigned long uptime);
  virtual void mqtt_connect();
  virtual void mqtt_do_subscribe(){};
  virtual void start();
  virtual void stop();
  virtual void pre_sleep(int duration=0) {};
  virtual void post_sleep() {};
  virtual void pre_reboot(String reason="") {};
  virtual String makeBaseTopic();

  virtual void mqtt_disconnect() {};
  virtual bool hasHelp()
  {
    return ( (cmd_descriptions && cmd_descriptions->size()) ||
	     (value_descriptions && value_descriptions->size()));
  }
  virtual bool wants_topic(String type, String name, String topic);
  virtual bool wants_raw_topic(String topic) { return false ; }
  virtual bool mqtt_receive(String type, String name, String topic, String payload, bool direct=false);
  virtual bool mqtt_receive_raw(String topic, String payload) {return false;};
  virtual void status_pub() {};
  virtual void config_pub() {};
  virtual void stats_pub() {};
  virtual bool parsePayloadBool(String payload, bool default_value = false) ;
  void message(Leaf *target, String topic, String payload="1", codepoint_t where=undisclosed_location);
  void message(String target, String topic, String payload="1", codepoint_t where=undisclosed_location);
  void publish(String topic, String payload, int level = L_DEBUG, codepoint_t where=undisclosed_location);
  void publish(String topic, uint16_t payload, int level = L_DEBUG, codepoint_t where=undisclosed_location) ;
  void publish(String topic, float payload, int decimals=1, int level=L_DEBUG, codepoint_t where=undisclosed_location);
  void publish(String topic, bool flag, int level=L_DEBUG, codepoint_t where=undisclosed_location);
  void mqtt_publish(String topic, String payload, int qos = 0, bool retain = false, int level=L_DEBUG, codepoint_t where=undisclosed_location);
  void registerCommand(codepoint_t where, String cmd, String description="");
  void registerLeafCommand(codepoint_t where, String cmd, String description="");
  void registerValue(codepoint_t where, String name, enum leaf_value_kind kind, void *value, String description="", enum leaf_value_acl=ACL_GET_SET, bool save=true, value_setter_t setter=NULL);
  void registerLeafValue(codepoint_t where, String name, enum leaf_value_kind kind, void *value, String description="", enum leaf_value_acl=ACL_GET_SET, bool save=true, value_setter_t setter=NULL);
  bool loadValues(void);
  bool loadValue(String name, Value *val);
  String getValueHelp(String name, Value *val);
  bool setValue(String topic, String payload, bool direct=false, bool allow_save=true, bool override_perms=false, bool *changed_r = NULL);
  bool getValue(String topic, String payload, Value **val_r, bool direct=false);

  virtual bool valueChangeHandler(String topic, Value *v) {
    LEAF_HANDLER(L_INFO);
    LEAF_HANDLER_END;
  }
  virtual bool commandHandler(String type, String name, String topic, String payload) {
    LEAF_HANDLER(L_INFO);
    LEAF_HANDLER_END;
  }
  void mqtt_subscribe(String topic, int qos = 0, int level=L_INFO, codepoint_t where=undisclosed_location);
  void mqtt_subscribe(String topic, codepoint_t where=undisclosed_location);
  String get_type() { return leaf_type; } // deprecated
  String getType() { return leaf_type; }
  String getBaseTopic() { return base_topic; }
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
  Leaf *setMute(bool m=true) {
    leaf_mute=m;
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
  AbstractIpLeaf *getIpComms() { return (leaf_type=="ip")?(AbstractIpLeaf *)this:ipLeaf; }
  AbstractPubsubLeaf *getPubsubComms() { return (leaf_type=="pubsub")?(AbstractPubsubLeaf *)this:pubsubLeaf; }
  String describeComms()
  {
    String result="";
    result += ipLeaf?((Leaf *)ipLeaf)->getName():"NONE";
    result += "/";
    result += pubsubLeaf?((Leaf *)pubsubLeaf)->getName():"NONE";
    return result;
  }

  virtual void setComms(AbstractIpLeaf *ip, AbstractPubsubLeaf *pubsub)
  {
    String commsWas = describeComms();
    if (ip) this->ipLeaf = ip;
    if (pubsub) this->pubsubLeaf = pubsub;
    String commsNow = describeComms();
    if (commsWas != commsNow) {
      LEAF_INFO("Comms for %s changed from %s to %s",
		describe().c_str(),
		commsWas.c_str(),
		commsNow.c_str()
	);

      // change of pubsub mechanism may result in change of topic structure
      makeBaseTopic();
    }
  }

  bool hasPriority() { return (leaf_priority.length() > 0); }
  String getPriority() { return leaf_priority; }
  bool isPriority(String s) { return (hasPriority() && (leaf_priority==s)); }

  static void wdtReset(codepoint_t where)
  {
#if USE_WDT
#ifdef ESP8266
      ESP.wdtFeed();
#else
      esp_err_t e = esp_task_wdt_reset();
      if (e!=ESP_OK) {
	ALERT_AT(where, "WDT feed error 0x%x", (int)e);
      }
#endif
#endif
  }

  static void reboot(String reason="", bool immediate=false);
  void install_taps(String target);
  void tap(String publisher, String alias, String type="");
  Leaf *tap_type(String type, int level=L_DEBUG);
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

  void describe_taps(int l=L_DEBUG);
  void describe_output_taps(int l=L_DEBUG);

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
  byte getBytePref(String key, byte default_value, String description="");
  bool getBytePref(String key, byte *value, String description="");
  unsigned long getULongPref(String key, unsigned long default_value, String description="");
  bool getULongPref(String key, unsigned long *value, String description="");
  virtual bool parseBool(String value, bool default_value=false, bool *valid_r=NULL);

  bool getBoolPref(String key, bool *value, String description="");
  bool getBoolPref(String key, bool default_value, String description="");
  float getFloatPref(String key, float default_value,String description="");
  bool getFloatPref(String key, float *value,String description="");
  double getDoublePref(String key, double default_value,String description="");
  bool getDoublePref(String key, double *value, String description="");
  void setPref(String key, String value);
  void setBoolPref(String key, bool value);
  void setBytePref(String key, byte value);
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


  bool setup_done = false;
  bool started = false;
  bool run = true;
  bool inhibit_start = false;
  bool impersonate_backplane = false;
  const char *TAG=NULL;
  String leaf_type="";
  String base_topic="";
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

Leaf::Leaf(String t, String name, pinmask_t pins, String target)
  : Debuggable(name)
{
  LEAF_ENTER_STR(L_INFO, String(__FILE__));
  this->leaf_type = t;
  this->tap_targets = target;
  this->pin_mask = pins;
  TAG = leaf_name.c_str();
  taps = new SimpleMap<String,Tap*>(_compareStringKeys);
  tap_sources = new SimpleMap<String,Leaf*>(_compareStringKeys);
  cmd_descriptions = new SimpleMap<String,String>(_compareStringKeys);
  value_descriptions = new SimpleMap<String,Value *>(_compareStringKeys);
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
    NOTICE("Entering separate loop for %s\n", leaf->describe().c_str());
    while (leaf->canRun()) {
      leaf->loop();
    }
    NOTICE("Exiting separate loop for %s\n", leaf->describe().c_str());
  }
}
#endif

void Leaf::start(void)
{
#if DEBUG_LINES
  LEAF_ENTER(L_DEBUG)
#else
  LEAF_ENTER_STR(L_INFO,String(__FILE__));
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
    esp_err_t err;
    BaseType_t res;
    
    LEAF_WARN("    Creating separate loop task for %s", describe().c_str());
    snprintf(task_name, sizeof(task_name), "%s_loop", leaf_name.c_str());
    res = xTaskCreate(
      &leaf_own_loop,      // task code
      task_name,           // task_name
      loop_stack_size,     // stack depth
      this,                // parameters
      1,                   // priority
      &leaf_loop_handle   // task handle
      //,ARDUINO_RUNNING_CORE // core id
      );
    if (res != pdPASS) {
      LEAF_ALERT("Task create failed (0x%x)", (int)res);
    }
    else {
#if USE_WDT
      WARN("    Subscribing %s to task WDT", task_name);
      err = esp_task_wdt_add(leaf_loop_handle);
      if (err != ESP_OK) {
	LEAF_ALERT("Task WDT install failed (0x%x)", (int)err);
      }
#endif // USE_WDT
    }
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

void Leaf::reboot(String reason, bool immediate)
{
  int leaf_index;
  ALERT("reboot reason=%s%s", reason.c_str(),immediate?" IMMEDIATE":"");
  if (!immediate) {
    for (leaf_index=0; leaves[leaf_index]; leaf_index++);
    for (leaf_index--; leaf_index>=0; leaf_index--) {
      leaves[leaf_index]->pre_reboot(reason);
    }
  }
  debug_flush = true;
  ACTION("REBOOT");
  delay(2000);

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

String Leaf::makeBaseTopic()
{
  String new_base_topic;

  LEAF_ENTER(L_DEBUG);
  if (!pubsubLeaf || pubsubLeaf->pubsubUseDeviceTopic()) {
    // if pubsubLeaf is null (due to app-selected transport) may guess wrong here

    if (impersonate_backplane) {
      //LEAF_DEBUG("Leaf %s will impersonate the backplane", leaf_name.c_str());
      new_base_topic = _ROOT_TOPIC + "devices/" + device_id + String("/");
    } else {
      //LEAF_DEBUG("Leaf %s uses a device-type based topic", leaf_name.c_str());
      new_base_topic = _ROOT_TOPIC + "devices/" + device_id + String("/") + leaf_type + String("/") + leaf_name + String("/");
    }
  }
  else {
    //LEAF_DEBUG("Leaf %s uses a device-id based topic", leaf_name.c_str());
    new_base_topic = _ROOT_TOPIC + device_id + String("/");
    if (hasUnit()) {
      new_base_topic = new_base_topic + leaf_unit + "/";
    }
  }
  if (new_base_topic != base_topic) {
    if (base_topic.length()) {
      //LEAF_INFO("Change of base_topic for %s: [%s] => [%s]", getNameStr(), base_topic.c_str(), new_base_topic.c_str());
    }
    base_topic = new_base_topic;
  }

  LEAF_STR_RETURN(base_topic);
}

void Leaf::setup(void)
{
  ACTION("SETUP %s", leaf_name.c_str());
  LEAF_ENTER_STR(L_DEBUG, String(__FILE__));
  //LEAF_INFO("Tap targets for %s are %s", describe().c_str(), tap_targets.c_str());

  // Find and save a pointer to the default IP and PubSub leaves, if any.   This relies on
  // these leaves being declared before any of their users.
  //
  // Note that prefs,ip,pubsub are not full taps (they don't automatically consume all publishes from the source)
  //
  // Note: don't try and tap yourself!
  //LEAF_DEBUG("Install standard taps");
  if (leaf_name == "prefs") {
    prefsLeaf = (StorageLeaf *)this;
  }
  else {
    //LEAF_DEBUG("tap storage");
    prefsLeaf = (StorageLeaf *)find_type("storage");
    if (prefsLeaf == NULL) {
      LEAF_INFO("Did not find any active prefs leaf");
    }
  }

  if (ipLeaf == NULL) {
    AbstractIpLeaf *ip = (AbstractIpLeaf *)find_type("ip");
    if (ip == NULL) {
      LEAF_INFO("Did not find any active IP leaf");
    }
    else {
      //LEAF_INFO("Using ipLeaf %s", ip->describe().c_str());
      ipLeaf = ip;
    }
  }

  if (pubsubLeaf == NULL) {
    AbstractPubsubLeaf *pubsub = (AbstractPubsubLeaf *)find_type("pubsub");
    if (pubsub == NULL) {
      LEAF_INFO("Did not find any active pubsub leaf");
    }
    else {
      //LEAF_INFO("Using pubsubLeaf %s", pubsub->describe().c_str());
      pubsubLeaf = pubsub;
    }
  }
  if (tap_targets.length()) {
    //LEAF_DEBUG("Install declared taps %s", tap_targets.c_str());
    install_taps(tap_targets);
  }

  //LEAF_DEBUG("Configure mqtt behaviour");
  makeBaseTopic();

  //LEAF_DEBUG("Configure pins");
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

  // these are all "unlisted" values/commands, to not clutter up the help tables
  registerLeafValue(HERE, "do_status", VALUE_KIND_BOOL, &do_status);
  registerLeafValue(HERE, "mute", VALUE_KIND_BOOL, &leaf_mute);
  registerLeafValue(HERE, "debug_level", VALUE_KIND_INT, &class_debug_level);
  if (do_status) {
    registerCommand(HERE, "status");
  }

  setup_done = true;
  LEAF_LEAVE;
}

void Leaf::registerCommand(codepoint_t where,String cmd, String description)
{
  LEAF_ENTER_STR(L_DEBUG, cmd);
  if (description.length() > 0) {
    LEAF_INFO("Register command %s::%s (%s)", getNameStr(), cmd.c_str(), description.c_str());
  }
  else {
    LEAF_DEBUG("Register command %s::%s  (unlisted)", getNameStr(), cmd.c_str());
  }
#if !STACX_USE_HELP
  description = ""; // save RAM
#endif
  cmd_descriptions->put(cmd, description);
  LEAF_LEAVE;
}

void Leaf::registerLeafCommand(codepoint_t where, String cmd, String description)
{
  registerCommand(CODEPOINT(where), leaf_name+"_"+cmd, description);
}

bool Leaf::loadValues()
{
  LEAF_ENTER(L_INFO);
  int count = value_descriptions->size();
  String name;
  Value *val = NULL;
  bool changed = false;

  //LEAF_INFO("Leaf %s has %d preferences", getNameStr(), count);
  for (int i=0; i < count; i++) {
    name = value_descriptions->getKey(i);
    val = value_descriptions->getData(i);
    if (val && val->value) {
      changed |= loadValue(name, val);
    }
  }
  return changed;
}

bool Leaf::loadValue(String name, Value *val)
{
  bool changed = false;
  if (!val) return changed;

  if (val->hasSetter()) {
    // This Value uses a custom setter callback
    String payload = prefsLeaf->get(name, "");
    changed = val->setter(this, name, val, payload);
  }
  else {
    // this Value uses the normal valueChangeHandler mechanism
    switch (val->kind) {
    case VALUE_KIND_BOOL: {
      bool was = VALUE_AS(bool, val);
      getBoolPref(name, (bool *)(val->value));
      changed = (VALUE_AS(bool, val) != was);
      break;
    }
    case VALUE_KIND_BYTE: {
      byte was = VALUE_AS(byte, val);
      getBytePref(name, (byte *)(val->value));
      changed = (VALUE_AS(byte, val) != was);
      break;
    }
    case VALUE_KIND_INT: {
      int was = VALUE_AS(int, val);
      getIntPref(name, (int *)(val->value));
      changed = (VALUE_AS(int, val) != was);
      break;
    }
    case VALUE_KIND_ULONG: {
      unsigned long was = VALUE_AS(unsigned long, val);
      getULongPref(name, (unsigned long *)(val->value));
      changed = (VALUE_AS(unsigned long, val) != was);
      break;
    }
    case VALUE_KIND_FLOAT: {
      float was = VALUE_AS(float, val);
      getFloatPref(name, (float *)(val->value));
      changed = (VALUE_AS(float, val) != was);
      break;
    }
    case VALUE_KIND_DOUBLE:
    {
      double was = VALUE_AS(double, val);
      getDoublePref(name, (double *)(val->value));
      changed = (VALUE_AS(double, val) != was);
      break;
    }
    case VALUE_KIND_STR:
    {
      String was = VALUE_AS(String, val);
      getPref(name, (String *)(val->value));
      changed = (VALUE_AS(String, val) != was);
    }
    } // end switch

  } // end if custom/normal setter

  if (changed) {
    LEAF_NOTICE("Setting %s::%s <= %s", getNameStr(), name.c_str(), val->asString());
    valueChangeHandler(name, val);
  }
  return changed;
}



void Leaf::registerValue(codepoint_t where, String name, enum leaf_value_kind kind, void *value, String description, enum leaf_value_acl acl, bool save, value_setter_t setter)
{
  LEAF_ENTER_STR(L_DEBUG, name);

  Value *val = new Value(kind, value, acl, save);

  if (!val) {
    LEAF_ALERT("Allocation failed");
    return;
  }
  bool unlisted = (description=="");
#if STACX_USE_HELP
  val->description = description;
#else
  val->description = ""; // save RAM
#endif
  val->setter = setter;
  value_descriptions->put(name, val);

  if (unlisted) {
    LEAF_DEBUG("Register setting %s::%s %s %s: (unlisted) dfl=%s",
	       getNameStr(), name.c_str(),
	       val->getAcl(), val->kindName(),
	       val->asString().c_str());
  }
  else {
    LEAF_INFO("Register setting %s::%s %s %s (%s) dfl=%s",
	      getNameStr(), name.c_str(),
	      val->getAcl(), val->kindName(),
	      description.c_str(), val->asString().c_str()
      );
  }

  if (save && val && val->value) {
    loadValue(name, val);
    return;
  }

  LEAF_LEAVE;
}

void Leaf::registerLeafValue(codepoint_t where, String name, enum leaf_value_kind kind, void *value, String description, enum leaf_value_acl acl, bool save, value_setter_t setter)
{
  registerValue(CODEPOINT(where), leaf_name+"_"+name, kind, value, description, acl, save, setter);
}

bool Leaf::setValue(String topic, String payload, bool direct, bool allow_save, bool override_perms, bool *changed_r)
{
  LEAF_ENTER_STR(L_INFO, topic);

  bool handled = false;
  Value *val = NULL;
  bool matched = value_descriptions->has(topic);
  String pref_name = topic;

  if (matched) {
    val = value_descriptions->get(topic);
  }
  else if (direct) {
    // when doing direct inter leaf messaging, permit shortcuts such as cmd/foo instead of cmd/NAMEOFLEAF_foo
    String leaf_topic = getName()+"_"+topic;
    if (value_descriptions->has(leaf_topic)) {
      // we can handle set/NAMEOFLEAF_foo and we were given set/foo, which is considered a match
      //LEAF_INFO("Did fuzzy match for %s =~ %s", leaf_topic.c_str(), topic.c_str());
      matched=true;
      val = value_descriptions->get(leaf_topic);
      pref_name = leaf_topic;
    }
  }

  if (!matched) {
    LEAF_TRACE("There is no registered value matching [%s]", topic.c_str());
    LEAF_BOOL_RETURN(false);
  }
  if (!(val->canSet() || override_perms)) {
    LEAF_WARN("Value %s is not writable", topic.c_str());
    LEAF_BOOL_RETURN(false);
  }
  bool do_save = allow_save && val->save;
  bool changed = (val->value == NULL); // non-persistent settings are "always changed"

  if (val->hasSetter()) {
    val->setter(this, topic, val, payload);
  }
  else {
    switch (val->kind) {
    case VALUE_KIND_BOOL: {
      bool boolVal = parseBool(payload, VALUE_AS(bool, val));
      if (boolVal != VALUE_AS(bool, val)) {
	changed = true;
	if (val->value) VALUE_AS(bool, val) = boolVal;
	if (do_save) setBoolPref(pref_name, VALUE_AS(bool, val));
      }
    }
      break;
    case VALUE_KIND_BYTE: {
      byte byteVal = payload.toInt();
      if (byteVal != VALUE_AS(byte, val)) {
	changed = true;
	if (val->value) VALUE_AS(byte,val) = byteVal;
	if (do_save) setBytePref(pref_name, VALUE_AS(byte,val));
      }
    }
      break;
    case VALUE_KIND_INT: {
      int intVal = payload.toInt();
      if (intVal != VALUE_AS(int, val)) {
	changed = true;
	if (val->value) *(int *)val->value = intVal;
	if (do_save) setIntPref(pref_name, *(int *)val->value);
      }
    }
      break;
    case VALUE_KIND_ULONG: {
      unsigned long ulVal = payload.toInt();
      if (ulVal != *(unsigned long *)val->value) {
	changed = true;
	if (val->value) *(unsigned long *)val->value = ulVal;
	if (do_save) setULongPref(pref_name, *(unsigned long *)val->value);
      }
    }
      break;
    case VALUE_KIND_FLOAT: {
      float floatVal = payload.toFloat();
      if (floatVal != *(float *)val->value) {
	changed = true;
	if (val->value) *(float *)val->value = floatVal;
	if (do_save) setFloatPref(pref_name, *(float *)val->value);
      }
    }
      break;
    case VALUE_KIND_DOUBLE: {
      double doubleVal = payload.toDouble();
      if (doubleVal != *(double *)val->value) {
	changed = true;
	if (val->value) *(double *)val->value = doubleVal;
	if (do_save) setDoublePref(pref_name, *(double *)val->value);
      }
    }
      break;
    case VALUE_KIND_STR: {
      if (payload != *(String *)val->value) {
	changed = true;
	if (val->value) *((String *)val->value) = payload;
	if (do_save) setPref(pref_name, *((String *)val->value));
      }
    }
    } // end switch
    if (changed) {
      LEAF_NOTICE("New value for %s::%s <= %s", getNameStr(), topic.c_str(), val->asString().c_str());
      this->valueChangeHandler(topic, val);
    }
    if (changed_r) *changed_r = changed;
  } // end if custom/default setter
  LEAF_BOOL_RETURN(true);
}

bool Leaf::getValue(String topic, String payload, Value **val_r, bool direct)
{
  LEAF_ENTER_STR(L_INFO, topic);

  bool matched = value_descriptions->has(topic);
  Value *val = NULL;

  if (matched) {
    val = value_descriptions->get(topic);
  }
  else if (direct) {
    // when doing direct inter leaf messaging, permit shortcuts such as cmd/foo instead of cmd/NAMEOFLEAF_foo
    String leaf_topic = getName()+"_"+topic;
    if (value_descriptions->has(leaf_topic)) {
      // we can handle set/NAMEOFLEAF_foo and we were given set/foo, which is considered a match
      //LEAF_INFO("Did fuzzy match for %s =~ %s", leaf_topic.c_str(), topic.c_str());
      matched=true;
      val = value_descriptions->get(leaf_topic);
    }
  }

  if (!val) {
    LEAF_TRACE("There is no registered value named [%s]", topic.c_str());
    LEAF_BOOL_RETURN(false);
  }
  if (!val->canGet()) {
    LEAF_WARN("Value [%s] is not readable", topic.c_str());
    LEAF_BOOL_RETURN(false);
  }

  if (val_r) {
    *val_r = val;
  }
  LEAF_BOOL_RETURN(true);
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

  if (hasOwnLoop()) {
    Leaf::wdtReset(HERE);
  }

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
  LEAF_ENTER_STR(L_DEBUG, topic);

  if (((topic=="cmd/help") || (topic=="cmd/help_all")) && hasHelp()) {
    // this module offers help
    LEAF_BOOL_RETURN(true);
  }

  else if (cmd_descriptions && topic.startsWith("cmd/")) {
    bool wanted = false;
    int word_end;
    if ((word_end=topic.lastIndexOf('/')) > 4) {
      // topic has a second slash eg cmd/do/thing (here word_end will be 6)
      //                             0123456789abc
      //
      // we look for "do/" in the command table, indincating that this command accepts arguments
      //
      if (cmd_descriptions->has(topic.substring(4,word_end-3))) LEAF_BOOL_RETURN(true);
    }
    else {
      // topic is a simple word eg cmd/xyzzy, we just look for "xyzzy" in the command table
      //
      if (cmd_descriptions->has(topic.substring(4))) LEAF_BOOL_RETURN(true); // this is a registered command
    }
  }
  else if (value_descriptions && (topic.startsWith("get/") || topic.startsWith("set/"))) {
    bool isSet = topic.startsWith("set/");
    bool matched = value_descriptions->has(topic.substring(4));

    if (matched) {
      Value *val = value_descriptions->get(topic.substring(4));
      if (val) {
	// Return true if topic is a valid get or set operation for a respectively gettable or settable value
	LEAF_BOOL_RETURN( (isSet && val->canSet()) || (!isSet && val->canGet()) );
      }
    }
  }
  LEAF_BOOL_RETURN((type=="*" || type == leaf_type) && (name=="*" || name == leaf_name)); // this message addresses this leaf

  // subclasses should handle other wants, and also call this parent method
}

String Leaf::getValueHelp(String name, Value *val)
  {
    String help = "{\"name\": \""+name+"\"";
    help += ",\"kind\": \""+String(val->kindName())+"\"";
    if (val->hasHelp()) {
      help += ",\"desc\": \""+val->description+"\"";
    }
    if (val->default_value.length()) {
      help += ",\"default\":\""+val->default_value+"\"";
    }
    if (val->value) {
      String current = val->asString();
      if (current != val->default_value) {
	help += ",\"current\":\""+val->asString()+"\"";
      }
    }
    help += ",\"acl\":\""+String(val->getAcl())+"\"";
    help += ",\"from\":\""+describe()+"\"}";
    return help;
  }


bool Leaf::mqtt_receive(String type, String name, String topic, String payload, bool direct)
{
  LEAF_ENTER(L_DEBUG);
  LEAF_INFO("Message for %s::%s as %s: [%s] <= [%s]", base_topic.c_str(), getNameStr(), name.c_str(), topic.c_str(), payload.c_str());
  bool handled = false;
  String key;
  String desc;
  Value *val = NULL;

  WHEN("set/debug_level", {
      int l = payload.toInt();
      LEAF_NOTICE("Set debug level for %s to %d", describe().c_str(), l);
      setDebugLevel(l);
    })
  ELSEWHENEITHER("cmd/help", "cmd/help_all", {
      Value *val = value_descriptions->get(topic);
      bool show_all = false;
      bool all_kinds = true;
      String filter = "";
      String kind = "";
      int count;

      // some user interfaces pass a default "1" when no payload is entered, scrub that
      if (payload == "1") payload=""; // 1 means same as "all"

      if (topic == "cmd/help") {
	// The normal help command accepts arguments of the form help [type] [filter]
	//
	int space_pos = payload.indexOf(' ');
	if (space_pos>0) {
	  // payload has a kind and a filter, eg "set lte"
	  filter = payload.substring(space_pos+1);
	  kind = payload.substring(0,space_pos);
	  all_kinds = false;
	}
	else if (payload.length()) {
	  kind = payload;
	  all_kinds = false;
	}
      }
      else {
	// The cmd "help_all" means show all items, including "unlisted" (no description).
	// This command does not take a type payload, only a filter. 
	show_all = true;
	filter = payload;
	topic="cmd/help";
      }
      LEAF_DEBUG("Help paramters show_all=%s kind=[%s] filter=[%s]",
		  TRUTH(show_all), kind.c_str(), filter.c_str());

      if (all_kinds || (kind=="cmd")) {
	count = cmd_descriptions->size();
	//LEAF_INFO("Leaf %s has %d commands", getNameStr(), count);
	for (int i=0; i < count; i++) {
	  key = cmd_descriptions->getKey(i);
	  if (filter.length() && (key.indexOf(filter)<0)) continue;
	  desc = cmd_descriptions->getData(i);
	  if ((desc.length() > 0) || show_all) {
	    String help = "{\"name\":\""+key+"\"";
	    if (desc.length()>0) {
	      help+=",\"desc\":\""+desc+"\"";
	    }
	    help += ",\"from\":\""+describe()+"\"}";
	    mqtt_publish("help/cmd/"+key, help, 0, false, L_INFO, HERE);
	  }
	  else {
	    //LEAF_INFO("suppress silent command %s", key.c_str());
	  }
	}
      }

      // not an else-case
      if (all_kinds || (kind == "setting")) {
	count = value_descriptions->size();
	//LEAF_INFO("Leaf %s has %d settings", getNameStr(), count);
	for (int i=0; i < count; i++) {
	  key = value_descriptions->getKey(i);
	  if (filter.length() && (key.indexOf(filter)<0)) continue;
	  val = value_descriptions->getData(i);
	  if (show_all || val->hasHelp()) {
	    mqtt_publish("help/set/"+key, getValueHelp(key, val), 0, false, L_INFO, HERE);
	  }
	}
      }

      // not an else-case
      if (kind=="set") {
	for (int i=0; i < value_descriptions->size(); i++) {
	  key = value_descriptions->getKey(i);
	  if (filter.length() && (key.indexOf(filter)<0)) continue;
	  val = value_descriptions->getData(i);
	  if (val->canSet() && (val->description.length()>0)) {
	    mqtt_publish("help/set/"+key, getValueHelp(key, val), 0, false, L_INFO, HERE);
	  }
	}
      }
      else if (kind=="get") {
	for (int i=0; i < value_descriptions->size(); i++) {
	  key = value_descriptions->getKey(i);
	  if (filter.length() && (key.indexOf(filter)<0)) continue;
	  val = value_descriptions->getData(i);
	  if (val->canGet() && val->hasHelp()) {
	    mqtt_publish("help/get/"+key, getValueHelp(key, val), 0, false, L_INFO, HERE);
	  }
	}
      }
    })
  ELSEWHEN("cmd/config",{
      this->config_pub();
    })
  ELSEWHEN("cmd/stats",{
      this->stats_pub();
    })
  ELSEWHENEITHER("cmd/status","get/status", {
      if (this->do_status || payload.toInt()) {
	LEAF_NOTICE("Responding to cmd/status");
	this->status_pub();
      }
    })
  ELSEWHEN("cmd/leafstatus",{
      char top[80];
      char msg[80];
      snprintf(top, sizeof(top), "status/leaf/%s", describe().c_str());
      snprintf(msg, sizeof(msg), "canRun=%s started=%s ip=%s pubsub=%s",
	       TRUTH_lc(canRun()), TRUTH_lc(isStarted()), ipLeaf?ipLeaf->describe().c_str():"none", pubsubLeaf?pubsubLeaf->describe().c_str():"none");
      mqtt_publish(top, msg);
    })
  ELSEWHEN("cmd/taps",{
      describe_taps();
      describe_output_taps();
    })
  ELSEWHENPREFIX("cmd/", {
      bool has_handler = false;
      int word_end;

      word_end=topic.lastIndexOf('/');
      if (word_end > 0) {
	String topic_leader = topic.substring(0,word_end+1);
	// command topic contains one more slashes eg do/thing or do/other/thing
	//
	// we look for "do/" or "do/+" in the command table, indicating that this command accepts arguments
	//
	if (cmd_descriptions->has(topic_leader)) {
	  has_handler=true;
	}
	else if (cmd_descriptions->has(topic_leader+"+")) {
	  has_handler=true;
	}
      }
      //
      // check for a handled topic of topic that's an exact match eg for cmd/xyzzy, we just look for "xyzzy" in the command table
      //
      if (!has_handler && cmd_descriptions->has(topic)) {
	has_handler = true;
      }

      if (has_handler) {
	LEAF_INFO("Invoke command handler for %s", topic.c_str());
	handled = this->commandHandler(type, name, topic, payload);
      }
      else {
	LEAF_INFO("Unhandled command topic", topic.c_str());
      }
  })
  ELSEWHENPREFIX("set/", {
      LEAF_INFO("Invoke set handler for %s", topic.c_str());
      handled = setValue(topic, payload, direct);
    })
  ELSEWHENPREFIX("get/", {
      handled=false;
      val = NULL;
      LEAF_INFO("Invoke get handler for %s", topic.c_str());
      handled = getValue(topic, payload, &val);
      if (handled && val) {
	mqtt_publish("status/"+topic, val->asString());
      }

    });

  if (!handled && !topic.startsWith("status/")) {
    LEAF_INFO("Topic [%s] was not handled", topic.c_str());
  }

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
    target->mqtt_receive(this->leaf_type, this->leaf_name, topic, payload, true);
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
  LEAF_ENTER_STR(L_DEBUG, topic);

  // Send the publish to any leaves that have "tapped" into this leaf
  for (int t = 0; t < this->taps->size(); t++) {
    String target_name = this->taps->getKey(t);
    Tap *tap = this->taps->getData(t);
    Leaf *target = tap->target;
    String alias = tap->alias;

    if (topic.startsWith("_")) {
      level++; // lower the priority of internal topics
    }
    __LEAF_DEBUG_AT__(CODEPOINT(where), level, "TPUB %s(%s) => %s %s %s",
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

  if (pubsubLeaf == NULL) {
    LEAF_WARN("No pubusb leaf with which to subscribe");
    return;
  }
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
  LEAF_ENTER_STR(L_INFO, topic);

  bool is_muted = leaf_mute;
  if (is_muted && topic.startsWith("help/")) {
    //LEAF_TRACE("Supress mute for help message");
    is_muted=false;
  }

  // Publish to the MQTT server (unless this leaf is "muted", i.e. performs local publish only)
  if (pubsubLeaf) {
    if (::pubsub_loopback) {
      // don't actually publish, capture output in a buffer
      pubsubLeaf->sendLoopback(topic, payload);
    }
    else if (is_muted) {
      // do nothing
    }
    else if (topic.startsWith("status/") && !use_status) {
      LEAF_NOTICE("Status publish disabled for %s", topic.c_str());
    }
    else if (topic.startsWith("event/") && !use_event) {
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

    if (::pubsub_loopback) {
      // this message won't be sent to MQTT, but record it for shell
      Serial.printf("PUB %s %s\n", topic.c_str(), payload.c_str());
    }
  }

  // Send the publish to any leaves that have "tapped" into this leaf
  // (except short-circuit help strings, which don't need to be spammed around)
  if (!topic.startsWith("help")) {
    publish(topic, payload, level, CODEPOINT(where));
  }

  LEAF_LEAVE;
}

extern Leaf *leaves[]; // you should define and null-terminate this array in your main.ino

Leaf *Leaf::find(String find_name, String find_type)
{
  Leaf *result = NULL;
  //LEAF_ENTER(L_DEBUG);

  // Find a leaf with a given name, and return a pointer to it
  for (int s=0; leaves[s]; s++) {
    if ((find_name=="") || (leaves[s]->leaf_name == find_name)) {
      if ((find_type=="") || (leaves[s]->leaf_type == find_type)) {
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

  // Find an enabled leaf with a given type, and return a pointer to it
  for (int s=0; leaves[s]; s++) {
    if ((leaves[s]->leaf_type == find_type) && leaves[s]->canRun()) {
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
  //LEAF_INFO("Leaf %s taps into %s@%s (as %s)",
  //	    this->leaf_name.c_str(),
  //	    (type=="")?"any":type.c_str(),
  //	    publisher.c_str(),
  //	    alias.c_str());

  Leaf *target = find(publisher, type);
  if (target) {
    target->add_tap(alias, this);
    this->tap_sources->put(alias, target);
  }
  else {
    LEAF_WARN("Did not find tap %s/%s", type.c_str(),publisher.c_str());
  }

  //LEAF_LEAVE;
}

Leaf * Leaf::tap_type(String type, int level)
{
  LEAF_ENTER_STR(level, type);

  Leaf *target = find_type(type);
  if (target) {
    __LEAF_DEBUG__(level,"Leaf [%s] taps [%s]", this->describe().c_str(), target->describe().c_str());
    target->add_tap(leaf_name, this);
    this->tap_sources->put(target->getName(), target);
  }
  else {
    __LEAF_DEBUG__(level, "No match");
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

void Leaf::describe_taps(int l)
{
  LEAF_ENTER(L_DEBUG);
  __LEAF_DEBUG__(l, "Leaf %s has %d tap sources: ", this->leaf_name.c_str(), this->tap_sources->size());
  int source_count = this->tap_sources?this->tap_sources->size():0;
  for (int t = 0; t < this->tap_sources->size(); t++) {
    String alias = this->tap_sources->getKey(t);
    Leaf *target = this->tap_sources->getData(t);
    __LEAF_DEBUG__(l, "   Tap %s <= %s(%s)",
	   this->leaf_name.c_str(), target->leaf_name.c_str(), alias.c_str());
  }
  LEAF_LEAVE;
}

void Leaf::describe_output_taps(int l)
{
  LEAF_ENTER(L_DEBUG);
  __LEAF_DEBUG__(l, "Leaf %s has %d tap outputs: ", this->leaf_name.c_str(), this->taps->size());
  for (int t = 0; t < this->taps->size(); t++) {
    String target_name = this->taps->getKey(t);
    Tap *tap = this->taps->getData(t);
    String alias = tap->alias;
    __LEAF_DEBUG__(l, "   Tap %s => %s as %s",
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

byte Leaf::getBytePref(String key, byte default_value, String description)
{
  if (!prefsLeaf) {
    LEAF_ALERT("Cannot get %s, no preferences leaf", key.c_str());
    return default_value;
  }
  return prefsLeaf->getByte(key, default_value, description);
}

bool Leaf::getBytePref(String key, byte *value, String description)
{
  LEAF_ENTER_STR(L_DEBUG, key);
  if (!prefsLeaf) {
    LEAF_ALERT("Cannot get %s, no preferences leaf", key.c_str());
    LEAF_BOOL_RETURN(false);
  }
  if (value) {
    *value = prefsLeaf->getByte(key, *value, description);
  }
  LEAF_BOOL_RETURN(true);
}

unsigned long Leaf::getULongPref(String key, unsigned long default_value, String description)
{
  if (!prefsLeaf) {
    LEAF_ALERT("Cannot get %s, no preferences leaf", key.c_str());
    return default_value;
  }
  return prefsLeaf->getULong(key, default_value, description);
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

void Leaf::setBytePref(String key, byte value)
{
  if (!prefsLeaf) {
    LEAF_ALERT("Cannot save %s, no preferences leaf", key.c_str());
  }
  else {
    prefsLeaf->putByte(key, value);
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
