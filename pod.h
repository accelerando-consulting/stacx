
// Wemos d1 mini (esp8266) exposes gpios up to gpio16
// For ESP32 you may need to set max pin as high as 39
#if defined(ESP8266)
#define MAX_PIN 16
#define pinmask_t uint32_t
#else
#define MAX_PIN 39
#define pinmask_t uint64_t
#endif

#define POD_PIN(n) ((pinmask_t)1<<(pinmask_t)n)

#define FOR_PINS(block)  for (int pin = 0; pin <= MAX_PIN ; pin++) { pinmask_t mask = ((pinmask_t)1)<<(pinmask_t)pin; if (pin_mask & mask) block }

#define WHEN(topic_str, block) if (topic==topic_str) { handled=true; block; }
#define ELSEWHEN(topic_str, block) else WHEN(topic_str,block)

// 
// Forward delcarations for the MQTT base functions (TODO: make this a class)
// 
extern void _mqtt_publish(String topic, String payload, int qos=0, bool retain=false);

extern void _mqtt_subscribe(String topic);


//
//@******************************* class Pod *********************************
//
// An abstract superclass for all sensor/actuator types.
//
// Takes care of pin resources, naming, and heartbeat.
//
class Pod 
{
public: 
  Pod(String t, String name, pinmask_t pins);
  virtual void setup();
  virtual void loop();
  virtual void mqtt_connect();
  virtual void mqtt_subscribe() {};
  virtual void mqtt_disconnect() {};
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

  String pod_type;
  String pod_name;
  String base_topic;
  pinmask_t pin_mask;
  unsigned long last_heartbeat;
	
};

Pod::Pod(String t, String name, pinmask_t pins) 
{
  ENTER(L_INFO);
  pod_type = t;
  pod_name = name;
  pin_mask = pins;
  last_heartbeat = 0;
  base_topic = String("devices/") + pod_type + String("/") + pod_name ;
  LEAVE;
}

void Pod::setup(void) 
{
#if defined(ESP8266)
  INFO("Pin mask for %s is %08x", base_topic.c_str(), pin_mask);
#else
  INFO("Pin mask for %s is %08x%08x",
       base_topic.c_str(), (unsigned long)pin_mask>>32, (unsigned long)pin_mask);
#endif
}

void Pod::enable_pins_for_input(bool pullup) 
{
  FOR_PINS({
      INFO("%s claims pin %d as INPUT%s", base_topic.c_str(), pin, pullup?"_PULLUP":"");
      pinMode(pin, pullup?INPUT_PULLUP:INPUT);
    })
}

void Pod::enable_pins_for_output() 
{
  FOR_PINS({
      INFO("%s claims pin %d as OUTPUT", base_topic.c_str(), pin);
      pinMode(pin, OUTPUT);
    })
}

void Pod::set_pins() 
{
  FOR_PINS({digitalWrite(pin, HIGH);})
}

void Pod::clear_pins() 
{
  FOR_PINS({digitalWrite(pin, LOW);})
}

void Pod::loop() 
{
  unsigned long now = millis();
  
  if (now > (last_heartbeat + HEARTBEAT_INTERVAL_SECONDS*1000)) {
      last_heartbeat = now;
      mqtt_publish("status/heartbeat", String(now/1000, DEC));
    }

}

void Pod::mqtt_connect() 
{
  _mqtt_publish(base_topic, "online", true);
}

bool Pod::mqtt_receive(String type, String name, String topic, String payload) 
{
  INFO("Message for %s: %s <= %s", base_topic.c_str(), topic.c_str(), payload.c_str());
  bool handled = false;
  WHEN("cmd/status",status_pub());

  return true;
}

void Pod::mqtt_publish(String topic, String payload, int qos, bool retain) 
{
  _mqtt_publish(base_topic + "/" + topic, payload, qos, retain);
}

void Pod::mqtt_publish(String topic, String payload, bool retain) 
{
  mqtt_publish(topic, payload, 0, retain);
}

void Pod::mqtt_publish(String topic, const char * payload, bool retain) 
{
  mqtt_publish(topic, String(payload), 0, retain);
}

void Pod::mqtt_publish(const char *topic, String payload, bool retain) 
{
  mqtt_publish(String(topic), payload, 0, retain);
}

void Pod::mqtt_publish(const char *topic, const char *payload, bool retain) 
{
  mqtt_publish(String(topic), String(payload), 0, retain);
}

extern Pod *pods[]; // you must define and null-terminate this array in your program

  
// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
