//
//@**************************** class DHTPod ******************************
// 
// This class encapsulates a temp/humidity sensor that publishes measured
// environment values to MQTT
//

#include <DHTesp.h>

class Dht11Pod : public AbstractTempPod
{
public:
  DHTesp dht;
 
  Dht11Pod(String name, pinmask_t pins) : AbstractTempPod(name, pins) {
    ENTER(L_INFO);
    LEAVE;
  }

  void setup(void) {
    ENTER(L_NOTICE);
    Pod::setup();
    FOR_PINS(dht.setup(pin, DHTesp::DHT11););
    LEAVE;
  }

  bool poll(float *h, float *t, const char **status) 
  {
    INFO("Sampling DHT");
    // time to take a new sample
    *h = dht.getHumidity();
    *t = dht.getTemperature();
    *status = dht.getStatusString();
    DEBUG("h=%f t=%f (%s)", *h, *t, *status);
    return true;
  }

};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
