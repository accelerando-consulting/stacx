
//
//@**************************** class ButtonLeaf ******************************
// 
// This class encapsulates a door contact sensor (reed switch)
//
#pragma once
#pragma STACX_LIB Bounce2

#include <Bounce2.h>

class ContactLeaf : public Leaf
{
public:
  Bounce contact = Bounce(); // Instantiate a Bounce object

  ContactLeaf(String name, pinmask_t pins) : Leaf("contact", name, pins) {
    LEAF_ENTER(L_INFO);
    LEAF_LEAVE;
  }

  void setup(void) {
    LEAF_ENTER(L_NOTICE);
    Leaf::setup();
    int contactPin;
    FOR_PINS(contactPin=pin;);
    LEAF_INFO("%s claims pin %d as INPUT (debounced)", describe().c_str(), contactPin);
    contact.attach(contactPin,INPUT_PULLUP); 
    contact.interval(25); 
    LEAF_LEAVE;
  }

  void status_pub() 
  {
      mqtt_publish("status/contact", (contact.read()==LOW)?"closed":"open");
  }

  void start() 
  {
    Leaf::start();
    status_pub();
  }

  void loop(void) {
    Leaf::loop();
    contact.update();
    bool changed = false;
    static unsigned long lastPub = 0;
    
    if (contact.fell()) {
      mqtt_publish("event/close", String(millis(), DEC));
      changed = true;
    } else if (contact.rose()) {
      mqtt_publish("event/open", String(millis(), DEC));
      changed = true;
    }
    if (changed) status_pub();

/*
    if ((lastPub + 30000) < millis()) {
      lastPub = millis();
      status_pub();
    }
*/
  }

};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
