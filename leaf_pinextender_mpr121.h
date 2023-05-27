//
//@**************************** class PinExtenderMPR121Leaf ******************************
//
// This class encapsulates the MPR121 touch-keypad and LED driver chip
//
#pragma once

#include <Wire.h>
#include "trait_pollable.h"
#include "trait_wirenode.h"
#include "Adafruit_MPR121.h"

class PinExtenderMPR121Leaf : public Leaf, public WireNode, public Pollable
{
protected:
  uint16_t bits_out;
  uint16_t bits_in;
  uint16_t last_input_state;
  uint16_t bits_inverted;
  uint16_t bits_input;
  Adafruit_MPR121 *device = NULL;
  
  bool found;
  bool pin_inverted[12];
  String pin_names[12];
  bool publish_bits = true;
public:
  PinExtenderMPR121Leaf(String name, int address=0x5a, String names="", uint16_t direction=0)
    : Leaf("pinextender", name, NO_PINS)
    , TraitDebuggable(name)
  {
    LEAF_ENTER(L_INFO);
    found = false;
    this->address=address;
    this->wire = &Wire;
    bits_out = bits_in = last_input_state = direction;
    bits_inverted = 0;
    this->sample_interval_ms = 50;
    
    for (int c=0; c<12; c++) {
      pin_names[c] = "";
    }
    for (int c=0; c<12 && names.length(); c++) {
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
    device = new Adafruit_MPR121();

    address = getIntPref(String("pinextender_addr_")+getName(), address, "I2C address override for pin extender (decimal)");

    if (!probe(address)) {
      LEAF_ALERT("   MPR121 NOT FOUND at 0x%02x", (int)address);
      run=false;
      address=0;
      LEAF_VOID_RETURN;
    }

    if (!device->begin(address)) {
      LEAF_ALERT("   MPR121 INITIALISATION ERROR at 0x%02x", (int)address);
      run=false;
      address=0;
      LEAF_VOID_RETURN;
    }
      
    found=true;
    LEAF_NOTICE("%s claims I2C addr 0x%02x", describe().c_str(), address);
    if (pin_names[0].length()) {
      for (int c=0; (c<12) && pin_names[0].length(); c++) {
	LEAF_NOTICE("pin %02d is named %s%s", c, pin_names[c].c_str(), (bits_inverted&(1<<c))?" (inverted)":"");
      }
    }

    LEAF_LEAVE;
  }

  virtual void start(void) 
  {
    Leaf::start();
    LEAF_ENTER(L_INFO);
    LEAF_LEAVE;
  }

  virtual void loop(void) {
    LEAF_ENTER(L_DEBUG);

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
  
  virtual bool poll()
  {
    if (!device) return(false);
    
    LEAF_ENTER(L_DEBUG);
    
    uint16_t bits = device->touched();
    //LEAF_NOTICE("Input bits %02x", (int)bits_in);
    // 
    // If the value has changed, return true
    // 
    if (bits != bits_in) {
      bits_in = bits;
      LEAF_DEBUG("Input bit change %02x", (int)bits_in);
      LEAF_RETURN(true);
    }
    
    LEAF_RETURN(false);
  }

  void status_pub()
  {
    LEAF_ENTER(L_DEBUG);
    
    char msg[64];
    if (publish_bits) {
      snprintf(msg, sizeof(msg), "%03x", bits_in);
      mqtt_publish("status/bits_in", msg);
      draw_bits(bits_in, msg);
      publish("status/pins_in", msg);

      snprintf(msg, sizeof(msg), "%03x", bits_out);
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
    uint16_t last = last_input_state;
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
      LEAF_DEBUG("Is it %d:%s?", c, pin_names[c].c_str());
      if (s == pin_names[c]) {
	LEAF_INT_RETURN(c);
      }
    }
    int result = s.toInt();
    LEAF_INT_RETURN(result);
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
      LEAF_INFO("Updating output bit %d (val=%d mask=0x%02x)", bit, (int)val, bval);
      LEAF_ALERT("Output not implemented yet");

      // patch last_input_state so that we don't double-publish the state change
      last_input_state = ((last_input_state & ~(1<<bit)) | bval);

      publish(String("status/")+topicfrag, String(val?"on":"off"), L_INFO, HERE);
    })
    ELSEWHENPREFIX("set/direction/",{
      String topicfrag = payload.substring(payload.lastIndexOf('/')+1);
      bit = parse_channel(topicfrag);
      val = (payload=="out");
      int bval = val?(1<<bit):0;
      LEAF_INFO("Setting direction on %d", bit);
      LEAF_ALERT("Output not implemented yet");
      // suppress the change-of-state detection
      last_input_state = ((last_input_state & ~(1<<bit)) | bval);

      publish(String("status/")+topicfrag, String(val?"on":"off"));
    })
    ELSEWHEN("set/pins",{
	uint8_t mask = (uint8_t)strtoul(payload.c_str(), NULL, 16);
	LEAF_INFO("Setting pin mask 0x%02", (int)mask);
	LEAF_ALERT("not implemented");
    })
    ELSEWHEN("set/publish_bits",{
	publish_bits = parseBool(payload, false);
    })
    ELSEWHEN("cmd/set",{
      bit = parse_channel(payload);
      LEAF_NOTICE("Setting pin %d (%s)", bit, payload.c_str());
      LEAF_ALERT("not implemented");
      status_pub();
    })
    ELSEWHEN("cmd/toggle",{
      bit = parse_channel(payload);
      LEAF_NOTICE("Toggle pin %d (%s)", bit, payload.c_str());
      LEAF_ALERT("not implemented");
      status_pub();
    })
    ELSEWHEN("cmd/unset",{
      bit = parse_channel(payload);
      LEAF_NOTICE("Clear pin %d (%s)", bit, payload.c_str());
      LEAF_ALERT("not implemented");
      status_pub();
    })
    ELSEWHEN("cmd/clear",{
      bit = parse_channel(payload);
      LEAF_NOTICE("Clear pin %d (%s)", bit, payload.c_str());
      LEAF_ALERT("not implemented");
      status_pub();
    })
    ELSEWHEN("cmd/poll",{
	poll();
	char bits_bin[10];
	draw_bits(bits_in, bits_bin);
	LEAF_NOTICE("Input bit pattern is 0x%02x (%s)", (int)bits_in, bits_bin);
	status_pub();
      })
    ELSEWHEN("cmd/test", {
	if (device) {
	  Serial.print("Filt: ");
	  for (uint8_t i=0; i<12; i++) {
	    Serial.print(device->filteredData(i)); Serial.print("\t");
	  }
	  Serial.println();
	  Serial.print("Base: ");
	  for (uint8_t i=0; i<12; i++) {
	    Serial.print(device->baselineData(i)); Serial.print("\t");
	  }
	  Serial.println();
	}
      })
    LEAF_BOOL_RETURN(handled);
  };

};


// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
