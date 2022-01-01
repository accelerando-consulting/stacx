//
//@**************************** class ButtonLeaf ******************************
//
// This class encapsulates a simple pushbutton that publishes to MQTT when it
// changes state
//
#pragma once

#include <Bounce2.h>

class LevelLeaf : public Leaf
{
public:
  Bounce button = Bounce(); // Instantiate a Bounce object
  int level = 0;
  int active = HIGH;

  LevelLeaf(String name, pinmask_t pins) : Leaf("level", name, pins) {
    LEAF_ENTER(L_INFO);
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
    mqtt_publish("status/level", (button.read()==active)?"1":"0");
  }

  virtual void loop(void) {
    Leaf::loop();
    button.update();
    bool changed = false;

    if ((active==LOW)?button.fell():button.rose()) {
      mqtt_publish("event/active", String(millis(), DEC));
      changed = true;
    }
    if ((active==LOW)?button.rose():button.fell()) {
      mqtt_publish("event/idle", String(millis(), DEC));
      changed = true;
    }
    if (changed) status_pub();
  }

};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
