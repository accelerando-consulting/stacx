//
//@*************************** class MCP23017Leaf ******************************
//
// This class encapsulates the MCP23017 io expander
//
#pragma once

#include <Wire.h>
#include "trait_pollable.h"
#include "trait_wirenode.h"

class PinExtenderMCP23017Leaf : public Leaf, public WireNode, public Pollable
{
protected:
  uint16_t bits_dir=0xFFFF;  // default state of MCP23017 is all input
  uint16_t bits_out;
  uint16_t bits_in;
  uint16_t bits_inverted;
  uint16_t bits_pullup;
  String pin_names[16];
  bool found;
public:

  // The registers have two different orderings based on the IOCON.BANK bit,
  // the default BANK=0 has the registers for each bank side by side
  //
  static const int REG_IODIRA   = 0x00;
  static const int REG_IODIRB   = 0x01;
  static const int REG_IOPOLA   = 0x02;
  static const int REG_IOPOLB   = 0x03;
  static const int REG_GPINTENA = 0x04;
  static const int REG_GPINTENB = 0x05;
  static const int REG_DEFVALA  = 0x06;
  static const int REG_DEFVALB  = 0x07;
  static const int REG_INTCONA  = 0x08;
  static const int REG_INTCONB  = 0x09;
  static const int REG_IOCON    = 0x0A;
  static const int REG_GPPUA    = 0x0C;
  static const int REG_GPPUB    = 0x0D;
  static const int REG_INTFA    = 0x0E;
  static const int REG_INTFB    = 0x0F;
  static const int REG_INTCAPA  = 0x10;
  static const int REG_INTCAPB  = 0x11;
  static const int REG_GPIOA    = 0x12;
  static const int REG_GPIOB    = 0x13;
  static const int REG_OLATA    = 0x14;
  static const int REG_OLATB    = 0x15;

  PinExtenderMCP23017Leaf(String name, int address=0x40, String names="")
    : Leaf("pinextender", name, NO_PINS)
    , WireNode(name, address)
    , Pollable(50, -1)
    , Debuggable(name)
  {
    LEAF_ENTER(L_INFO);
    found = false;
    bits_inverted = bits_in = bits_out = 0;
    for (int c=0; c<16; c++) {
      pin_names[c]="";
    }
    for (int c=0; c<16 && names.length(); c++) {
      int pos = names.indexOf(',');
      if (pos >= 0) {
	pin_names[c] = names.substring(0,pos);
	names.remove(0,pos+1);
      }
      else {
	pin_names[c] = names;
	names="";
      }
      if (pin_names[c].startsWith("~")) {
	bits_inverted |= (1<<c);
      }
    }
    LEAF_LEAVE;
  }

