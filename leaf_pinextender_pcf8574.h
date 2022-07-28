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
  uint8_t bits_out;
  uint8_t bits_in;
  uint8_t last_input_state;
  uint8_t bits_inverted;
  uint8_t bits_input;
  
  bool found;
  bool pin_inverted[8];
  String pin_names[8];
public:
  PinExtenderPCF8574Leaf(String name, int address=0x20, String names=""
    ) : Leaf("pinextender", name, NO_PINS) {
    LEAF_ENTER(L_INFO);
    found = false;
    this->address=address;
    this->wire = &Wire;
    bits_inverted = bits_in = bits_out = 0;
    for (int c=0; c<8; c++) {
      pin_names[c] = "";
    }
    for (int c=0; c<8 && names.length(); c++) {
      int pos = names.indexOf(',');
      if (pos >= 0) {
	pin_names[c] = names.substring(0,pos);
	names.remove(0,pos+1);
      }
      else {
	pin_names[c] = names;
	names = "";
      }
      if (pin_names[c].startsWith("~")) {
	bits_inverted |= (1<<c);
      }
    }

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

    LEAF_NOTICE("%s claims i2c addr 0x%02x", base_topic.c_str(), address);
    if (pin_names[0].length()) {
      for (int c=0; (c<8) && pin_names[0].length(); c++) {
	LEAF_NOTICE("pin %02d is named %s%s", c, pin_names[c].c_str(), (bits_inverted&(1<<c))?" (inverted)":"");
      }
    }

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
    LEAF_ENTER(L_NOTICE);
    
    uint8_t pattern = bits ^ bits_inverted;
    LEAF_NOTICE("pcf8574_write addr=%02x bits=0x%02x pattern=0x%02x\n", address, (int)bits,(int)pattern);
    wire->beginTransmission(address);

    int rc = Wire.write(pattern);
    if (rc != 0) {
      LEAF_ALERT("PCF8574 pin write failed, returned %02x", rc);
      return -1;
    }
    if (wire->endTransmission(true) != 0) {
      LEAF_ALERT("MCP342x transaction failed");
      return -1;
    }
    LEAF_NOTICE("Write concluded");
    
    bits_out = bits;
    LEAF_RETURN(0);
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
    snprintf(msg, sizeof(msg), "%04x", bits_in);
    mqtt_publish("status/bits_in", msg);

    snprintf(msg, sizeof(msg), "%04x", bits_out);
    mqtt_publish("status/bits_out", msg);

    for (int c=0; c<8; c++) {
      uint16_t mask = 1<<c;
      if ((last_input_state & mask) != (bits_in & mask)) {
	if (pin_names[c].length()) {
	  snprintf(msg, sizeof(msg), "status/%s", pin_names[c].c_str());
	}
	else {
	  snprintf(msg, sizeof(msg), "status/%d", c);
	}
	mqtt_publish(msg, String((bits_in&mask)?1:0));
      }
    }
  }

  int parse_channel(String s) {
    for (int c=0; c<8 && pin_names[c].length(); c++) {
      if (pin_names[c] == s) return c;
    }
    return s.toInt();
  }

  virtual bool mqtt_receive(String type, String name, String topic, String payload) {
    LEAF_ENTER(L_NOTICE);
    bool handled = Leaf::mqtt_receive(type, name, topic, payload);
    bool val = false;
    if (payload == "on") val=true;
    else if (payload == "true") val=true;
    else if (payload == "high") val=true;
    else if (payload == "1") val=true;
    int bit = payload.toInt();

    LEAF_INFO("%s [%s]", topic.c_str(), payload.c_str());

    WHEN("cmd/status",{
	write(bits_out & ~(1<<bit));
      status_pub();
    })
    WHEN("get/pin",{
      poll();
      mqtt_publish(String("status/")+payload, String((bits_in & (1<<bit))?1:0));
    })
    WHENPREFIX("set/pin/",{
	String topicfrag = payload.substring(payload.lastIndexOf('/')+1);
      bit = parse_channel(topicfrag);
      int bval = (val?1:0)<<bit;
      LEAF_NOTICE("Updating output bit %d", bit);
      write((bits_out & !(1<<bit)) | bval);
      publish(String("status/")+topicfrag, val?"on":"off");
    })
    WHEN("set/pins",{
      write((uint8_t)strtoul(payload.c_str(), NULL, 16));
    })
    WHEN("cmd/set",{
      write(bits_out |= (1<<bit));
      status_pub();
    })
    WHEN("cmd/toggle",{
	write(bits_out ^= (1<<bit));
      status_pub();
    })
    WHEN("cmd/unset",{
	write(bits_out &= ~(1<<bit));
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
