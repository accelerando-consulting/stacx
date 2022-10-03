//
//@**************************** class PinExtenderPCF8574Leaf ******************************
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
    bits_inverted = bits_in = bits_out = last_input_state = 0;
    this->sample_interval_ms = 50;
    
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

    address = getIntPref(String("pinextender_addr_")+get_name(), address, "I2C address override for pin extender (decimal)");

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

    LEAF_NOTICE("Set outputs logically off at setup");
    write(!bits_inverted); // all outputs logicall "off" initially

    LEAF_LEAVE;
  }

  virtual void loop(void) {
    //LEAF_ENTER(L_DEBUG);

    if (!found) {
      return;
    }
    Leaf::loop();

    pollable_loop();
    //LEAF_LEAVE;
  }

  virtual void mqtt_do_subscribe() {
    LEAF_ENTER(L_DEBUG);
    Leaf::mqtt_do_subscribe();
    mqtt_subscribe("cmd/set", HERE);
    mqtt_subscribe("cmd/clear", HERE);
    mqtt_subscribe("cmd/toggle", HERE);
    LEAF_LEAVE;
  }

  void draw_bits(uint8_t bits, char *buf) 
  {
    int p=0;
    for (int i=0; i<8; i++) {
      buf[p] = ((bits>>(7-i))&0x01)?'1':'0';
      p++;
      if (i==3) {
	buf[p]='-';
	p++;
      }
    }
    buf[9]='\0';
  }
  
  int write(uint8_t bits) 
  {
    LEAF_ENTER(L_INFO);

    char bits_bin[10];
    char pat_bin[10];

    uint8_t pattern = bits ^ bits_inverted;
    draw_bits(bits, bits_bin);
    draw_bits(pattern, pat_bin);
      
    LEAF_NOTICE("pcf8574_write addr=%02x bits=0x%02x (%s) pattern=0x%02x (%s)\n", address, (int)bits,bits_bin, (int)pattern, pat_bin);
    wire->beginTransmission(address);

    int rc = Wire.write(pattern);
    if (rc != 1) {
      LEAF_ALERT("PCF8574 pin write failed, returned %02x", rc);
      LEAF_RETURN(-1);
    }
    if (wire->endTransmission(true) != 0) {
      LEAF_ALERT("PCF8574 transaction failed");
      LEAF_RETURN(-1);
    }
    LEAF_DEBUG("Write concluded");
    
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
      if ((now - then) > 200) {
	ALERT("Timeout waiting for I2C byte\n");
	return false;
      }
    }
    uint8_t bits = Wire.read() ^ bits_inverted;
    //LEAF_NOTICE("Input bits %02x", (int)bits_in);
    // 
    // If the value has changed, return true
    // 
    if (bits != bits_in) {
      bits_in = bits;
      LEAF_NOTICE("Input bit change %02x", (int)bits_in);
      LEAF_RETURN(true);
    }
    
    LEAF_RETURN(false);
  }

  void status_pub()
  {
    if (!found) return;

    char msg[64];
    snprintf(msg, sizeof(msg), "%02x", bits_in);
    mqtt_publish("status/bits_in", msg);
    draw_bits(bits_in, msg);
    publish("status/pins_in", msg);

    snprintf(msg, sizeof(msg), "%02x", bits_out);
    mqtt_publish("status/bits_out", msg);
    draw_bits(bits_out, msg);
    publish("status/pins_out", msg);

    for (int c=0; c<8; c++) {
      uint16_t mask = 1<<c;
      // when doing a shell command (pubsub_loopback) print everything. Otherwise only changed.
      if (pubsub_loopback || ((last_input_state & mask) != (bits_in & mask))) {
	if (pin_names[c].length()) {
	  snprintf(msg, sizeof(msg), "status/%s", pin_names[c].c_str());
	}
	else {
	  snprintf(msg, sizeof(msg), "status/%d", c);
	}
	mqtt_publish(msg, String((bits_in&mask)?1:0), 0, false, L_NOTICE, HERE);
      }
    }
    last_input_state = bits_in;
  }

  int parse_channel(String s) {
    LEAF_ENTER_STR(L_DEBUG, s);
    for (int c=0; (c<8) && pin_names[c].length(); c++) {
      LEAF_DEBUG("Is it %d:%s?", c, pin_names[c].c_str());
      if (s == pin_names[c]) {
	LEAF_INT_RETURN(c);
      }
    }
    int result = s.toInt();
    LEAF_INT_RETURN(result);
  }

  virtual bool mqtt_receive(String type, String name, String topic, String payload) {
    LEAF_ENTER(L_INFO);
    bool handled = Leaf::mqtt_receive(type, name, topic, payload);
    bool val = false;
    if (payload == "on") val=true;
    else if (payload == "true") val=true;
    else if (payload == "high") val=true;
    else if (payload == "1") val=true;
    int bit = payload.toInt();

    LEAF_INFO("%s [%s]", topic.c_str(), payload.c_str());

    WHEN("get/pin",{
      poll();
      bit = parse_channel(payload);
      mqtt_publish(String("status/")+payload, String((bits_in & (1<<bit))?1:0));
    })
    ELSEWHENPREFIX("set/pin/",{
      String topicfrag = topic.substring(payload.lastIndexOf('/')+1);
      bit = parse_channel(topicfrag);
      int bval = val?(1<<bit):0;
      LEAF_NOTICE("Updating output bit %d (val=%d mask=0x%02x)", bit, (int)val, bval);
      write((bits_out & ~(1<<bit)) | bval);

      // patch last_input_state so that we don't double-publish the state change
      last_input_state = ((last_input_state & ~(1<<bit)) | bval);

      publish(String("status/")+topicfrag, String(val?"on":"off"), L_NOTICE, HERE);
    })
    ELSEWHENPREFIX("set/direction/",{
      String topicfrag = payload.substring(payload.lastIndexOf('/')+1);
      bit = parse_channel(topicfrag);
      val = (payload=="out");
      int bval = val?(1<<bit):0;
      LEAF_NOTICE("Setting direction on %d", bit);
      write((bits_out & ~(1<<bit)) | bval);
      // suppress the change-of-state detection
      last_input_state = ((last_input_state & ~(1<<bit)) | bval);

      publish(String("status/")+topicfrag, String(val?"on":"off"));
    })
    ELSEWHEN("set/pins",{
	uint8_t mask = (uint8_t)strtoul(payload.c_str(), NULL, 16);
	LEAF_NOTICE("Seeting pin mask 0x%02", (int)mask);
	write(mask);
    })
    ELSEWHEN("cmd/set",{
      bit = parse_channel(payload);
      write(bits_out |= (1<<bit));
      status_pub();
    })
    ELSEWHEN("cmd/toggle",{
      bit = parse_channel(payload);
      write(bits_out ^= (1<<bit));
      status_pub();
    })
    ELSEWHEN("cmd/unset",{
      bit = parse_channel(payload);
      write(bits_out &= ~(1<<bit));
      status_pub();
    })
    ELSEWHEN("cmd/poll",{
	if (!found) {
	  LEAF_ALERT("pin extender device was not found");
	}
	else {
	  poll();
	  char bits_bin[10];
	  draw_bits(bits_in, bits_bin);
	  LEAF_NOTICE("Input bit pattern is 0x%02x (%s)", (int)bits_in, bits_bin);
	  status_pub();
	}
      })
    LEAF_LEAVE;
    return handled;
  };

};


// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
