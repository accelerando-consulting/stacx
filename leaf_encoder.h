//
//@**************************** class EncoderLeaf ******************************
//
// This class encapsulates a simple pushbutton that publishes to MQTT when it
// changes state
//
#pragma once
#pragma STACX_LIB Encoder
#include <Encoder.h>

class EncoderLeaf : public Leaf
{
public:
  Encoder *encoder=NULL;
  int pin1 = -1;
  int pin2 = -1;
  bool relative = false;
  long position = -999;


  EncoderLeaf(String name, int pin1, int pin2, bool relative=false)
    : Leaf("encoder", name, NO_PINS)
    , Debuggable(name)
  {
    this->do_heartbeat = false;
    this->pin1 = pin1;
    this->pin2 = pin2;
    this->relative=relative;
    
  }

  virtual void setup(void) {
    Leaf::setup();
    LEAF_ENTER(L_INFO);
    registerLeafIntValue("pin1", &pin1, "Pin number of encoder pin1"); // runtime override
    registerLeafIntValue("pin2", &pin2, "Pin number of encoder pin2"); // runtime override
    encoder = new Encoder(pin1, pin2);
    //registerLeafBoolValue("pullup", &pullup, "Enable button pullup");
    //registerLeafBoolValue("active", &active, "Button active value (0=LOW 1=HIGH)");

    LEAF_NOTICE("%s claims pins %d,%d as INPUT", describe().c_str(), pin1, pin2);

    LEAF_LEAVE;
  }

  virtual void start(void) 
  {
    Leaf::start();
    LEAF_ENTER(L_INFO);
    LEAF_LEAVE;
  }
  

  virtual void status_pub()
  {
    mqtt_publish("status/encoder", String(position));
    
  }

  virtual void loop(void) {
    Leaf::loop();

    long newPosition;
    if (relative) {
      newPosition = encoder->readAndReset();
    }
    else {
      newPosition = encoder->read();
    }
      
    if (newPosition != position) {
      position = newPosition;
      LEAF_INFO("Encoder position %d", position);
      if (position != 0) {
	mqtt_publish("event/position", String(position));
      }
    }

#if 0
    if ((active==LOW)?button.fell():button.rose()) {
      mqtt_publish("event/press", String(millis(), DEC));
      changed = true;
    }
    if ((active==LOW)?button.rose():button.fell()) {
      mqtt_publish("event/release", String(millis(), DEC));
      changed = true;
    }
    if (changed) status_pub();
#endif
  }
  
};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
