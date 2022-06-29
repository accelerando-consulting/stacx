//
//@**************************** class PinExtenderPCA9685Leaf ******************************
// 
// This class encapsulates the PCA9684 PWM IO extender (output only)
//
#pragma once

#include <Wire.h>
#include "trait_wirenode.h"
#include "PCA9685.h"

class PinExtenderPCA9685Leaf : public Leaf, public WireNode
{
protected:
  PCA9685 extender;
  uint32_t bits_out;
  byte pwm_out[16];
  bool pin_inverted[16];
  String pin_names[16];
  bool found;
public:
  PinExtenderPCA9685Leaf(String name, int address=0x41, String names=""
    ) : Leaf("pinextender", name, NO_PINS) {
    LEAF_ENTER(L_NOTICE);
    found = false;
    this->address=address;
    this->wire = &Wire;

    bits_out = 0;
    for (int c=0; c<16; c++) {
      pwm_out[c] = 0;
      pin_inverted[c] = false;
      pin_names[c] = "";
    }

    for (int c=0; c<16 && names.length(); c++) {
      int pos = names.indexOf(',');
      if (pos >= 0) {
	pin_names[c] = names.substring(0,pos);
	names.remove(0,pos+1);
      }
      else {
	pin_names[c] = names;
      }
      pin_inverted[c] = pin_names[c].startsWith("~");
    }
    
    LEAF_LEAVE;
  }

  virtual void setup(void) {
    Leaf::setup();

    LEAF_ENTER(L_NOTICE);

    if (!probe(address)) {
      LEAF_ALERT("   PCA9685 NOT FOUND at 0x%02x", (int)address);
      address=0;
      LEAF_VOID_RETURN;
    }
    found=true;
    LEAF_NOTICE("%s claims i2c addr 0x%02x", base_topic.c_str(), address);
    if (pin_names[0].length()) {
      for (int c=0; (c<16) && pin_names[0].length(); c++) {
	LEAF_NOTICE("%s   pin %02d is named %s%s", base_topic.c_str(), c, pin_names[c].c_str(), pin_inverted[c]?" (inverted)":"");
      }
    }

    extender.setupSingleDevice(Wire, (uint8_t)address);
    extender.setToServoFrequency();
    for (int c=0; c<16; c++) {
      extender.setChannelDutyCycle(c, pin_inverted[c]?100:0);
    }


    LEAF_LEAVE;
  }

  void status_pub()
  {
    if (!found) return;

    char msg[64];
    snprintf(msg, sizeof(msg), "0x%02x", bits_out);
    mqtt_publish("status/bits", msg);
    for (int bit=0; bit<16;bit++) {

      if (pwm_out[bit]) {
	if (pin_names[bit].length()) {
	  snprintf(msg, sizeof(msg), "status/pwm/%s", pin_names[bit].c_str());
	}
	else {
	  snprintf(msg, sizeof(msg), "status/pwm/%d", bit);
	}
	mqtt_publish(msg, String((int)pwm_out[bit]));
      }
      else {
	if (pin_names[bit].length()) {
	  snprintf(msg, sizeof(msg), "status/outputs/%s", pin_names[bit].c_str());
	}
	else {
	  snprintf(msg, sizeof(msg), "status/outputs/%d", bit);
	}
	mqtt_publish(msg, String((bits_out&(1<<bit))?1:0));
      }
    }
  }

  virtual void mqtt_do_subscribe() {
    LEAF_ENTER(L_DEBUG);
    Leaf::mqtt_do_subscribe();
    mqtt_subscribe("cmd/set");
    mqtt_subscribe("cmd/pwm/+");
    mqtt_subscribe("cmd/clear");
    mqtt_subscribe("cmd/toggle");
    LEAF_LEAVE;
  }

  int parse_channel(String s) {
    for (int c=0; c<16 && pin_names[c].length(); c++) {
      if (pin_names[c] == s) return c;
    }
    return s.toInt();
  }

  void set_channel_bool(int c, bool enable) {
    if (enable) {
      pwm_out[c] = pin_inverted[c]?0:100;
      bits_out |= 1<<c;
    }
    else {
      pwm_out[c] = pin_inverted[c]?100:0;
      bits_out &= ~(1<<c);
    }
    NOTICE("PWM channel %d <= (bool) %d", c, pwm_out[c]);
    extender.setChannelDutyCycle(c, pwm_out[c]);
  }

  void set_channel_pwm(int c, int percent) {
    pwm_out[c] = pin_inverted[c]?(100-c):c;
    if (percent) {
      bits_out |= 1<<c;
    }
    else {
      bits_out &= ~(1<<c);
    }
    NOTICE("PWM channel %d <= %d", c, pwm_out[c]);
    extender.setChannelDutyCycle(c, pwm_out[c]);
  }

  virtual bool mqtt_receive(String type, String name, String topic, String payload) {
    LEAF_ENTER(L_DEBUG);
    bool handled = Leaf::mqtt_receive(type, name, topic, payload);
    bool val = false;
    int bit = parse_channel(payload);

    LEAF_NOTICE("%s [%s]", topic.c_str(), payload.c_str());

  
    WHEN("cmd/status",{
      status_pub();
    })
    ELSEWHEN("cmd/set",{
      set_channel_bool(bit, true);
      status_pub();
    })
    ELSEWHEN("cmd/clear",{
      set_channel_bool(bit, false);
      status_pub();
    })
    else if(topic.startsWith("cmd/pwm/")) {
      bit = parse_channel(topic.substring(8));
      int value = payload.toInt();
      set_channel_pwm(bit, value);
      status_pub();
      handled = true;
    }
    ELSEWHEN("cmd/toggle",{
      if (bits_out & (1<<bit)) {
	set_channel_bool(bit, false);
      }
      else {
	set_channel_bool(bit, true);
      }
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
