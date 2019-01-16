//@***************************** class GroundPod ******************************
//
// Set the nominated pin(s) as LOW outputs (allow them to be used as
// additional ground pins for external LEDs)

class GroundPod : public Pod
{
public:

  GroundPod(String name, pinmask_t pins) : Pod("ground", name, pins){
  }

  void setup(void) {
    Pod::setup();
    enable_pins_for_output();
    clear_pins();
  }

};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
