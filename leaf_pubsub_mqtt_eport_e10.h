#pragma once
#include "abstract_pubsub_simcom.h"

//
//@********************** class PubsubMQTTSimcomSim7000Leaf ***********************
//
// This class encapsulates an Mqtt connection using the AT commands of the
// Sim7000 LTE modem
//

class PubsubMqttEportE10Leaf : public AbstractPubsubLeaf
{
public:
  PubsubMqttEportE10Leaf(String name, String target, bool use_ssl=true, bool use_device_topic=true, bool run = true) :
    AbstractPubsubLeaf(name, target, use_ssl, use_device_topic)
  {
    this->run = run;
  }

  virtual uint16_t _mqtt_publish(String topic, String payload, int qos=0, bool retain=false) 
  {
    LEAF_NOTICE("PUB (NOT IMPLEMENTED YET) [%s] <= [%s]", topic.c_str(), payload.c_str());
    return 0;
  }

  virtual void _mqtt_subscribe(String topic, int qos=0) 
  {
    LEAF_NOTICE("SUB (NOT IMPLEMENTED YET) [%s]", topic.c_str());
  }

  virtual void _mqtt_unsubscribe(String topic) 
  {
    LEAF_NOTICE("UNSUB (NOT IMPLEMENTED YET) [%s]", topic.c_str());
  }

  virtual void initiate_sleep_ms(int ms)
  {
    LEAF_NOTICE("eport_e10 initiate_sleep_ms NOT IMPLEMENTED YET");
  }
  
  
};

// Local Variables:
// mode: C++
// c-basic-offset: 2
// End:
