//
//@**************************** class DHTLeaf ******************************
// 
// This class encapsulates a temp/humidity sensor that publishes measured
// environment values to MQTT
//
#pragma once

#include <DHT12.h>
#include "abstract_temp.h"

class Dht12Leaf : public AbstractTempLeaf, public WireNode
{
public:
  DHT12 *dht12=NULL;
 
  Dht12Leaf(String name, int address = 0x5c)
    : AbstractTempLeaf(name, NO_PINS)
    , WireNode(name, address)
    , Debuggable(name)
  {
    LEAF_ENTER(L_INFO);
    LEAF_LEAVE;
  }

  void setup(void) {
    LEAF_ENTER(L_INFO);
    Leaf::setup();

    if (!probe(address)) {
      LEAF_ALERT("  DHT12 not found at 0x%02X", (int)address);
      stop();
      return;
    }

    dht12 = new DHT12(address);
    dht12->begin();
    LEAF_LEAVE;
  }

  bool poll(float *h, float *t, const char **status) 
  {
    *h = dht12->readHumidity();
    *t = dht12->readTemperature();
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
