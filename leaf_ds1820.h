//
//@**************************** class DS1820Leaf ******************************
// 
// This class encapsulates a temp/humidity sensor that publishes measured
// environment values to MQTT
//
#include <OneWire.h>
#include <DallasTemperature.h>

class Ds1820Leaf : public AbstractTempLeaf
{
private:
  OneWire *oneWire = NULL;
  DallasTemperature *sensors = NULL;

public:
  Ds1820Leaf(String name, pinmask_t pins) : AbstractTempLeaf(name, pins) {
    LEAF_ENTER(L_INFO);
    LEAF_LEAVE;
  }

  void setup(void) {
    LEAF_ENTER(L_INFO);
    Leaf::setup();
    FOR_PINS(oneWire = new OneWire(pin););
    sensors = new DallasTemperature(oneWire);
    sensors->begin();
    LEAF_LEAVE;
  }

  bool poll(float *h, float *t, const char **status) 
  {
    //LEAF_INFO("Sampling DS1820");
    // time to take a new sample
    sensors->requestTemperatures();
    *t = sensors->getTempCByIndex(0);
    if (*t = DEVICE_DISCONNECTED_C) {
      *status = "disconnected";
    }
    else {
      *status = "ok";
    }
    //LEAF_INFO("t=%f (%s)", *t, *status);
    return true;
  }

};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
