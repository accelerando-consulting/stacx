//@***************************** class GroundPod ******************************
//
// Set the nominated pin(s) as LOW outputs (allow them to be used as
// additional ground pins for external LEDs)

class GroundPod : public Pod
{
public:
  bool state;
  
  GroundPod(String name, pinmask_t pins, bool pin_state = LOW) : Pod("ground", name, pins){
    state = pin_state;
  }

  void setup(void) {
    Pod::setup();
    enable_pins_for_output();
    if (state) {
      set_pins();
    }
    else {
      clear_pins();
    }
  }

};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
 
