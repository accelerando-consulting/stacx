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
  bool publish_bits = true;
public:
  PinExtenderPCF8574Leaf(String name, int address=0, String names="", uint8_t direction=0xFF)
    : Leaf("pinextender", name, NO_PINS)
    , WireNode(name, address)
    , Pollable(50, -1)
    , Debuggable(name)
  {
    LEAF_ENTER(L_INFO);
    found = false;
    bits_out = bits_in = last_input_state = direction;
    bits_inverted = 0;

    setPinNames(names);

    LEAF_LEAVE;
  }

  void setPinNames(String names="")
  {
    LEAF_ENTER_STR(L_NOTICE, names);

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

    LEAF_ENTER(L_INFO);
    //wire->begin();

    registerLeafByteValue("i2c_addr", &address, "I2C address override for pin extender (decimal)");

    if ((address == 0) && probe(0x20)) {
      LEAF_NOTICE("   PCF8574 auto-detected at 0x20");
      address = 0x20;
    }
    delay(100);
    // not an else case to the above, we want that delay in any case
    if ((address == 0) && probe(0x38)) {
      LEAF_NOTICE("   PCF8574 auto-detected at 0x38");
      address = 0x38;
    }
    delay(100);

    if (!probe(address)) {
      LEAF_ALERT("   PCF8574 NOT FOUND at 0x%02x", (int)address);
      stop();
      LEAF_VOID_RETURN;
    }
    found=true;

    LEAF_NOTICE("%s claims I2C addr 0x%02x", describe().c_str(), address);
    if (pin_names[0].length()) {
      for (int c=0; (c<8) && pin_names[0].length(); c++) {
	LEAF_NOTICE("pin %02d is named %s%s", c, pin_names[c].c_str(), (bits_inverted&(1<<c))?" (inverted)":"");
      }
    }

    LEAF_NOTICE("Set outputs logically off at setup");
    write(~bits_inverted); // all outputs logicall "off" initially

    LEAF_LEAVE;
  }

  virtual void start(void)
  {
    Leaf::start();
    LEAF_ENTER(L_INFO);
    write(bits_out);
    LEAF_LEAVE;
  }

  virtual void loop(void) {
    LEAF_ENTER(L_TRACE);

    Leaf::loop();

    pollable_loop();
    LEAF_LEAVE;
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
    LEAF_ENTER_BYTE(L_INFO, bits);

    char bits_bin[10];
    char pat_bin[10];

    uint8_t pattern = bits ^ bits_inverted;
    draw_bits(bits, bits_bin);
    draw_bits(pattern, pat_bin);

    //LEAF_INFO("pcf8574_write addr=%02x bits=0x%02x (%s) pattern=0x%02x (%s)\n", address, (int)bits,bits_bin, (int)pattern, pat_bin);
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
    //LEAF_DEBUG("Write concluded");

    bits_out = bits;
    LEAF_RETURN(0);
  }

  virtual bool poll()
  {
    LEAF_ENTER(L_TRACE);

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
      LEAF_DEBUG("Input bit change %02x => %02x", (int)bits_in, (int)bits);
      bits_in = bits;
      LEAF_RETURN(true);
    }

    LEAF_RETURN(false);
  }

  virtual void status_pub()
  {
    LEAF_ENTER(L_DEBUG);

    char msg[64];
    if (publish_bits) {
      snprintf(msg, sizeof(msg), "%02x", bits_in);
      mqtt_publish("status/bits_in", msg);
      draw_bits(bits_in, msg);
      publish("status/pins_in", msg);

      snprintf(msg, sizeof(msg), "%02x", bits_out);
      mqtt_publish("status/bits_out", msg);
      draw_bits(bits_out, msg);
      publish("status/pins_out", msg);
    }

    // this function may be reentrant -- publishing an action may result in delivery of
    // an action which itself causes another change of state.
    //
    // Because of this, it is important to take a COPY of the state and keep it on the stack!
    //
    // Yes, it took a long time to find this out!
    //
    uint8_t last = last_input_state;
    last_input_state=bits_in;

    for (int c=0; c<8; c++) {
      uint16_t mask = 1<<c;
      // when doing a shell command (pubsub_loopback) print everything. Otherwise only changed.
      if (pubsub_loopback || ((last & mask) != (bits_in & mask))) {
	if (pin_names[c].length()) {
	  snprintf(msg, sizeof(msg), "status/%s", pin_names[c].c_str());
	}
	else {
	  snprintf(msg, sizeof(msg), "status/%d", c);
	}
	mqtt_publish(msg, String((bits_in&mask)?1:0), 0, false, L_INFO, HERE);
      }
    }

    LEAF_LEAVE;
  }

  int parse_channel(String s) {
    LEAF_ENTER_STR(L_DEBUG, s);
    for (int c=0; (c<8) && pin_names[c].length(); c++) {
      //LEAF_DEBUG("Is it %d:%s?", c, pin_names[c].c_str());
      if (s == pin_names[c]) {
	LEAF_INT_RETURN(c);
      }
    }
    int result = s.toInt();
    LEAF_INT_RETURN(result);
  }

  virtual bool mqtt_receive(String type, String name, String topic, String payload, bool direct=false) {
    LEAF_ENTER(L_DEBUG);
    bool handled = false;
    bool val = false;
    if (payload == "on") val=true;
    else if (payload == "true") val=true;
    else if (payload == "high") val=true;
    else if (payload == "1") val=true;
    int bit = payload.toInt();

    LEAF_INFO("RECV %s/%s %s <= [%s]", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());

    WHEN("get/pin",{
      poll();
      bit = parse_channel(payload);
      mqtt_publish(String("status/")+payload, String((bits_in & (1<<bit))?1:0));
    })
    WHEN("get/pins",{
      poll();
      char buf[4];
      snprintf(buf, sizeof(buf), "%02X", (int)bits_in);
      mqtt_publish(String("status/pins"), buf);
    })
    ELSEWHENPREFIX("set/pin/",{
      String topicfrag = topic.substring(payload.lastIndexOf('/')+1);
      bit = parse_channel(topicfrag);
      int bval = val?(1<<bit):0;
      //LEAF_INFO("Updating output bit %d (val=%d mask=0x%02x)", bit, (int)val, bval);
      write((bits_out & ~(1<<bit)) | bval);

      // patch last_input_state so that we don't double-publish the state change
      last_input_state = ((last_input_state & ~(1<<bit)) | bval);

      publish(String("status/")+topicfrag, String(val?"on":"off"), L_INFO, HERE);
    })
    ELSEWHENPREFIX("set/direction/",{
      String topicfrag = payload.substring(payload.lastIndexOf('/')+1);
      bit = parse_channel(topicfrag);
      val = (payload=="out");
      int bval = val?(1<<bit):0;
      //LEAF_INFO("Setting direction on %d", bit);
      write((bits_out & ~(1<<bit)) | bval);
      // suppress the change-of-state detection
      last_input_state = ((last_input_state & ~(1<<bit)) | bval);

      publish(String("status/")+topicfrag, String(val?"on":"off"));
    })
    ELSEWHEN("set/pins",{
	uint8_t mask = (uint8_t)strtoul(payload.c_str(), NULL, 16);
	//LEAF_INFO("Setting pin mask 0x%02", (int)mask);
	write(mask);
    })
    ELSEWHEN("set/publish_bits",{
	publish_bits = parseBool(payload, false);
    })
    ELSEWHEN("cmd/set",{
      bit = parse_channel(payload);
      LEAF_NOTICE("Setting pin %d (%s)", bit, payload.c_str());
      write(bits_out |= (1<<bit));
      status_pub();
    })
    ELSEWHEN("cmd/toggle",{
      bit = parse_channel(payload);
      LEAF_NOTICE("Toggle pin %d (%s)", bit, payload.c_str());
      write(bits_out ^= (1<<bit));
      status_pub();
    })
    ELSEWHEN("cmd/unset",{
      bit = parse_channel(payload);
      LEAF_NOTICE("Clear pin %d (%s)", bit, payload.c_str());
      write(bits_out &= ~(1<<bit));
      status_pub();
    })
    ELSEWHEN("cmd/clear",{
      bit = parse_channel(payload);
      LEAF_NOTICE("Clear pin %d (%s)", bit, payload.c_str());
      write(bits_out &= ~(1<<bit));
      status_pub();
    })
    ELSEWHEN("cmd/poll",{
	poll();
	char bits_bin[10];
	draw_bits(bits_in, bits_bin);
	LEAF_NOTICE("Input bit pattern is 0x%02x (%s)", (int)bits_in, bits_bin);
	status_pub();
      })
    else {
      handled = Leaf::mqtt_receive(type, name, topic, payload, direct);
    }

    LEAF_BOOL_RETURN(handled);
  };

};


// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
