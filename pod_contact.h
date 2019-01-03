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

  ContactPod(String name, pinmask_t pins) : Pod("motion", name, pins) {
    ENTER(L_INFO);
    LEAVE;
  }

  void setup(void) {
    ENTER(L_NOTICE);
    Pod::setup();
    int contactPin;
    FOR_PINS(contactPin=pin);
    contact.attach(contactPin,INPUT_PULLUP); 
    contact.interval(25); 
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
      mqtt_publish("status/contact", contact.read()==LOW?"closed":"open"));
    });

    LEAVE;
    return handled;
  };
    
  void loop(void) {
    Pod::loop();
    contact.update();
    
    if (contact.fell()) {
      mqtt_publish("event/close", millis());
    } else if (button.rose()) {
      mqtt_publish("event/open", millis());
    }
  }

};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
