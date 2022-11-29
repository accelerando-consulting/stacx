//
//@**************************** class ImsADXL345Leaf ******************************
//
// This class encapsulates the ADXL 345 3-axis accelerometer
//

#include <Wire.h>
#include "abstract_ims.h"
#include "trait_wirenode.h"

#include "Adafruit_ADXL345_U.h"

class ImsADXL345Leaf : public AbstractIMSLeaf, public WireNode
{
protected:
  Adafruit_ADXL345_Unified ims;
  bool found;
  float delta_ang = 1;

public:
  ImsADXL345Leaf(String name)
    : AbstractIMSLeaf(name, 0)
    , TraitDebuggable(name)
  {
    found = false;
    this->sample_interval_ms = 500;
    this->report_interval_sec = 900;
  }

  void sensor_info(void)
  {
    sensor_t sensor;
    ims.getSensor(&sensor);
    mqtt_publish("info/sensor_name", sensor.name);
    mqtt_publish("info/sensor_ver", String(sensor.version));
    mqtt_publish("info/sensor_id", String(sensor.sensor_id));
    mqtt_publish("info/sensor_max_value", String(sensor.max_value));
    mqtt_publish("info/sensor_min_value", String(sensor.min_value));
    mqtt_publish("info/sensor_resolution", String(sensor.resolution));
  }

  void sensor_rate_info(void)
  {
    String rate;
  
    switch(ims.getDataRate())
    {
    case ADXL345_DATARATE_3200_HZ:
      rate="3200";
      break;
    case ADXL345_DATARATE_1600_HZ:
      rate="1600"; 
      break;
    case ADXL345_DATARATE_800_HZ:
      rate="800"; 
      break;
    case ADXL345_DATARATE_400_HZ:
      rate="400"; 
      break;
    case ADXL345_DATARATE_200_HZ:
      rate="200"; 
      break;
    case ADXL345_DATARATE_100_HZ:
      rate="100"; 
      break;
    case ADXL345_DATARATE_50_HZ:
      rate="50"; 
      break;
    case ADXL345_DATARATE_25_HZ:
      rate="25"; 
      break;
    case ADXL345_DATARATE_12_5_HZ:
      rate="12.5"; 
      break;
    case ADXL345_DATARATE_6_25HZ:
      rate="6.25"; 
      break;
    case ADXL345_DATARATE_3_13_HZ:
      rate="3.13"; 
      break;
    case ADXL345_DATARATE_1_56_HZ:
      rate="1.56"; 
      break;
    case ADXL345_DATARATE_0_78_HZ:
      rate="0.78"; 
      break;
    case ADXL345_DATARATE_0_39_HZ:
      rate="0.39"; 
      break;
    case ADXL345_DATARATE_0_20_HZ:
      rate="0.20"; 
      break;
    case ADXL345_DATARATE_0_10_HZ:
      rate="0.10"; 
      break;
    default:
      rate="unk";
      break;
    }
    mqtt_publish("info/sensor_rate", rate);
  }

  void sensor_range_info(void)
  {
    String range;
  
    switch(ims.getRange())
    {
    case ADXL345_RANGE_16_G:
      range="16"; 
      break;
    case ADXL345_RANGE_8_G:
      range="8"; 
      break;
    case ADXL345_RANGE_4_G:
      range="4"; 
      break;
    case ADXL345_RANGE_2_G:
      range="2"; 
      break;
    default:
      range="unk";
    }  
    mqtt_publish("info/sensor_range", range);
  }
  

  virtual void setup(void) {
    AbstractIMSLeaf::setup();

    LEAF_ENTER(L_NOTICE);
    char buf[64];

    snprintf(buf, sizeof(buf), "%s_delta", leaf_name.c_str());
    getFloatPref(buf, &delta_ang, "Tilt sensor change threshold (degrees)");
    
    if (! ims.begin()){
      LEAF_ALERT("ADXL345 sensor not found");
      run=false;
    }
    else {
      found = true;
      LEAF_NOTICE("Found ADXL345 on I2C (library does not tell us its address >_<)");
      ims.setRange(ADXL345_RANGE_16_G);

      sensor_info();
      sensor_rate_info();
      sensor_range_info();
    }

    LEAF_LEAVE;
  }

  

  virtual bool poll()
  {
    if (!found) return false;

    LEAF_ENTER(L_TRACE);

    sensors_event_t event; 
    ims.getEvent(&event);

    bool result = false;
    float accX,accY,accZ;
    float angX,angY;

    accX = event.acceleration.x;
    accY = event.acceleration.y;
    accZ = event.acceleration.z;

    // Computing accel angles
    angX = wrap((atan2(accY, sqrt(accZ * accZ + accX * accX))) * RAD_TO_DEG);
    angY = wrap((-atan2(accX, sqrt(accZ * accZ + accY * accY))) * RAD_TO_DEG);
    LEAF_TRACE("acc[%.2f,%.2f,%.2f] => angAcc[%4.f,%4.f]",accX, accY, accZ, angX, angY);

    //
    // Calculate if inclination has changed "significantly"
    //
    if (isnan(tilt_x) || abs(tilt_x-angX) >= delta_ang) {
      result=true;
      tilt_x = angX;
    }
    if (isnan(tilt_y) || abs(tilt_y-angY) >= delta_ang) {
      tilt_y = angY;
      result=true;
    }
    if (result) {
      LEAF_NOTICE("Inclination change [%.1f/%.1f]", tilt_x, tilt_y);
      LEAF_INFO("acc[%.2f,%.2f,%.2f] => angAcc[%4.f,%4.f]",accX, accY, accZ, angX, angY);
    }

    LEAF_LEAVE;
    return result;

  }

  void status_pub()
  {
    if (!found) return;

    AbstractIMSLeaf::status_pub();
    char msg[64];

    snprintf(msg, sizeof(msg), "[%.2f, %.2f]", tilt_x, tilt_y);
    mqtt_publish("status/orientation", msg);
  }


};


// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
