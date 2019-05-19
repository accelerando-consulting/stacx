//
//@**************************** class ExampleLeaf *****************************
// 
// You can copy this class to use as a boilerplate for new classes
// 

#if 0
class ExampleLeaf : public Leaf
{
public:
  // 
  // Declare your leaf-specific instance data here
  // 
  bool state;

  // 
  // Leaf constructor method(s)
  // Call the superclass constructor to handle common arguments (type, name, pins)
  //
  MotionLeaf(String name, pinmask_t pins) : Leaf("example", name, pins){
    state = LOW;
  }

  //
  // Arduino Setup function
  // Use this to configure any IO resources
  //
  void setup(void) {
    Leaf::setup();
    enable_pins_for_input();
  }

  // 
  // Arduino loop function
  // (Superclass function will take care of heartbeats)
  // 
  void loop(void) {
    Leaf::loop();
    
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
  // (Use the superclass callback to ignore messages not addressed to this leaf)
  //
  bool mqtt_receive(String type, String name, String topic, String payload) {
    ENTER(L_INFO);
    bool handled = Leaf::mqtt_receive(type, name, topic, payload);

#if TODO
    WHEN("cmd/foo",{cmd_foo()})
    ELSEWHEN("set/other",{set_other(payload)});
#endif
    
    return handled;
  }
    
};

#endif

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
