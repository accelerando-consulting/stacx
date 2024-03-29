//
//@**************************** class ButtonLeaf ******************************
//
// This class encapsulates a simple pushbutton that publishes to MQTT when it
// changes state
//
#pragma once
#pragma STACX_LIB Bounce2
#include <Bounce2.h>

class ButtonLeaf : public Leaf
{
public:
  Bounce button = Bounce(); // Instantiate a Bounce object
  int button_pin = -1;
  int active = LOW;
  bool pullup = true;

  ButtonLeaf(String name, pinmask_t pins, int active=LOW, int pullup=true)
    : Leaf("button", name, pins)
    , Debuggable(name)
  {
    this->active = active;
    this->do_heartbeat = false;
    this->pullup = pullup;
  }

  virtual void setup(void) {
    Leaf::setup();
    LEAF_ENTER(L_INFO);
    FOR_PINS({button_pin=pin;}); // compile time default
    registerLeafIntValue("pin", &button_pin, "Pin number of button"); // runtime override
    registerLeafBoolValue("pullup", &pullup, "Enable button pullup");
    registerLeafBoolValue("active", &active, "Button active value (0=LOW 1=HIGH)");

    LEAF_NOTICE("%s claims pin %d as INPUT (active %s, %s)",
		describe().c_str(), button_pin,
		active?"HIGH":"LOW",pullup?"pull-up":"float");
    LEAF_LEAVE;
  }

  virtual void start(void) 
  {
    Leaf::start();
    LEAF_ENTER(L_INFO);
    button.attach(button_pin,pullup?INPUT_PULLUP:INPUT);
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
