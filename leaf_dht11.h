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
    : AbstractTempLeaf(name, pins, 0.9, 1.5, 3, 3)
    , Debuggable(name)
  {
    FOR_PINS({dht_pin=pin;});
    sample_interval_ms = 5000;
    report_interval_sec = 600;
  }

  void setup(void) {
    LEAF_ENTER(L_INFO);
    Leaf::setup();
    if (dht) {
      delete dht;
      dht=NULL;
    }
    dht = new DHT(dht_pin, DHT11);
    registerCommand(HERE,"poll", "poll the DHT sensor");
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
    if (isnan(*h) && isnan(*t)) {
      return false;
    }
    return true;
  }

  virtual bool commandHandler(String type, String name, String topic, String payload) {
    LEAF_HANDLER(L_INFO);

    WHEN("poll", {
	float h;
	float t;
	poll(&h, &t, NULL);
    })
    else {
      handled = Leaf::commandHandler(type, name, topic, payload);
    }
    LEAF_BOOL_RETURN(handled);
  }

};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
