#pragma once
#include "abstract_pubsub_simcom.h"

//
//@********************** class PubsubMQTTSimcomLeaf ***********************
//
// This class encapsulates an Mqtt connection using the AT commands of the
// Sim7000 LTE modem
//

class PubsubMqttSimcomSim7080Leaf : public AbstractPubsubSimcomLeaf
{
public:
  PubsubMqttSimcomSim7080Leaf(String name, String target, bool use_ssl=true, bool use_device_topic=true, bool run = true) :
    AbstractPubsubSimcomLeaf(name, target, use_ssl, use_device_topic)
  {
    this->run = run;
    pubsub_reboot_modem = true;
  }
};

// Local Variables:
// mode: C++
// c-basic-offset: 2
// End:
