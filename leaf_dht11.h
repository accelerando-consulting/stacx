//
//@**************************** class DHTLeaf ******************************
// 
// This class encapsulates a temp/humidity sensor that publishes measured
// environment values to MQTT
//
#pragma STACX_LIB esp826611
#include <DHT.h>
#include "abstract_temp.h"

class Dht11Leaf : public AbstractTempLeaf
{
public:
  DHT *dht=NULL;
  int dht_pin;
  
  Dht11Leaf(String name, pinmask_t pins)
    : AbstractTempLeaf(name, pins, 0.24, 0.99, 3, 12)
    , Debuggable(name)
  {
    FOR_PINS({dht_pin=pin;});
    sample_interval_ms = 5000;
    report_interval_sec = 600;
  }

  void setup(void) {
    LEAF_ENTER(L_INFO);
    AbstractTempLeaf::setup();
    if (dht) {
      delete dht;
      dht=NULL;
    }
    LEAF_NOTICE("DHT11 sensor on pin %d", dht_pin);
    dht = new DHT(dht_pin, DHT11);
    LEAF_LEAVE;
  }

  
  void start(void) 
  {
    AbstractTempLeaf::start();
    LEAF_ENTER(L_INFO);
    if (dht) dht->begin();
    LEAF_LEAVE;
  }

  bool poll(float *h, float *t, const char **status) 
  {
    if (!dht) return false;
    LEAF_DEBUG("Sampling DHT");
    // time to take a new sample
    *h = dht->readHumidity();
    *t = dht->readTemperature();
    //*status = dht.getStatusString();
    LEAF_DEBUG("h=%f t=%f", *h, *t);
    if (isnan(*h) && isnan(*t)) {
      return false;
    }
    return true;
  }

};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
