//
//@**************************** class MCP342xLeaf ******************************
//
// This class encapsulates the Maxim MAX44009 ambient light sensor
//
#pragma once

#include <Wire.h>
#include "Max44009.h"
#include "trait_pollable.h"
#include "trait_wirenode.h"

class MAX44009Leaf : public Leaf, public WireNode, public Pollable
{
protected:
  bool found;
  TwoWire *i2c_bus;
  Max44009 sensor;

  float lux = 0;

public:
  MAX44009Leaf(String name, int address=0x4a, TwoWire *bus=NULL)
    : Leaf("max44009", name, NO_PINS) {
    LEAF_ENTER(L_INFO);
    found = false;
    this->address=address;
    if (!bus) {
      bus = &Wire;
    }
    this->i2c_bus = bus;

    sample_interval_ms = 10000;
    report_interval_sec = 10;
    
    LEAF_LEAVE;
  }

  virtual void setup(void) {
    Leaf::setup();

    LEAF_ENTER(L_NOTICE);
    sensor.configure(address, i2c_bus);
    sensor.setAutomaticMode();
    found = sensor.isConnected();
    if (found) {
      LEAF_NOTICE("%s on I2C at 0x%02x", base_topic.c_str(), (int)address);
    }
  }

  virtual void loop(void) {
    //LEAF_ENTER(L_DEBUG);

    if (!address) {
      return;
    }
    Leaf::loop();

    pollable_loop();
    //LEAF_LEAVE;
  }

  virtual bool poll()
  {
    if (!found) return false;

    LEAF_ENTER(L_NOTICE);

    // 
    // read the current channel, return "no change" if failed
    // 
    float new_lux = sensor.getLux();
    int err = sensor.getError();
    LEAF_NOTICE("Read lux=%f err=%d", new_lux, err);

    if (err != 0) {
      LEAF_ALERT("Lux read error: %d", err);
      return false;
    }
    
    if (new_lux != lux) {
      lux = new_lux;
      LEAF_RETURN(true);
    }
    
    LEAF_RETURN(false);
  }

  void status_pub()
  {
    if (!found) return;
    char msg[64];

    snprintf(msg, sizeof(msg), "%.2f", lux);
    mqtt_publish("status/lux", msg);
  }

};


// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
