//
//@**************************** class SCD30Leaf ******************************
// 
// This class encapsulates a temp/humidity/co2 sensor that publishes measured
// environment values to MQTT
//

#include <Wire.h>
#include "abstract_temp.h"
#include "SparkFun_SCD30_Arduino_Library.h" 

class Scd30Leaf : public AbstractTempLeaf
{
public:
  SCD30 scd30;
	
  Scd30Leaf(String name) : AbstractTempLeaf(name, 0) {
    LEAF_ENTER(L_INFO);
    LEAF_LEAVE;
  }

  void setup(void) {
    AbstractTempLeaf::setup();
    
    LEAF_ENTER(L_INFO);
    Wire.begin();
    scd30.begin();
    LEAF_LEAVE;
  }

  bool poll(float *h, float *t, const char **status) 
  {
    LEAF_ENTER(L_DEBUG);
    
    if (!scd30.dataAvailable()) {
      RETURN(false);
    }
		  
    *h = scd30.getHumidity();
    *t = scd30.getTemperature();
    this->ppmCO2 = scd30.getCO2();
    
    *status = "ok";

    RETURN(true);
  }

};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
