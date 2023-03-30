
//
// This class encapsulates the JoyOfClicks, a 5-way-joystick plus 3 button keypad using PCF8574
#pragma once

#include "leaf_pinextender_pcf8574.h"

class JoyOfClicksLeaf : public PinExtenderPCF8574Leaf
{
public:
  static constexpr const char *input_names = "down,center,right,left,up,but_c,but_b,but_a";

  JoyOfClicksLeaf(String name, int address=0x20, uint8_t orientation=0)
    : PinExtenderPCF8574Leaf(name, address, JoyOfClicksLeaf::input_names)
    , Debuggable(name)
  {
    this->orientation=orientation;
    this->publish_bits=false;
    this->bits_inverted=0;
  }
  
protected:
  uint8_t orientation;

  virtual void setup(void) 
  {
    LEAF_NOTICE("bits_out=%02x bit_in=%02x bits_inverted=%02x bits_input=%02x", (int)bits_out, (int)bits_in, (int)bits_inverted, (int)bits_input);
    PinExtenderPCF8574Leaf::setup();
    setOrientation(orientation);
  }

  void setOrientation(uint8_t orientation) 
  {
  }

  virtual void status_pub()
  {
    LEAF_ENTER(L_DEBUG);

    char msg[64];
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
      if ((last & mask) != (bits_in & mask)) {
	String  event = "press";
	if ((bits_in & mask) == HIGH) {
	  event = "release");
	LEAF_NOTICE("%7s %s", event.c_str(), pin_names[c].c_str());
	mqtt_publish(String("event/button/")+pin_names[c], event);
      }
    }

    LEAF_LEAVE;
  }
};


// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
