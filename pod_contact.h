//
//@**************************** class ButtonPod ******************************
// 
// This class encapsulates a door contact sensor (reed switch)
//
#include <Bounce2.h>

class ContactPod : public Pod
{
public:
  Bounce contact = Bounce(); // Instantiate a Bounce object

  ContactPod(String name, pinmask_t pins) : Pod("contact", name, pins) {
    ENTER(L_INFO);
    LEAVE;
  }

  void setup(void) {
    ENTER(L_NOTICE);
    Pod::setup();
    int contactPin;
    FOR_PINS(contactPin=pin;);
    contact.attach(contactPin,INPUT_PULLUP); 
    contact.interval(25); 
    LEAVE;
  }

  void status_pub() 
  {
      mqtt_publish("status/contact", (contact.read()==LOW)?"closed":"open");
  }

  void loop(void) {
    Pod::loop();
    contact.update();
    bool changed = false;
    
    if (contact.fell()) {
      mqtt_publish("event/close", String(millis(), DEC));
      changed = true;
    } else if (contact.rose()) {
      mqtt_publish("event/open", String(millis(), DEC));
      changed = true;
    }
    if (changed) status_pub();
  }

};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End: