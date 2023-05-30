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
    : AbstractTempLeaf(name, pins)
    , Debuggable(name)
  {
    FOR_PINS({dht_pin=pin;});
    //delta=2.5;
  }

  void setup(void) {
    LEAF_ENTER(L_INFO);
    Leaf::setup();
    if (dht) {
      delete dht;
      dht=NULL;
    }
    dht = new DHT(dht_pin, DHT11);
    LEAF_LEAVE;
  }
  void start(void) 
  {
    Leaf::start();
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
    return true;
  }

};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
