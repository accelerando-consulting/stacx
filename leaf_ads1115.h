//
//@**************************** class ADS1115Leaf ******************************
//
// This class encapsulates an ADS1115 quad-channel analog sensor
//
#pragma once

#include <Wire.h>
#include "trait_wirenode.h"
#include "trait_pollable.h"
#include <ADS1X15.h>

class ADS1115Leaf : public Leaf, public WireNode, public Pollable
{
public:
  ADS1115Leaf(String name, String target="", byte address=0, int sample_ms=500,int report_sec=900)
    : Leaf("ads1115", name, NO_PINS, target)
    , Debuggable(name)
    , WireNode(name, address)
    , Pollable(sample_ms, report_sec)
  {
  }

  virtual void setup();
  virtual void loop();
  virtual void status_pub();
  virtual bool commandHandler(String type, String name, String topic, String payload);
  
protected:
  String target;
  uint16_t id;
  ADS1115 *ads1115 = NULL;
  bool chenable[4]={true,false,false,false};
  bool changed[4]={false,false,false,false};
    
  float volts[4]={0,0,0,0};
  float volts_delta = 0.01;
 
  virtual bool poll ();
};


void ADS1115Leaf::setup(void) {
  Leaf::setup();
  LEAF_ENTER(L_DEBUG);
  this->install_taps(target);

  registerLeafBoolValue("ch1_enable", chenable, "Channel 1 enable");
  registerLeafBoolValue("ch2_enable", chenable+1, "Channel 2 enable");
  registerLeafBoolValue("ch3_enable", chenable+2, "Channel 3 enable");
  registerLeafBoolValue("ch4_enable", chenable+3, "Channel 4 enable");
  registerLeafByteValue("i2c_addr", &address, "I2C address override for ads1115 (decimal)");
  registerLeafFloatValue("volts_delta", &volts_delta, "Significant change hysteresis for absolute voltage (volts)");
  registerCommand(HERE,"poll","read and report the sensor inputs");

  if (address == 0) {
    // autodetect the device
    LEAF_NOTICE("Address not specified, attempting to auto-probe");
    
    for (int address = 0x48; address <= 0x51; address++) {
      if (probe(address)) {
	break;
      }
    }
    if (address > 0x51) {
      LEAF_WARN("Failed to detect ads1115");
      address = 0;
      stop();
      return;
    }
  }
  else {
    if (!probe(address)) {
      address=0;
      LEAF_ALERT("ADS1115 current sensor not found at 0x%02x",(int)address);
      stop();
      return;
    }
  }
  
  ads1115 = new ADS1115(address);
  ads1115->begin();
  ads1115->setGain(0);  // 6.144 volt

  LEAF_LEAVE;
}

bool ADS1115Leaf::poll()
{
  LEAF_ENTER(L_DEBUG);
  static bool first = false;

  bool result = false;
  
  if (!ads1115) {
    LEAF_ALERT("No device");
    return result;
  }
  
  if (first) {
    result=true;
    first=false;
  }

  for (int c=0; c<4; c++) {
    if (!chenable[c]) continue;
    
    float reading = ads1115->readADC(c);
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
  }
  
  LEAF_LEAVE;
  return result;
}

void ADS1115Leaf::status_pub()
{
  LEAF_ENTER(L_INFO);

  for (int c=0; c<4; c++) {
    if (!chenable[c] || !changed[c]) continue;
    
    LEAF_NOTICE("ADS1115 Ch%d volts=%.3f", c, volts[c]);
    publish("status/millivolts"+String(c+1), String(volts[c]*1000,3));
    changed[c]=false;
  }
  
  LEAF_LEAVE;
}

void ADS1115Leaf::loop(void) {
  //LEAF_ENTER(L_DEBUG);

  if (!address) {
    return;
  }
  Leaf::loop();

  pollable_loop();
  //LEAF_LEAVE;
}

bool ADS1115Leaf::commandHandler(String type, String name, String topic, String payload) {
  LEAF_HANDLER(L_INFO);

  WHEN("poll", {
    poll();
    status_pub();
  })
  ELSEWHEN("status", {
    for (int c=0;c<4;c++) changed[c]=chenable[c];
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
