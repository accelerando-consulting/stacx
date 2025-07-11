//
//@**************************** class BH1750Leaf ******************************
//
// This class encapsulates the BH1740 illumination sensor
//
#pragma once

#include <Wire.h>
#include "trait_pollable.h"
#include "trait_wirenode.h"
#include "Adafruit_CCS811.h"

class CCS811Leaf : public Leaf, public WireNode, public Pollable
{
protected:
  bool found=false;
  uint16_t raw;
  Adafruit_CCS811 ccs811;
  
public:
  CCS811Leaf(String name, int address=0x23)
    : Leaf("ccs811", name, NO_PINS)
    , WireNode(name, address)
    , Pollable(1000, 15)
    , Debuggable(name)
  {
  }

  virtual void setup(void) {
    Leaf::setup();

    LEAF_ENTER(L_INFO);

    if (!ccs811.begin()) {
      LEAF_ALERT("  CCS811 NOT FOUND");
      address=0;
      stop();
      LEAF_VOID_RETURN;
    }

    LEAF_NOTICE("  CCS811 voc sensor found");
    found=true;
    
    LEAF_LEAVE;
  }

  virtual void loop(void) {
    //LEAF_ENTER(L_DEBUG);

    if (!ccs811.available()) {
      return;
    }
    Leaf::loop();

    pollable_loop();
    //LEAF_LEAVE;
  }

  virtual bool poll()
  {
    if (!found) return false;

    LEAF_ENTER(L_DEBUG);

    // 
    // read the current channel, return "no change" if failed
    // 
    uint16_t new_raw = ccs811.getVOC();
    LEAF_NOTICE("CCS811 %d", (int)new_raw);

    if (new_raw != raw) {
      raw = new_raw;
      LEAF_RETURN(true);
    }
    
    LEAF_RETURN(false);
  }

  void status_pub()
  {
    if (!found) return;

    mqtt_publish("status/tvoc", String(raw, DEC));
  }

};


// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
