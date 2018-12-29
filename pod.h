
#define POD_PIN(n) (((unsigned long long)1)<<n)

#define FOR_PINS(block)  for (int pin = 0; pin < 64 ; pin++) { if (pins & (1<<pin)) block }

// 
// Forward delcarations for the MQTT base functions (TODO: make this a class)
// 
extern bool _mqtt_publish(String topic, String payload, int qos=0, bool retain=false);


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
  Pod(String t, String name, unsigned long long pins);
  void setup() {} ;
  void loop();
  void mqtt_connect() {} ;
  void mqtt_subscribe() {} ;
  bool mqtt_receive(String type, String name, String topic, String payload);
  void mqtt_publish(String topic, String payload, int qos = 0, bool retain = false);
  String describe() { return base_topic; }
    
protected:
  void enable_pins_for_input(bool pullup=false);
  void enable_pins_for_output() ;

  String pod_type;
  String pod_name;
  String base_topic;
  unsigned long long pins;
  unsigned long last_heartbeat;
	
};

Pod::Pod(String t, String name, unsigned long long pins) 
{
  this->pod_type = t;
  this->pod_name = name;
  this->pins = pins;
  this->last_heartbeat = 0;

  this->base_topic = String("devices/") + pod_type + String("/") + pod_name + String("/");
}

void Pod::enable_pins_for_input(bool pullup) 
{
  FOR_PINS({pinMode(pin, pullup?INPUT_PULLUP:INPUT);})
}

void Pod::enable_pins_for_output() 
{
  FOR_PINS({pinMode(pin, OUTPUT);})
}

void Pod::loop() 
{
  unsigned long now = millis();
  
  if (now > (this->last_heartbeat + HEARTBEAT_INTERVAL)) {
      this->last_heartbeat = now;
      char msg[16];
      snprintf(msg, sizeof(msg), "%lu", now/1000);
      char heartbeat_topic[80];
      mqtt_publish("status/heartbeat", msg);
    }

}

bool Pod::mqtt_receive(String type, String name, String topic, String payload) 
{
  if ((this->pod_type != type) || (this->pod_name != name)) {
    // the message is not directed at this pod
    return false;
  }
  INFO("Message for %s: %s <= %s", base_topic.c_str(), topic.c_str(), payload.c_str());
  
  return true;
}

void Pod::mqtt_publish(String topic, String payload, int qos, bool retain) 
{
  _mqtt_publish(base_topic + "/" + topic, payload, qos, retain);
}

extern Pod *pods[]; // you must define and null-terminate this array in your program

//
//@**************************** class MotionPod ******************************
// 
// This class encapsulates a motion sensor that publishes to MQTT when it
// sees motion
// 

class MotionPod : public Pod
{
public:
  bool state;
 
  MotionPod(String name, unsigned long long pins) : Pod("motion", name, pins) {
    state = LOW;
  }

  void setup(void) {
    Pod::setup();
    enable_pins_for_input();
  }

  void loop(void) {
    Pod::loop();
    
    bool new_state = false;
    FOR_PINS({new_state |= digitalRead(pin);})

    if (new_state != state) {
      if (new_state) {
	mqtt_publish("motion", String((millis()/1000)));
      }
      state = new_state;
    }
  }

  bool mqtt_receive(String type, String name, String topic, String payload) {
    if (!Pod::mqtt_receive(type, name, topic, payload)) return false;  // ignore irrelevant messages

    // TODO process a message addressed to us
    
  }
    
};


//
//@**************************** class ExamplePod *****************************
// 
// You can copy this class to use as a boilerplate for new classes
// 

#if 0
class ExamplePod : Public Pod
{
  // 
  // Declare your pod-specific instance data here
  // 
  bool state;

  // 
  // Pod constructor method(s)
  // Call the superclass constructor to handle common arguments (type, name, pins)
  //
  MotionPod(String name, unsigned long long pins) {
    Pod::Pod("motion", name, pins);
    state = LOW;
  }

  //
  // Arduino Setup function
  // Use this to configure any IO resources
  //
  void setup(void) {
    Pod::setup();
    enable_pins_for_input();
  }

  // 
  // Arduino loop function
  // (Superclass function will take care of heartbeats)
  // 
  void loop(void) {
    Pod::loop();
    
    bool new_state = false;
    FOR_PINS({new_state |= digitalRead(pin);})

    if (new_state != state) {
      if (new_state) {
	mqtt_publish("motion", String((millis()/1000)));
      }
      state = new_state;
    }
  }

  // 
  // MQTT message callback
  // (Use the superclass callback to ignore messages not addressed to this pod)
  //
  bool mqtt_receive(String type, String name, String topic, String payload) {
    if (!Pod::mqtt_receive(type, name, topic, payload)) return false;  // ignore irrelevant messages

    // TODO process a message addressed to us
    
  }
    
};

#endif

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
