//
//@**************************** class INA3221Leaf ******************************
//
// This class encapsulates a ina3211 triple-channel voltage/current sensor
//
#pragma once

#include <Wire.h>
#include "trait_wirenode.h"
#include "trait_pollable.h"
#include <INA3221.h>

class INA3221Leaf : public Leaf, public WireNode, public Pollable
{
public:
  INA3221Leaf(String name, String target="", byte address=0, int sample_ms=500,int report_sec=900)
    : Leaf("ina3221", name, NO_PINS, target)
    , Debuggable(name)
    , WireNode(name, address)
    , Pollable(sample_ms, report_sec)
  {
  }

  virtual void setup();
  virtual void loop();
  virtual void status_pub();
  virtual bool commandHandler(String type, String name, String topic, String payload);
  
  virtual bool mqtt_receive(String type, String name, String topic, String payload, bool direct=false);
  virtual bool probe(int addr);

protected:
  String target;
  uint16_t id;
  INA3221 *ina3221 = NULL;
  bool chenable[3]={true,false,false};
  bool changed[3]={false,false,false};
    
  float volts[3]={0,0,0};
  float milliamps[3]={0,0,0};
  float volts_delta = 0.25;
  float milliamps_delta = 50;
 
  virtual bool poll ();
};


bool INA3221Leaf::probe(int addr) 
{
      LEAF_DEBUG("Probe 0x%x", (int)address);
      int v = read_register16(0xFE, 500);
      if (v < 0) {
	LEAF_DEBUG("No response from I2C address %02x", (int)address);
	return false;
      }
      if (v != 0x5449) {
	LEAF_NOTICE("INA3221 Chip signature (%04x) not recognised at addr %02x", (int)v, (int)addr);
	return false;
      }
      id = v;
      LEAF_NOTICE("Detected ina3221 (id=%04x) at address %02x ", (int)id, (int)address);
      return true;
}

void INA3221Leaf::setup(void) {
  Leaf::setup();
  LEAF_ENTER(L_DEBUG);
  this->install_taps(target);

  registerLeafBoolValue("ch1_enable", chenable, "Channel 1 enable");
  registerLeafBoolValue("ch2_enable", chenable+1, "Channel 2 enable");
  registerLeafBoolValue("ch3_enable", chenable+2, "Channel 3 enable");
  registerLeafByteValue("i2c_addr", &address, "I2C address override for ina3221 (decimal)");
  registerLeafFloatValue("volts_delta", &volts_delta, "Significant change hysteresis for absolute voltage (volts)");
  registerLeafFloatValue("milliamps_delta", &milliamps_delta, "Significant change hysteresis for current measurement (milliamps)");
  registerCommand(HERE,"poll","read and report the sensor inputs");

  if (address == 0) {
    // autodetect the device
    LEAF_NOTICE("Address not specified, attempting to auto-probe");
    
    for (0x40; address <= 0x43; address++) {
      if (probe(address)) {
	break;
      }
    }
    if (address > 0x43) {
      LEAF_WARN("Failed to detect 3221");
      address = 0;
      stop();
      return;
    }
  }
  else {
    if (!probe(address)) {
      address=0;
      LEAF_ALERT("INA3221 current sensor not found");
      stop();
      return;
    }
  }
  
  ina3221 = new INA3221(address);
  ina3221->begin();
  ina3221->reset();
  ina3221->setShuntRes(100, 100, 100);

  LEAF_LEAVE;
}

bool INA3221Leaf::poll()
{
  LEAF_ENTER(L_DEBUG);
  static bool first = false;

  bool result = false;
  
  if (!ina3221) {
    LEAF_ALERT("No device");
    return result;
  }
  
  if (first) {
    result=true;
    first=false;
  }

  for (int c=0; c<3; c++) {
    
    float reading = ina3221->getVoltage(c);
    float change = volts[c]-reading;
    if (fabs(change)>=volts_delta) {
      volts[c] = reading;
      if (volts_delta >= 0) {
	// negative threshold means never consider the change significant, but
	// always register it
	changed[c] = result = true;
	LEAF_INFO("CH%d volts=%.3f change=%.3f", c, volts, change);
      }
    }
    
    reading = ina219->getCurrent();
    change = milliamps[c]-reading;
    if (fabs(change) >= milliamps_delta) {
      milliamps[c] = reading;
      if (milliamps_delta >= 0) {
	changed[c] = result = true;
	LEAF_INFO("CH%d milliamps=%.3f (change=%.3f)", c, milliamps, change);
      }
    }
  }
  
  LEAF_LEAVE;
  return result;
}

void INA219Leaf::status_pub()
{
  LEAF_ENTER(L_INFO);

  for (int c=0; c<3; c++) {
    if (!chenable[c] || !changed[c]) continue;
    
    LEAF_NOTICE("INA3221 Ch%d volts=%.3f milliamps=%.3f", c, volts[c], milliamps[c]);
    publish("status/millivolts"+String(c+1), String(volts[c]*1000,3));
    publish("status/milliamps"+String(c+1), String(milliamps[c],3));
    changed[c]=false;
  }
  
  LEAF_LEAVE;
}

void INA3221Leaf::loop(void) {
  //LEAF_ENTER(L_DEBUG);

  if (!address) {
    return;
  }
  Leaf::loop();

  pollable_loop();
  //LEAF_LEAVE;
}



bool INA3221Leaf::commandHandler(String type, String name, String topic, String payload) {
  LEAF_HANDLER(L_INFO);

  WHEN("poll", {
    poll();
    status_pub();
  })
  ELSEWHEN("status", {
    for (int c=0;c<3;c++) changed[c]=chenable[c];
    status_pub();
  })
  else {
    handled = Leaf::commandHandler(type, name, topic, payload);
  }

  LEAF_HANDLER_END;
}


// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
