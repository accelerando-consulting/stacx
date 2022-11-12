//
//@**************************** class MotionLeaf ******************************
// 
// This class encapsulates a motion sensor that publishes to MQTT when it
// sees motion
// 
#pragma once
#pragma STACX_LIB Bounce2
#include <Bounce2.h>

class MotionLeaf : public Leaf
{
public:
  Bounce sensor = Bounce(); // Instantiate a Bounce object

  MotionLeaf(String name, pinmask_t pins) : Leaf("motion", name, pins) {
    LEAF_ENTER(L_INFO);
    LEAF_LEAVE;
  }

  void setup(void) {
    Leaf::setup();
    LEAF_ENTER(L_NOTICE);
    int sensorPin;
    FOR_PINS({sensorPin=pin;});
    LEAF_INFO("%s claims pin %d as INPUT (debounced)", describe().c_str(), sensorPin);
    sensor.attach(sensorPin,INPUT_PULLUP); 
    LEAF_LEAVE;
  }

  void status_pub() 
  {
      mqtt_publish("status/motion", TRUTH_lc(sensor.read()));
  }

  void loop(void) {
    Leaf::loop();
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
      
    //LEAF_LEAVE;
  }

};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
