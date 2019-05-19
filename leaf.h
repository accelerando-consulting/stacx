
// Wemos d1 mini (esp8266) exposes gpios up to gpio17 (aka A0)
// For ESP32 you may need to set max pin as high as 39
#if defined(ESP8266)
#define MAX_PIN 17
#define pinmask_t uint32_t
#else
#define MAX_PIN 39
#define pinmask_t uint64_t
#endif

#define LEAF_PIN(n) ((pinmask_t)1<<(pinmask_t)n)

#define FOR_PINS(block)  for (int pin = 0; pin <= MAX_PIN ; pin++) { pinmask_t mask = ((pinmask_t)1)<<(pinmask_t)pin; if (pin_mask & mask) block }

#define WHEN(topic_str, block) if (topic==topic_str) { handled=true; block; }
#define ELSEWHEN(topic_str, block) else WHEN(topic_str,block)

// 
// Forward delcarations for the MQTT base functions (TODO: make this a class)
// 
extern uint16_t _mqtt_publish(String topic, String payload, int qos=0, bool retain=false);

extern void _mqtt_subscribe(String topic);


//
//@******************************* class Leaf *********************************
//
// An abstract superclass for all sensor/actuator types.
//
// Takes care of pin resources, naming, and heartbeat.
//
class Leaf 
{
public: 

  Leaf(String t, String name, pinmask_t pins);
  virtual void setup();
  virtual void loop();
  virtual void mqtt_connect();
  virtual void mqtt_subscribe() {_mqtt_subscribe(base_topic+"/cmd/status"); };
  virtual void mqtt_disconnect() {};
  virtual bool wants_topic(String type, String name, String topic);
  virtual bool mqtt_receive(String type, String name, String topic, String payload);
  virtual void status_pub() {};

  void mqtt_publish(String topic, String payload, int qos = 0, bool retain = false);
  void mqtt_publish(String topic, String payload, bool retain = false);
  void mqtt_publish(String topic, const char *payload, bool retain = false);
  void mqtt_publish(const char *topic, String payload, bool retain = false);
  void mqtt_publish(const char *topic, const char *payload, bool retain = false);
  String describe() { return base_topic; }
    
protected:
  void enable_pins_for_input(bool pullup=false);
  void enable_pins_for_output() ;
  void set_pins();
  void clear_pins();

  bool impersonate_backplane;
  String leaf_type;
  String leaf_name;
  String base_topic;
  pinmask_t pin_mask;
  unsigned long last_heartbeat;
	
};

Leaf::Leaf(String t, String name, pinmask_t pins) 
{
  ENTER(L_INFO);
  leaf_type = t;
  leaf_name = name;
  pin_mask = pins;
  last_heartbeat = 0;
  impersonate_backplane = false;
  LEAVE;
}

void Leaf::setup(void) 
{
  if (impersonate_backplane) {
    base_topic = String("devices/backplane/") + device_id + String("/") + leaf_name ;;
  } else {
    base_topic = String("devices/") + leaf_type + String("/") + leaf_name ;
  }
#if defined(ESP8266)
  INFO("Pin mask for %s is %08x", base_topic.c_str(), pin_mask);
#else
  INFO("Pin mask for %s is %08x%08x",
       base_topic.c_str(), (unsigned long)pin_mask>>32, (unsigned long)pin_mask);
#endif
}

void Leaf::enable_pins_for_input(bool pullup) 
{
  FOR_PINS({
      INFO("%s claims pin %d as INPUT%s", base_topic.c_str(), pin, pullup?"_PULLUP":"");
      pinMode(pin, pullup?INPUT_PULLUP:INPUT);
    })
}

void Leaf::enable_pins_for_output() 
{
  FOR_PINS({
      INFO("%s claims pin %d as OUTPUT", base_topic.c_str(), pin);
      pinMode(pin, OUTPUT);
    })
}

void Leaf::set_pins() 
{
  FOR_PINS({
      DEBUG("%s sets pin %d HIGH", base_topic.c_str(), pin);
      digitalWrite(pin, HIGH);
    })
}

void Leaf::clear_pins() 
{
  FOR_PINS({
      DEBUG("%s sets pin %d LOW", base_topic.c_str(), pin);
      digitalWrite(pin, LOW);
    })
}

void Leaf::loop() 
{
  unsigned long now = millis();
  
  if (now > (last_heartbeat + HEARTBEAT_INTERVAL_SECONDS*1000)) {
      last_heartbeat = now;
      mqtt_publish("status/heartbeat", String(now/1000, DEC));
    }

}

void Leaf::mqtt_connect() 
{
  _mqtt_publish(base_topic, "online", true);
}

bool Leaf::wants_topic(String type, String name, String topic) 
{
  return ((type=="*" || type == leaf_type) && (name=="*" || name == leaf_name));
}

bool Leaf::mqtt_receive(String type, String name, String topic, String payload) 
{
  INFO("Message for %s: %s <= %s", base_topic.c_str(), topic.c_str(), payload.c_str());
  bool handled = false;
  WHEN("cmd/status",status_pub());
  return handled;
}

void Leaf::mqtt_publish(String topic, String payload, int qos, bool retain) 
{
  _mqtt_publish(base_topic + "/" + topic, payload, qos, retain);
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

extern Leaf *leaves[]; // you must define and null-terminate this array in your leaves.h

  
// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
