//
//@**************************** class BMP180Leaf ******************************
// 
// This class encapsulates a temp/pressure sensor that publishes measured
// environment values to MQTT
//
#pragma once

#include "abstract_temp.h"

#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085.h>


class Bmp180Leaf : public AbstractTempLeaf
{
public:
  Adafruit_BMP085 bmp = Adafruit_BMP085();
 
  Bmp180Leaf(String name, pinmask_t pins=NO_PINS) : AbstractTempLeaf(name, pins) {
    LEAF_ENTER(L_INFO);
    LEAF_LEAVE;
  }

  void setup(void) {
    LEAF_ENTER(L_NOTICE);
    Leaf::setup();
    if(!bmp.begin()) {
      LEAF_ALERT("BMP180 Sensor not detected");
    }
    LEAF_LEAVE;
  }

  virtual bool poll(float *h, float *t, const char **status) 
  {
    pressure = 1013.25;
    temperature = 24.1;
    
    temperature = bmp.readTemperature();
    pressure = bmp.readPressure()/100;
    //LEAF_INFO("p=%.2f t=%.2f", pressure, temperature);
    if (t) *t=temperature;
    if (status) {
      *status = "ok";
    }

    return true;
  }
  
};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
