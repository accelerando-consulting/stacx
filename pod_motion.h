//
//@**************************** class MotionPod ******************************
// 
// This class encapsulates a motion sensor that publishes to MQTT when it
// sees motion
// 
#include <Bounce2.h>

class MotionPod : public Pod
{
public:
  Bounce sensor = Bounce(); // Instantiate a Bounce object

  MotionPod(String name, pinmask_t pins) : Pod("motion", name, pins) {
    ENTER(L_INFO);
    LEAVE;
  }

  void setup(void) {
    Pod::setup();
    ENTER(L_NOTICE);
    int sensorPin;
    FOR_PINS({sensorPin=pin;});
    INFO("%s claims pin %d as INPUT (debounced)", base_topic.c_str(), sensorPin);
    sensor.attach(sensorPin,INPUT_PULLUP); 
    LEAVE;
  }

  void status_pub() 
  {
      mqtt_publish("status/motion", truth(sensor.read()));
  }

  void loop(void) {
    Pod::loop();
    sensor.update();
    
    bool changed = false;

    if (sensor.fell()) {
      changed = true;
    } else if (sensor.rose()) {
      mqtt_publish("event/motion", String(millis()));
      changed = true;
    }
    if (changed) {
      status_pub();
    }
      
    //LEAVE;
  }

};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
