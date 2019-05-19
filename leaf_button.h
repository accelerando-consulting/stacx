//
//@**************************** class ButtonLeaf ******************************
// 
// This class encapsulates a simple pushbutton that publishes to MQTT when it
// changes state
//
#include <Bounce2.h>

class ButtonLeaf : public Leaf
{
public:
  Bounce button = Bounce(); // Instantiate a Bounce object

 
  ButtonLeaf(String name, pinmask_t pins) : Leaf("button", name, pins) {
    ENTER(L_INFO);
    LEAVE;
  }

  void setup(void) {
    ENTER(L_NOTICE);
    Leaf::setup();
    int buttonPin;
    FOR_PINS({buttonPin=pin;});
    INFO("%s claims pin %d as INPUT (debounced)", base_topic.c_str(), buttonPin);
    button.attach(buttonPin,INPUT_PULLUP); 
    button.interval(25); 
    LEAVE;
  }

  void status_pub() 
  {
    mqtt_publish("status/button", (button.read()==LOW)?"pressed":"released");
  }
  
  void loop(void) {
    Leaf::loop();
    button.update();
    bool changed = false;

    
    if (button.fell()) {
      mqtt_publish("event/press", String(millis(), DEC));
      changed = true;
    } else if (button.rose()) {
      mqtt_publish("event/release", String(millis(), DEC));
      changed = true;
    }
    if (changed) status_pub();
  }

};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
