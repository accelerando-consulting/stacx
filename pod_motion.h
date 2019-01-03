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
 
  MotionPod(String name, pinmask_t pins) : Pod("motion", name, pins) {
    ENTER(L_INFO);
    state = LOW;
    LEAVE;
  }

  void setup(void) {
    ENTER(L_NOTICE);
    Pod::setup();
    enable_pins_for_input();
    LEAVE;
  }

  void mqtt_subscribe() {
    ENTER(L_NOTICE);
    _mqtt_subscribe(base_topic+"/cmd/status");
    LEAVE;
  }
  
  bool mqtt_receive(String type, String name, String topic, String payload) {
    if (!Pod::mqtt_receive(type, name, topic, payload)) return false;
    ENTER(L_DEBUG);
    bool handled = false;
    
    WHEN("cmd/status",{
      INFO("Refreshing device status");
      mqtt_publish("status/motion", TRUTH(state));
    });

    LEAVE;
    return handled;
  };
    
  void loop(void) {
    Pod::loop();
    
    bool new_state = false;
    // For sensors with multiple pins, essentially do a wired-OR
    FOR_PINS({
	bool pin_state = digitalRead(pin);
	new_state |= pin_state;
      })

    if (new_state != state) {
      if (new_state) {
	mqtt_publish("event/motion", String((millis()/1000)));
      }
      state = new_state;
    }
    //LEAVE;
  }

};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
