#pragma once
//
//@**************************** class DS1820Leaf ******************************
// 
// This class encapsulates a temp/humidity sensor that publishes measured
// environment values to MQTT
//
#include <OneWire.h>
#include <DallasTemperature.h>
#include "abstract_temp.h"

class Ds1820Leaf : public AbstractTempLeaf
{
private:
  OneWire *oneWire = NULL;
  DallasTemperature *sensor = NULL;

public:
  Ds1820Leaf(String name, pinmask_t pins)
    : AbstractTempLeaf(name, pins)
    , Debuggable(name, L_INFO)
  {
    LEAF_ENTER(L_INFO);
    LEAF_LEAVE;
  }

  void setup(void) {
    LEAF_ENTER(L_NOTICE);
    Leaf::setup();
    FOR_PINS({
	LEAF_NOTICE("DS1820 temperature sensor on GPIO %d", pin);
	oneWire = new OneWire(pin);
      });
    sensor = new DallasTemperature(oneWire);
    LEAF_LEAVE;
  }

  void start(void) 
  {
    Leaf::start();
    LEAF_ENTER(L_NOTICE);
    sensor->begin();
    LEAF_LEAVE;
  }
  

  bool poll(float *h, float *t, const char **status) 
  {
    LEAF_INFO("Sampling DS1820");
    // time to take a new sample
    sensor->requestTemperatures();
    delay(200);
    *t = sensor->getTempCByIndex(0);
    if (*t == DEVICE_DISCONNECTED_C) {
      LEAF_NOTICE("DS18B20 is disconnected");
      *status = "disconnected";
      return false;
    }
    else {
      *status = "ok";
    }
    LEAF_NOTICE("t=%f (%s)", *t, *status);
    return true;
  }


};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
