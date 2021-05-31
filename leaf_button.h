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
  int active = LOW;

  ButtonLeaf(String name, pinmask_t pins, int active=LOW) : Leaf("button", name, pins) {
    LEAF_ENTER(L_INFO);
    this->active = active;
    this->do_heartbeat = false;
    LEAF_LEAVE;
  }

  virtual void setup(void) {
    LEAF_ENTER(L_NOTICE);
    Leaf::setup();
    int buttonPin=-1;
    FOR_PINS({buttonPin=pin;});
    LEAF_INFO("%s claims pin %d as INPUT (debounced)", base_topic.c_str(), buttonPin);
    button.attach(buttonPin,INPUT_PULLUP);
    button.interval(25);
    LEAF_LEAVE;
  }

  virtual void status_pub()
  {
    mqtt_publish("status/button", (button.read()==active)?"pressed":"released");
  }

  virtual void loop(void) {
    Leaf::loop();
    button.update();
    bool changed = false;


    if ((active==LOW)?button.fell():button.rose()) {
      mqtt_publish("event/press", String(millis(), DEC));
      changed = true;
    }
    if ((active==LOW)?button.rose():button.fell()) {
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
