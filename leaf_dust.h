//
//@**************************** class DustLeaf *****************************
// 
// You can copy this class to use as a boilerplate for new classes
//
#include <GP2YDustSensor.h>


class DustLeaf : public Leaf
{
public:
  // 
  // Declare your leaf-specific instance data here
  //
  GP2YDustSensor *sensor = NULL;
  int ledPin;
  int analogPin;
  uint16_t dustDensity;
  uint16_t averageDensity;
  float sensitivity;
  

  // 
  // Leaf constructor method(s)
  // Call the superclass constructor to handle common arguments (type, name, pins)
  //
  DustLeaf(String name, int ledPin, int analogPin)
    : Leaf("dust", name, ledPin|analogPin)
    , TraitDebuggable(name)
  {
    sensor = new GP2YDustSensor(GP2YDustSensorType::GP2Y1010AU0F, ledPin, analogPin);
  }

  //
  // Arduino Setup function
  // Use this to configure any IO resources
  //
  void setup(void) {
    Leaf::setup();
    sensor->begin();
  }

  void loop(void) 
  {
    uint16_t value = sensor->getDustDensity();
    LEAF_DEBUG("dustValue=%d", (int)value);
    if (value != dustDensity) {
      dustDensity = value;
      mqtt_publish("status/dust", String((int)dustDensity));
    }
    value = sensor->getRunningAverage();
    if (value != averageDensity) {
      averageDensity = value;
      mqtt_publish("status/average_dust", String((int)averageDensity));
    }
  }
  
};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
