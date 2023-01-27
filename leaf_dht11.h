//
//@**************************** class DHTLeaf ******************************
// 
// This class encapsulates a temp/humidity sensor that publishes measured
// environment values to MQTT
//

#include <DHTesp.h>

class Dht11Leaf : public AbstractTempLeaf
{
public:
  DHTesp dht;
 
  Dht11Leaf(String name, pinmask_t pins) : AbstractTempLeaf(name, pins) {
    LEAF_ENTER(L_INFO);
    LEAF_LEAVE;
  }

  void setup(void) {
    LEAF_ENTER(L_INFO);
    Leaf::setup();
    FOR_PINS(dht.setup(pin, DHTesp::DHT11););
    LEAF_LEAVE;
  }

  bool poll(float *h, float *t, const char **status) 
  {
    LEAF_INFO("Sampling DHT");
    // time to take a new sample
    *h = dht.getHumidity();
    *t = dht.getTemperature();
    *status = dht.getStatusString();
    LEAF_DEBUG("h=%f t=%f (%s)", *h, *t, *status);
    return true;
  }

};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
