//
//@**************************** class INA219Leaf ******************************
//
// This class encapsulates a ina219 voltage/current sensor
//
#pragma once

#include <Wire.h>
#include "trait_wirenode.h"
#include "trait_pollable.h"
#include <Adafruit_INA219.h>

class INA219Leaf : public Leaf, public WireNode, public Pollable
{
public:
  INA219Leaf(String name, String target="", byte address=0, int sample_ms=500,int report_sec=900)
    : Leaf("ina219", name, NO_PINS, target)
    , Debuggable(name)
    , WireNode(name, address)
    , Pollable(sample_ms, report_sec)
  {
  }

  virtual void setup();
  virtual void loop();
  virtual void status_pub();
  virtual void mqtt_do_subscribe();
  virtual bool mqtt_receive(String type, String name, String topic, String payload, bool direct=false);
  virtual bool probe(int addr);

protected:
  String target;
  uint16_t id;
  Adafruit_INA219 *ina219 = NULL;
  float volts=0;
  float shunt_mv=0;
  float milliamps=0;
  float volts_delta = 0.25;
  float shunt_mv_delta = -1;
  float milliamps_delta = 50;
 
  virtual bool poll ();
};

bool INA219Leaf::probe(int addr) 
{
      LEAF_DEBUG("Probe 0x%x", (int)address);
      int v = read_register16(0x00, 500);
      if (v < 0) {
	LEAF_DEBUG("No response from I2C address %02x", (int)address);
	return false;
      }
      if (v != 0x399f) {
	LEAF_NOTICE("INA2129 Chip signature (%04x) not recognised at addr %02x", (int)v, (int)addr);
	return false;
      }
      id = v;
      LEAF_NOTICE("Detected ina219 (id=%04x) at address %02x ", (int)id, (int)address);
      return true;
}


void INA219Leaf::setup(void) {
  Leaf::setup();
  LEAF_ENTER(L_DEBUG);
  this->install_taps(target);

  registerLeafByteValue("i2c_addr", &address, "I2C address override for pin extender (decimal)");
  registerLeafFloatValue("volts_delta", &volts_delta, "Significant change hysteresis for absolute voltage (volts)");
  registerLeafFloatValue("shunt_mv_delta", &shunt_mv_delta, "Significant change hysteresis for shunt voltage (millivolts)");
  registerLeafFloatValue("milliamps_delta", &milliamps_delta, "Significant change hysteresis for current measurement (milliamps)");

  if (address == 0) {
    // autodetect the device
    LEAF_NOTICE("Address not specified, attempting to auto-probe");
    
    for (address = 0x40; address <= 0x4F; address++) {
      if (probe(address)) {
	break;
      }
    }
    if (address > 0x4F) {
      LEAF_ALERT("  INA219 current sensor not found in range 0x40-0x4F");
      address = 0;
      stop();
      return;
    }
  }
  else {
    if (!probe(address)) {
      LEAF_ALERT("  INA219 current sensor not found at 0x%02x", address);
      stop();
      return;
    }
  }
  
  ina219 = new Adafruit_INA219(address);
  ina219->begin();
  LEAF_NOTICE("Set INA219 calibration values to 32V_2A");
  ina219->setCalibration_32V_2A();

  registerCommand(HERE,"poll","read and report the sensor inputs");

  LEAF_LEAVE;
}

bool INA219Leaf::poll()
{
  LEAF_ENTER(L_DEBUG);
  static bool first = false;

  bool result = false;
  
  if (!ina219) {
    LEAF_ALERT("No device");
    return result;
  }
  
  if (first) {
    result=true;
    first=false;
  }
  
  float reading = ina219->getBusVoltage_V();
  float change = volts-reading;
  if (fabs(change)>=volts_delta) {
    volts = reading;
    if (volts_delta >= 0) {
      // negative threshold means never consider the change significant, but
      // always register it
      result = true;
      LEAF_INFO("volts=%.3f change=%.3f", volts, change);
    }
  }

  reading = ina219->getShuntVoltage_mV();
  change = shunt_mv-reading;
  if (fabs(change) >= shunt_mv_delta) {
    shunt_mv = reading;
    if (shunt_mv_delta >=0) {
      result = true;
      LEAF_INFO("shunt_mv=%.3f (change=%.3f)", shunt_mv, change);
    }
  }

  reading = ina219->getCurrent_mA();
  change = milliamps-reading;
  if (fabs(change) >= milliamps_delta) {
    milliamps = reading;
    if (milliamps_delta >= 0) {
      result = true;
      LEAF_INFO("milliamps=%.3f (change=%.3f)", milliamps, change);
    }
  }

  LEAF_LEAVE;
  return result;
}

void INA219Leaf::status_pub()
{
  LEAF_ENTER(L_INFO);
  LEAF_NOTICE("INA219 volts=%.3f shunt=%.3f milliamps=%.3f", volts, shunt_mv, milliamps);
  publish("status/millivolts", String(volts*1000,3));
  //publish("status/shunt_mv", String(shunt_mv));
  publish("status/milliamps", String(milliamps,3));
  LEAF_LEAVE;
}

void INA219Leaf::loop(void) {
  //LEAF_ENTER(L_DEBUG);

  if (!address) {
    return;
  }
  Leaf::loop();

  pollable_loop();
  //LEAF_LEAVE;
}

void INA219Leaf::mqtt_do_subscribe() 
{
  Leaf::mqtt_do_subscribe();
}


  // 
  // MQTT message callback
  //
bool INA219Leaf::mqtt_receive(String type, String name, String topic, String payload, bool direct) {
  LEAF_ENTER(L_INFO);
  bool handled = false;

  WHEN("cmd/poll", {
      LEAF_NOTICE("handling cmd/poll");
      poll();
      status_pub();
      handled=true;
    })
  ELSEWHEN("cmd/status", {
      LEAF_NOTICE("handling cmd/status");
      status_pub();
      handled=true;
    });
    
  
  if (!handled) {
    // pass to superclass
    handled = Leaf::mqtt_receive(type, name, topic, payload, direct);
  }
      
    
  return handled;
}
    



// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
