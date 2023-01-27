//
//@**************************** class DHTLeaf ******************************
// 
// This class encapsulates a temp/humidity sensor that publishes measured
// environment values to MQTT
//

#include <DHT12.h>
#include "abstract_temp.h"

class Dht12Leaf : public AbstractTempLeaf
{
public:
  DHT12 dht12;
 
  Dht12Leaf(String name, pinmask_t pins=0) : AbstractTempLeaf(name, pins) {
    LEAF_ENTER(L_INFO);
    LEAF_LEAVE;
  }

  void setup(void) {
    LEAF_ENTER(L_INFO);
    Leaf::setup();
    dht12.begin();
    LEAF_LEAVE;
  }

  bool poll(float *h, float *t, const char **status) 
  {
    *h = dht12.readHumidity();
    *t = dht12.readTemperature();
    if (isnan(*h) || isnan(*t)) {
      LEAF_ALERT("Failed to read from DHT12 sensor");
      *status = "readfail";
      return false;
    }
    *status = "ok";
    return true;
  }
  
};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
