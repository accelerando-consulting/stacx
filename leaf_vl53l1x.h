//
//@**************************** class Vl53l1xLeaf ******************************
//
// This class encapsulates a VL53L1X LIDAR sensor
//
//
#pragma once
#pragma STACX_LIB SparkFun_VL53L1X_4m_Laser_Distance_Sensor

#include <Wire.h>
#include "SparkFun_VL53L1X.h"
#include "trait_wirenode.h"
#include "trait_pollable.h"

class Vl53l1xLeaf : public Leaf, public WireNode, public Pollable
{
protected:
  SFEVL53L1X distanceSensor;

  int delta;
  int dist;
  
public:
  Vl53l1xLeaf(String name, pinmask_t pins=0, byte address=0)
    : Leaf("vl53l1x", name, pins)
    , Pollable(1000, 300)
    , WireNode(name, address)
    , Debuggable(name)
  {
    LEAF_ENTER(L_INFO);
    this->delta = 100;
    this->do_heartbeat = false;

    LEAF_LEAVE;
  }

  virtual void setup(void) {
    Leaf::setup();
    LEAF_ENTER(L_DEBUG);

    if (distanceSensor.begin() != 0) //Begin returns 0 on a good init
    {
      ALERT("vl53l1x lidar not detected");
      preventRun();
      return;
    }
    LEAF_NOTICE("%s claims I2C addr 0x%02x", describe().c_str(), distanceSensor.getI2CAddress());
    distanceSensor.setDistanceModeLong();
    distanceSensor.setIntermeasurementPeriod(1000);
    distanceSensor.startRanging();
    LEAF_LEAVE;
  }

  virtual bool poll() {
    bool result = false;
    LEAF_ENTER(L_DEBUG);

    int new_dist = distanceSensor.getDistance(); //Get the result of the measurement from the sensor
    distanceSensor.clearInterrupt();

    if (abs(new_dist - dist) > delta) {
      LEAF_INFO("distance change %d=>%dmm", dist, new_dist);
      dist = new_dist;
      result = true;
    }
    
    LEAF_LEAVE;
    return result;
  }

  virtual void status_pub()
  {
    LEAF_ENTER(L_INFO);
    publish("status/distance", String(dist));
    LEAF_LEAVE;
  }

  virtual void loop(void) {
    LEAF_ENTER(L_DEBUG);

    Leaf::loop();
    pollable_loop();
    LEAF_LEAVE;
  }
};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
//
