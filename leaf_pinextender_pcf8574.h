//
//@**************************** class MCP342xLeaf ******************************
//
// This class encapsulates the PCF8574 io expander
//
#pragma once

#include <Wire.h>
#include "trait_pollable.h"
#include "trait_wirenode.h"

class PinExtenderPCF8574Leaf : public Leaf, public WireNode, public Pollable
{
protected:
  byte bits_out;
  byte bits_in;
  bool found;
public:
  PinExtenderPCF8574Leaf(String name, int address=0x41
    ) : Leaf("pinextender", name, NO_PINS) {
    LEAF_ENTER(L_INFO);
    found = false;
    this->address=address;
    this->wire = &Wire;
    LEAF_LEAVE;
  }

  virtual void setup(void) {
    Leaf::setup();

    LEAF_ENTER(L_NOTICE);
    //wire->begin();

    if (!probe(address)) {
      LEAF_ALERT("   PCF8574 NOT FOUND at 0x%02x", (int)address);
      address=0;
      LEAF_VOID_RETURN;
    }
    found=true;

    LEAF_LEAVE;
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

  virtual void mqtt_do_subscribe() {
    LEAF_ENTER(L_DEBUG);
    Leaf::mqtt_do_subscribe();
    mqtt_subscribe("cmd/set");
    mqtt_subscribe("cmd/clear");
    mqtt_subscribe("cmd/toggle");
    LEAF_LEAVE;
  }

  int write(uint8_t bits) 
  {
    LEAF_NOTICE("pcf8574_write addr=%02x bits=0x%02x\n", address, (int)bits);
    wire->beginTransmission(address);

    int rc = Wire.write(bits);
    if (rc != 0) {
      LEAF_ALERT("PCF8574 pin write failed, returned %02x", rc);
      return -1;
    }
    if (wire->endTransmission(true) != 0) {
      LEAF_ALERT("MCP342x transaction failed");
      return -1;
    }
    bits_out = bits;
    return 0;
  }

  virtual bool poll()
  {
    if (!found) return false;

    LEAF_ENTER(L_DEBUG);

    Wire.requestFrom((uint8_t)address, (uint8_t)1);
    unsigned long then = millis();
    while (!Wire.available()) {
      unsigned long now = millis();
      if ((now - then) > 1000) {
	ALERT("Timeout waiting for I2C byte\n");
	return false;
      }
    }
    uint8_t bits = Wire.read();
    // 
    // If the value has changed, return true
    // 
    if (bits != bits_in) {
      bits_in = bits;
      LEAF_RETURN(true);
    }
    
    LEAF_RETURN(false);
  }

  void status_pub()
  {
    if (!found) return;

    char msg[64];
    snprintf(msg, sizeof(msg), "[0x%02x,0x%02x]", bits_out, bits_in);
    mqtt_publish("status/bits", msg);
  }

  virtual bool mqtt_receive(String type, String name, String topic, String payload) {
    LEAF_ENTER(L_DEBUG);
    bool handled = Leaf::mqtt_receive(type, name, topic, payload);
    bool val = false;
    if (payload == "on") val=true;
    else if (payload == "true") val=true;
    else if (payload == "high") val=true;
    else if (payload == "1") val=true;
    int bit = payload.toInt();

    LEAF_INFO("%s [%s]", topic.c_str(), payload.c_str());

    WHEN("cmd/set",{
	write(bits_out | (1<<bit));
      status_pub();
    })
    WHEN("cmd/status",{
	write(bits_out & ~(1<<bit));
      status_pub();
    })
    WHEN("cmd/toggle",{
	write(bits_out ^ (1<<bit));
      status_pub();
    })
    LEAF_LEAVE;
    return handled;
  };

};


// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
