//
//@**************************** class SCD40Leaf ******************************
// 
// This class encapsulates a temp/humidity/co2 sensor that publishes measured
// environment values to MQTT
//

#include <Wire.h>
#include "abstract_temp.h"
#include <SensirionI2CScd4x.h>
#include "trait_wirenode.h"

class Scd40Leaf : public AbstractTempLeaf, public WireNode
{
public:
  SensirionI2CScd4x scd40;

  Scd40Leaf(String name, int address=0x62)
    : AbstractTempLeaf(name, 0)
    , WireNode(name, address)
    , Debuggable(name)
 {
    LEAF_ENTER(L_INFO);
    LEAF_LEAVE;
  }

  void setup(void) {
    AbstractTempLeaf::setup();
    
    LEAF_ENTER(L_INFO);
    scd40.begin(Wire);

    uint16_t serial0;
    uint16_t serial1;
    uint16_t serial2;
    uint16_t error;
    error = scd40.getSerialNumber(serial0, serial1, serial2);
    if (error) {
      LEAF_ALERT("Scd40 not responding to serial number");
      stop();
      return;
    }
    LEAF_NOTICE("Scd40 at 0x%02x, serial 0x%04x%04x%04x",
		address, serial0, serial1, serial2);

    error = scd40.startPeriodicMeasurement();
    if (error) {
      LEAF_ALERT("Error trying to execute startPeriodicMeasurement");
    }


    LEAF_LEAVE;
  }

  bool poll(float *h, float *t, const char **status) 
  {
    LEAF_ENTER(L_DEBUG);

    uint16_t new_co2 = 0;
    float new_temperature = 0.0f;
    float new_humidity = 0.0f;
    bool isDataReady = false;
    uint16_t error;
    bool changed = false;

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
      LEAF_ALERT("scd40 readMeasurement failed");
      return false;
    }
    if (new_co2 == 0) {
      LEAF_WARN("Invalid sample detected, skipping.");
      return false;
    }

    if (isnan(*h) || (*h != new_humidity)) {
      *h = new_humidity;
      changed = true;
    }
    if (isnan(*t) || (*t != new_temperature)) {
      *t = new_temperature;
      changed = true;
    }

    if (isnan(ppmCO2) || (ppmCO2 != new_co2)) {
      ppmCO2 = new_co2;
      changed = true;
    }

    RETURN(changed);
  }

};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
