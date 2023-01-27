//
//@**************************** class PinExtenderPCA9685Leaf ******************************
//
// This class encapsulates the PCA9684 PWM IO extender (output only)
//
#pragma once

#include <Wire.h>
#include "trait_wirenode.h"
#include "PCA9685.h"
// TODO: make an abstract pin extender class

class PinExtenderPCA9685Leaf;

struct Pca9685OneshotContext
{
  int bit;
  class PinExtenderPCA9685Leaf *leaf;
};

struct Pca9685OneshotContext pca9685OneshotContext;


class PinExtenderPCA9685Leaf : public Leaf, public WireNode
{
protected:
  PCA9685 extender;
  uint32_t bits_out;
  byte pwm_out[16];
  bool pin_inverted[16];
  String pin_names[16];
  bool found;
  Ticker oneshotTimer;
public:
  PinExtenderPCA9685Leaf(String name, byte address=0x41, String names="")
    : Leaf("pinextender", name, NO_PINS)
    , WireNode(name, address)
    , Debuggable(name)
  {
    LEAF_ENTER(L_NOTICE);
    found = false;

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

    LEAF_ENTER(L_INFO);

    registerLeafByteValue("i2c_addr", &address, "I2C address override for pin extender (decimal)");

    if (!probe(address)) {
      LEAF_ALERT("   PCA9685 NOT FOUND at 0x%02x", (int)address);
      stop();
      address=0;
      LEAF_VOID_RETURN;
    }

    found=true;
    LEAF_NOTICE("%s claims I2C addr 0x%02x", describe().c_str(), (int)address);
    if (pin_names[0].length()) {
      for (int c=0; (c<16) && pin_names[0].length(); c++) {
	LEAF_INFO("%s   pin %02d is named %s%s", describe().c_str(), c, pin_names[c].c_str(), pin_inverted[c]?" (inverted)":"");
      }
    }

    extender.setupSingleDevice(*wire, (uint8_t)address);
    extender.setToServoFrequency();
    for (int c=0; c<16; c++) {
      extender.setChannelDutyCycle(c, pin_inverted[c]?100:0);
    }

    registerCommand(HERE, "cmd/set", "Set an output pin high");
    registerCommand(HERE, "cmd/clear", "Set an output pin low");
    registerCommand(HERE, "cmd/pwm/", "Set an output pin pwm/NAME to a given PWM duty cycle");
    registerCommand(HERE, "cmd/toggle", "Toggle the state of an output pin");

    LEAF_LEAVE;
  }

  void status_pub()
  {
    if (!found) return;
    LEAF_ENTER(L_NOTICE);

    char msg[64];
    char top[64];

    snprintf(top, sizeof(top), "status/%s/bits", leaf_name.c_str());
    snprintf(msg, sizeof(msg), "0x%02x", bits_out);

    mqtt_publish(top, msg);

    for (int bit=0; bit<16;bit++) {
      if (pin_names[bit].length()) {
	snprintf(top, sizeof(top), "status/%s/%s", leaf_name.c_str(), pin_names[bit].c_str());
      }
      else {
	snprintf(top, sizeof(top), "status/%s/%d", leaf_name.c_str(), bit);
      }
      if (pwm_out[bit]) {
	mqtt_publish(top, String((int)pwm_out[bit]));
      }
      else {
	mqtt_publish(top, String((bits_out&(1<<bit))?"on":"off"));
      }
    }
    LEAF_VOID_RETURN;
  }

  int parse_channel(String s) {
    for (int c=0; c<16 && pin_names[c].length(); c++) {
      if (pin_names[c] == s) return c;
    }
    if (!isdigit(s[0])) {
      LEAF_ALERT("Did not match pin name '%s'", s.c_str());
      return -1;
    }
    return s.toInt();
  }

  void set_channel_bool(int c, bool enable) {
    if (c < 0) return;
    pwm_out[c] = 0; // signify that the channel is boolean
    int duty;
    if (enable) {
      bits_out |= 1<<c;
      duty = pin_inverted[c]?0:100;
    }
    else {
      duty = pin_inverted[c]?100:0;
      bits_out &= ~(1<<c);
    }
    LEAF_INFO("BOOL channel %d (%s) <= (bool) %d", c, pin_names[c].c_str(), duty);
    //Serial.printf("%s %s %d\n",pin_names[c].c_str(), STATE(enable), duty);
    extender.setChannelDutyCycle(c, duty);
  }

  void set_channel_pwm(int c, int percent) {
    if (c < 0) return;
    pwm_out[c] = pin_inverted[c]?(100-percent):percent;
    if (percent) {
      bits_out |= 1<<c;
    }
    else {
      bits_out &= ~(1<<c);
    }
    NOTICE("PWM channel %d (%s) <= %d", c, pin_names[c].c_str(), pwm_out[c]);
    extender.setChannelDutyCycle(c, pwm_out[c]);
  }

  virtual bool mqtt_receive(String type, String name, String topic, String payload, bool direct=false) {
    LEAF_ENTER(L_DEBUG);
    bool handled = false;
    bool val = false;
    int bit;

    LEAF_INFO("RECV %s/%s => %s [%s]", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());


    WHEN("cmd/set",{
      bit = parse_channel(payload);
      set_channel_bool(bit, true);
    })
    ELSEWHEN("cmd/clear",{
      bit = parse_channel(payload);
      set_channel_bool(bit, false);
    })
    ELSEWHENPREFIX("cmd/pwm/", {
      bit = parse_channel(topic.substring(8));
      int value = payload.toInt();
      set_channel_pwm(bit, value);
      handled = true;
    })
    ELSEWHEN("cmd/toggle",{
      bit = parse_channel(payload);
      if (bits_out & (1<<bit)) {
	set_channel_bool(bit, false);
      }
      else {
	set_channel_bool(bit, true);
      }
    })
      ELSEWHENPREFIX("cmd/oneshot/",{
      bit = parse_channel(topic);
      int duration = payload.toInt();
      LEAF_NOTICE("Triggering operation (%dms) on channel %s", duration, payload.c_str());
      set_channel_bool(bit, true);
      pca9685OneshotContext.bit = bit;
      pca9685OneshotContext.leaf = this;
      oneshotTimer.once_ms(duration, [](){
	pca9685OneshotContext.leaf->set_channel_bool(pca9685OneshotContext.bit, false);
      });
    })
    else {
      handled = Leaf::mqtt_receive(type, name, topic, payload, direct);
    }

    LEAF_LEAVE;
    return handled;
  };
};


// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
