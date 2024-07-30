//
//@**************************** class SCD40Leaf ******************************
// 
// This class encapsulates a temp/humidity/co2 sensor that publishes measured
// environment values to MQTT
//

#include <Wire.h>
#include "abstract_temp.h"

//#define USE_SCD40_SPARKFUN_LIBRARY 1
#define USE_SCD40_SENSIRION_LIBRARY 1

#ifdef USE_SCD40_SENSIRION_LIBRARY
#include <SensirionI2CScd4x.h>
#elif defined(USE_SCD40_SPARKFUN_LIBRARY)
#include <SparkFun_SCD4x_Arduino_Library.h>
#else
#error "No library choice defined"i
#endif

#include "trait_wirenode.h"

#ifdef USE_SCD40_SPARKFUN_LIBRARY
#ifndef SCD4x_SENSOR_TYPE
#define SCD4x_SENSOR_TYPE CD4x_SENSOR_SCD41
#endif
#endif

class Scd40Leaf : public AbstractTempLeaf, public WireNode
{
protected:
  bool has_changed=false;
public:
#ifdef USE_SCD40_SENSIRION_LIBRARY
  SensirionI2CScd4x scd40;
#elif defined(USE_SCD40_SPARKFUN_LIBRARY)
  //SCD4x scd40(SCD4x_SENSOR_TYPE);
  SCD4x scd40;
#endif
  
  Scd40Leaf(String name, int address=0x62)
    : AbstractTempLeaf(name, 0)
    , WireNode(name, address)
    , Debuggable(name)
#ifdef USE_SCD40_SPARKFUN_LIBRARY
    , scd40(SCD4x_SENSOR_TYPE)
#endif
 {
    LEAF_ENTER(L_INFO);
#ifdef USE_SCD40_SPARKFUN_LIBRARY
    sample_interval_ms = 5000;
#endif
    LEAF_LEAVE;
  }

  void setup(void) {
    LEAF_ENTER(L_INFO);
    AbstractTempLeaf::setup();

    if (!probe(address)) {
      LEAF_ALERT("  SCD4x not found at 0x%02X", (int)address);
      stop();
      return;
    }
    
#ifdef USE_SCD40_SENSIRION_LIBRARY
    LEAF_INFO("scd40.begin (Sensirion library)");
    scd40.begin(Wire);
    uint16_t serial0;
    uint16_t serial1;
    uint16_t serial2;
    uint16_t error;
    LEAF_INFO("scd40.getSerialNumber");
    error = scd40.getSerialNumber(serial0, serial1, serial2);
    if (error) {
      LEAF_ALERT("Scd40 not responding to serial number");
      stop();
      return;
    }
    LEAF_NOTICE("Scd40 at 0x%02x, serial 0x%04x%04x%04x",
		address, serial0, serial1, serial2);

    LEAF_INFO("scd40.startPeriodicMeasurement");
    error = scd40.startPeriodicMeasurement();
    if (error) {
      LEAF_ALERT("Error trying to execute startPeriodicMeasurement");
    }

#elif defined(USE_SCD40_SPARKFUN_LIBRARY)

    scd40.enableDebugging();
    
    LEAF_INFO("scd40.begin (SparkFun library)");
    if (scd40.begin() == false) {
      LEAF_ALERT("SCD40 initialisation failed");
      stop();
      return;
    }
    LEAF_NOTICE("SCD40 at I2C 0x%02x type %d", address, SCD4x_SENSOR_TYPE);
#endif
    // cause the first sample to be taken in (sample-interval + 1sec) to give
    // the device time to warm up
    last_sample = millis() + 1000;

    LEAF_LEAVE;
  }

  virtual bool hasChanged(float h, float t) 
  {
    if (has_changed) {
      has_changed = false;
      return true;
    }
    return false;
  }

  bool poll(float *h, float *t, const char **status) 
  {
    // this poll routines ignores the h and t pointers and updates
    // the superclass' storage directly (since it needs to do that for co2 anyway)
    LEAF_ENTER(L_DEBUG);

    uint16_t new_co2 = 0;
    float new_temperature = 0.0f;
    float new_humidity = 0.0f;
    bool isDataReady = false;
    uint16_t error;

#ifdef USE_SCD40_SENSIRION_LIBRARY
    error = scd40.getDataReadyFlag(isDataReady);
    if (error) {
      LEAF_WARN("sc4x is not able to provide status");
      return false;
    }
    if (!isDataReady) {
      LEAF_WARN("sc4x is not ready");
      return false;
    }

    error = scd40.readMeasurement(new_co2, new_temperature, new_humidity);
    if (error) {
      LEAF_ALERT("scd40 readMeasurement failed (device not ready)");
      LEAF_NOTICE("  FWIW CO2=%d temp=%.1f hum=%l1f", new_co2, new_temperature, new_humidity);
      return false;
    }
    if (new_co2 == 0) {
      LEAF_WARN("Invalid sample detected, skipping.");
      return false;
    }
#elif defined(USE_SCD40_SPARKFUN_LIBRARY)
    if (!scd40.readMeasurement()) {
      LEAF_ALERT("scd40 readMeasurement failed (device not ready)");
      return false;
    }
    new_co2 = scd40.getCO2();
    new_temperature = scd40.getTemperature();
    new_humidity= scd40.getHumidity();
#endif
    LEAF_DEBUG("SCD40 reading %dppm temp=%.1f hum=%.1f", (int)new_co2, new_temperature, new_humidity);

    if (isnan(humidity) || (fabs(humidity-new_humidity)>=humidity_change_threshold)) {
      LEAF_NOTICE("SCD40 Humidity value changed %.1f%% => %.1f%%", humidity, new_humidity);
      *h = humidity = new_humidity;
      has_changed = true;
    }
    if (isnan(temperature) || (fabs(temperature-new_temperature)>=temperature_change_threshold)) {
      LEAF_NOTICE("SCD40 Temperature value changed %.1fC => %.1fC", temperature, new_temperature);
      *t = temperature = new_temperature;
      has_changed = true;
    }
    if (new_co2 == 0) {
      LEAF_NOTICE("CO2 sensor is still warming up");
    }
    else if (isnan(ppmCO2) || (fabs(ppmCO2-new_co2)>=ppmCO2_change_threshold)) {
      LEAF_NOTICE("SCD40 CO2 reading changed %.0fppm => %dppm", ppmCO2, (int)new_co2);
      ppmCO2 = new_co2;
      has_changed = true;
    }

    RETURN(has_changed);
  }
};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
