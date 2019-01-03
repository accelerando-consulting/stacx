//
//@**************************** class ButtonPod ******************************
// 
// This class encapsulates a simple pushbutton that publishes to MQTT when it
// changes state
//
#include <Bounce2.h>

class ButtonPod : public Pod
{
public:
  Bounce button = Bounce(); // Instantiate a Bounce object

 
  ButtonPod(String name, pinmask_t pins) : Pod("motion", name, pins) {
    ENTER(L_INFO);
    LEAVE;
  }

  void setup(void) {
    ENTER(L_NOTICE);
    Pod::setup();
    int buttonPin;
    FOR_PINS(buttonPin=pin);
    button.attach(buttonPin,INPUT_PULLUP); 
    button.interval(25); 
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
      mqtt_publish("status/button", STATE(button.read()));
    });

    LEAVE;
    return handled;
  };
    
  void loop(void) {
    Pod::loop();
    button.update();
    
    if (button.fell()) {
      mqtt_publish("event/press", millis());
    } else if (button.rose()) {
      mqtt_publish("event/release", millis());
    }
  }

};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
