//
//@**************************** class SGP30Leaf ******************************
// 
// This class encapsulates a voc/eco2/h2/ethanol sensor that publishes measured
// environment values to MQTT
//

#include <Wire.h>
#include "leaf_temp_abstract.h"
#include "Adafruit_SGP30.h"

class Sgp30Leaf : public AbstractTempLeaf
{
public:
  bool found;
  Adafruit_SGP30 sgp30;
  int counter;
  uint16_t TVOC_base, eCO2_base;
  
  Sgp30Leaf(String name) : AbstractTempLeaf(name, 0) {
    ENTER(L_INFO);
    found = false;
    counter =0;
    TVOC_base=eCO2_base=0;
    LEAVE;
  }

  void setup(void) {
    AbstractTempLeaf::setup();

    ENTER(L_NOTICE);
    Wire.begin();
    if (! sgp30.begin()){
      ALERT("SGP30 sensor not found");
    }
    else {
      found = true;
      NOTICE("Found SGP30 serial #%02x%02x%02x", sgp30.serialnumber[0],sgp30.serialnumber[1],sgp30.serialnumber[2]);
    }
    
    
    // TODO: tap into scd30 to get temp/hum values
    
    LEAVE;
  }

  bool poll(float *h, float *t, const char **status) 
  {
    ENTER(L_DEBUG);
    
    if (!found) return false;
    if (! sgp30.IAQmeasure()) {
      ALERT("SGP30 Measurement failed");
      LEAVE;
      return false;
    }
    ppmtVOC = sgp30.TVOC;
    ppmeCO2 = sgp30.eCO2;

    if (! sgp30.IAQmeasureRaw()) {
      ALERT("SGP30 Raw Measurement failed");
      return false;
    }
    rawH2 = sgp30.rawH2;
    rawEthanol = sgp30.rawEthanol;

    ++counter;
    if (counter >= 30) {
      counter=0;

      if (! sgp30.getIAQBaseline(&eCO2_base, &TVOC_base)) {
	ALERT("Failed to get baseline readings");
	return false;
      }
      NOTICE("Baseline values eCO2=0x%x TVOC=0x%x", eCO2_base, TVOC_base);
      mqtt_publish("status/eco2_baseline", String(eCO2_base));
      mqtt_publish("status/tvoc_baseline", String(TVOC_base));
    }
    
    LEAVE;
    return true;
  }

  
};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
