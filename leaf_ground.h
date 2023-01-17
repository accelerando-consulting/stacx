//@***************************** class GroundLeaf ******************************
//
// Set the nominated pin(s) as LOW outputs (allow them to be used as
// additional ground pins for external LEDs)

class GroundLeaf : public Leaf
{
public:
  bool state;
  
  GroundLeaf(String name, pinmask_t pins, bool pin_state = LOW)
    : Leaf("ground", name, pins)
    , Debuggable(name)
  {
    state = pin_state;
  }

  void setup(void) {
    Leaf::setup();
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
 