  virtual void setup(void) {
    Leaf::setup();

    LEAF_ENTER(L_INFO);
    //wire->begin();

    registerLeafByteValue("i2c_addr", &address, "I2C address override for pin extender (decimal)");

    if (!probe(address)) {
      LEAF_ALERT("   MCP23017 NOT FOUND at 0x%02x", (int)address);
      address=0;
      stop();
      LEAF_VOID_RETURN;
    }
    found=true;
    LEAF_NOTICE("%s claims I2C addr 0x%02x", describe().c_str(), address);
    if (pin_names[0].length()) {
      for (int c=0; (c<16) && pin_names[0].length(); c++) {
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

  int write_regpair(uint16_t bits, int offset = REG_GPIOA) 
  {
    LEAF_NOTICE("write_regpair reg=0x%02x bits=0x%02x", offset, (int)bits);
    if (!found) return -1;
    
    wire->beginTransmission(address);

    int rc = Wire.write(offset);
    if (rc != 1) {
      LEAF_ALERT("MCP23017 reg write failed, returned %02x", rc);
      return -1;
    }
    rc = Wire.write(bits&0xFF);
    if (rc != 1) {
      LEAF_ALERT("MCP23017 REGA write failed, returned %02x", rc);
      return -1;
    }
    rc = Wire.write((bits>>8)&0xFF);
    if (rc != 1) {
      LEAF_ALERT("MCP23017 REGB write failed, returned %02x", rc);
      return -1;
    }
    if (wire->endTransmission(true) != 0) {
      LEAF_ALERT("MCP23017 transaction failed");
      return -1;
    }
    if (offset == REG_GPIOA) {
      bits_out = bits;
    }
    else if (offset == REG_IODIRA) {
      bits_dir = bits;
    }
    else if (offset == REG_IOPOLA) {
      bits_inverted = bits;
    }
    else if (offset == REG_GPPUA) {
      bits_pullup = bits;
    }
    
    return 0;
  }

  virtual bool poll()
  {
    if (!found) return false;

    LEAF_ENTER(L_TRACE);

    Wire.beginTransmission(address);
    int cnt = Wire.write(REG_GPIOA);
    if (cnt != 1) {
      LEAF_ALERT("poll address write failed");
    }
    Wire.endTransmission(false);
    Wire.requestFrom((int)address, (int)2);
    unsigned long start=millis();
    while (!Wire.available()) {
      unsigned long now = millis();
      if ((now - start) > 1000) {
	LEAF_ALERT("I2C GPIOA read timeout");
	return false;
      }
    }
    int valA = Wire.read();

    while (!Wire.available()) {
      unsigned long now = millis();
      if ((now - start) > 1000) {
	LEAF_ALERT("I2C GPIOB read timeout");
	return true;
      }
    }
    int valB = Wire.read();

    uint16_t bits = (valB & 0xFF)<<8 | (valA & 0xFF);
    LEAF_TRACE("mcp23107 GPIO read result = 0x%04X", (int)bits);

    // 
    // If the value has changed, return true
    // 
    if (bits != bits_in) {
      bits_in = bits;
      LEAF_INFO("mcp23107 GPIO input change: 0x%04X", (int)bits_in);
      LEAF_RETURN(true);
    }
    
    LEAF_RETURN(false);
  }

  void status_pub()
  {
    if (!found) return;
    static uint16_t last_input_state = 0;
    poll();

    char top[64];
    char msg[64];
    snprintf(top, sizeof(top), "status/%s/bits_in", leaf_name.c_str());
    snprintf(msg, sizeof(msg), "%04x", bits_in);
    mqtt_publish(top, msg);
    snprintf(top, sizeof(top), "status/%s/bits_out", leaf_name.c_str());
    snprintf(msg, sizeof(msg), "%04x", bits_out);
    mqtt_publish(top, msg);

    for (int c=0; c<16; c++) {
      uint16_t mask = 1<<c;
      if ((last_input_state & mask) != (bits_in & mask)) {
	if (pin_names[c].length()) {
	  snprintf(top, sizeof(top), "status/%s/%s", leaf_name.c_str(),pin_names[c].c_str());
	}
	else {
	  snprintf(top, sizeof(top), "status/%s/%d", leaf_name.c_str(),c);
	}
	int value = (bits_in&mask)?1:0;
	LEAF_NOTICE("mcp23107 GPIO pin change: %s: %d", top+7, value);
	mqtt_publish(top, String(value));
      }
    }
    last_input_state = bits_in;
  }

  int parse_channel(String s) {
    for (int c=0; c<16 && pin_names[c].length(); c++) {
      if (pin_names[c] == s) return c;
    }
    return s.toInt();
  }

  virtual bool mqtt_receive(String type, String name, String topic, String payload, bool direct=false) {
    LEAF_ENTER(L_DEBUG);
    bool handled = false;
    bool val = false;
    if (payload == "on") val=true;
    else if (payload == "true") val=true;
    else if (payload == "high") val=true;
    else if (payload == "1") val=true;
    int bit = parse_channel(payload);
    bool failed = false;

    LEAF_INFO("%s [%s]", topic.c_str(), payload.c_str());

    WHEN("get/pin",{
      poll();
      mqtt_publish(String("status/")+payload, String((bits_in & (1<<bit))?1:0));
    })
    ELSEWHENPREFIX("set/pin/",{
      bit = parse_channel(payload);
      int bval = (val?1:0)<<bit;
      if (write_regpair((bits_out & !(1<<bit)) | bval)<0) {
	failed=true;
      }
    })
    ELSEWHEN("set/pins",{
	if (write_regpair((uint16_t)strtoul(payload.c_str(), NULL, 16)<0)) {
	  failed = true;
	}
      })
    ELSEWHEN("set/directions",{
	if (write_regpair((uint16_t)strtoul(payload.c_str(), NULL, 16), REG_IODIRA)<0) {
	  failed=true;
	}
    })
    ELSEWHEN("set/inversions",{
	if (write_regpair((uint16_t)strtoul(payload.c_str(), NULL, 16), REG_IOPOLA)<0) {
	  failed=true;
	}
    })
    ELSEWHEN("set/pullups",{
	if (write_regpair((uint16_t)strtoul(payload.c_str(), NULL, 16), REG_GPPUA)<0){
	  failed=true;
	}
    })
    ELSEWHEN("cmd/set",{
	if (write_regpair(bits_out | (1<<bit))<0) {
	  failed=true;
	}
      status_pub();
    })
    ELSEWHEN("cmd/clear",{
	if (write_regpair(bits_out & ~(1<<bit))<0) {
	  failed=true;
	}
      status_pub();
    })
    ELSEWHEN("cmd/toggle",{
	if (write_regpair(bits_out ^ (1<<bit))<0) {
	  failed=true;
	}
      status_pub();
    })
    else {
      handled = Leaf::mqtt_receive(type, name, topic, payload, direct);
    }
    if (failed) {
      LEAF_WARN("pinextender operation failed (%s <= %s)", topic.c_str(), payload.c_str());
    }
    LEAF_LEAVE;
    return handled;
  };

};


// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
