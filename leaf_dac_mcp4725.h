//
//@*************************** class MCP4725Leaf ******************************
//
// This class encapsulates the MCP4725 I2C DAC
//
#pragma once

#include <Wire.h>
#include "trait_wirenode.h"
#include "MCP4725.h"

class DACMCP4725Leaf : public Leaf, public WireNode
{
protected:
  int millivolts;
  int max_millivolts;
  bool found;
  MCP4725 *dac=0;
public:


  DACMCP4725Leaf(String name, int address=0x50, int max_mv = 3300)
    : Leaf("dac", name, NO_PINS)
    , WireNode(name, address)
    , Debuggable(name)
  {
    LEAF_ENTER(L_INFO);
    found = false;
    this->max_millivolts = max_mv;
    LEAF_LEAVE;
  }

  virtual void setup(void) {
    Leaf::setup();

    LEAF_ENTER(L_INFO);

    registerLeafByteValue("i2c_addr", &address, "I2C address override for pin extender (decimal)");
    registerLeafIntValue("output", &millivolts, "initial DAC output level", ACL_GET_SET, VALUE_NO_SAVE);

    if (!probe(address)) {
      LEAF_ALERT("   MCP4725 NOT FOUND at 0x%02x", (int)address);
      address=0;
      stop();
      LEAF_VOID_RETURN;
    }
    found=true;
    LEAF_NOTICE("%s claims I2C addr 0x%02x", describe().c_str(), address);

    dac = new MCP4725(address);
    dac->begin();
    dac->setMaxVoltage((float)max_millivolts / 1000.0);
    dac->setMilliVolts(millivolts);

    LEAF_LEAVE;
  }

  virtual bool valueChangeHandler(String topic, Value *v) {
    LEAF_HANDLER(L_INFO);
    WHEN("output", {
      LEAF_INFO("Set DAC millivolts = %d", millivolts);
      dac->setMilliVolts(millivolts);
    })
    else handled = Leaf::valueChangeHandler(topic, v);
    LEAF_HANDLER_END;
  }

};


// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
