#pragma once
#include "trait_pollable.h"

//
//@**************************** class DHTLeaf ******************************
// 
// This class encapsulates an inertial measurement sensor sensor that
// publishes measured acceleration/rotation/orientation values to MQTT
// 
class AbstractIMSLeaf : public Leaf, public Pollable
{
public:
  float accel_x;
  float accel_y;
  float accel_z;
  float gyro_x;
  float gyro_y;
  float gyro_z;
  float compass_x;
  float compass_y;
  float compass_z;
  float tilt_x;
  float tilt_y;
  int decimal_places = 3;

  AbstractIMSLeaf(String name, pinmask_t pins)
    : Leaf("ims", name, pins)  
    , TraitDebuggable(name)
  {
    LEAF_ENTER(L_INFO);
    accel_x = accel_y = accel_z = NAN;
    gyro_x = gyro_y = gyro_z = NAN;
    compass_x = compass_y = compass_z = NAN;
    tilt_x = tilt_y = NAN;
    
    report_interval_sec = 60;
    sample_interval_ms = 2000;
    last_sample = 0;
    last_report = 0;
    
    LEAF_LEAVE;
  }

  virtual void status_pub() 
  {
    LEAF_ENTER(L_INFO);

    if (!isnan(accel_x)) mqtt_publish("status/accel_x", String(accel_x,decimal_places));
    if (!isnan(accel_y)) mqtt_publish("status/accel_y", String(accel_y,decimal_places));
    if (!isnan(accel_z)) mqtt_publish("status/accel_z", String(accel_z,decimal_places));
    if (!isnan(gyro_x)) mqtt_publish("status/gyro_x", String(gyro_x,decimal_places));
    if (!isnan(gyro_y)) mqtt_publish("status/gyro_y", String(gyro_y,decimal_places));
    if (!isnan(gyro_z)) mqtt_publish("status/gyro_z", String(gyro_z,decimal_places));
    if (!isnan(compass_x)) mqtt_publish("status/compass_x", String(compass_x,decimal_places));
    if (!isnan(compass_y)) mqtt_publish("status/compass_y", String(compass_y,decimal_places));
    if (!isnan(compass_z)) mqtt_publish("status/compass_x", String(compass_z,decimal_places));

    LEAF_LEAVE;
  }
  
  void loop(void) {
    //LEAF_ENTER(L_DEBUG);
    Leaf::loop();
    pollable_loop();
    //LEAF_LEAVE;
  }

protected:
  float wrap(float angle)
  {
    //LEAF_DEBUG("wrap angle=%f", angle);
  
    while (angle > +180) angle -= 360;
    while (angle < -180) angle += 360;
    return angle;
  }

  float angle_average(float wa, float a, float wb, float b)
  {
    //LEAF_DEBUG("angle_average wa=%f a=%f wb=%f b=%f", wa, a, wb, b);
  
    return wrap(wa * a + wb * (a + wrap(b-a)));
  }

};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
