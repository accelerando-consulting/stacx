//
//@**************************** class ExamplePod *****************************
// 
// You can copy this class to use as a boilerplate for new classes
// 

#if 0
class ExamplePod : public Pod
{
public:
  // 
  // Declare your pod-specific instance data here
  // 
  bool state;

  // 
  // Pod constructor method(s)
  // Call the superclass constructor to handle common arguments (type, name, pins)
  //
  MotionPod(String name, unsigned long long pins) : Pod("example", name, pins){
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
