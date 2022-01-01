//
//@**************************** class BH1750Leaf ******************************
//
// This class encapsulates the BH1740 illumination sensor
//
#pragma once

#include <Wire.h>
#include "trait_pollable.h"
#include "trait_wirenode.h"

class BH1750Leaf : public Leaf, public WireNode, public Pollable
{
protected:
  bool found=false;
  bool cont=true;
  int mode=0;  // 0=H 1=H2 3=L
  uint16_t raw;
  
public:
  BH1750Leaf(String name, int address=0x23)
    : Leaf("bh1750", name, NO_PINS) {
    LEAF_ENTER(L_INFO);
    this->address=address;
    this->wire = &Wire;
    LEAF_LEAVE;
  }

  virtual void setup(void) {
    Leaf::setup();

    LEAF_ENTER(L_NOTICE);
    //wire->begin();

    // continuous mode, default settings
    uint8_t config = (cont?0x10:0x20)|(mode&0x03);
    
    if (!probe(address)) {
      LEAF_ALERT("   BH1750 NOT FOUND at 0x%02x", address);
      address=0;
      LEAF_VOID_RETURN;
    }

    if (write_config(config) != 0) {
      LEAF_ALERT("    BH1750 config write failed");
      found=false;
      return;
    }
    NOTICE("    BH1750 at 0x%02x", address);
    found=true;
    
    LEAF_LEAVE;
  }

  virtual void loop(void) {
    //LEAF_ENTER(L_DEBUG);

    if (!address) {
      return;
    }
    Leaf::loop();

    pollable_loop();
    //LEAF_LEAVE;
  }

  int write_config(uint8_t b) 
  {
    LEAF_NOTICE("bh1750 _config addr=%02x cfg=0x%02x\n", address, (int)b);
    wire->beginTransmission(address);

    int rc = Wire.write(b);
    if (rc != 0) {
      LEAF_ALERT("BH1750 config write failled, returned %02x", rc);
      return -1;
    }
    if (wire->endTransmission(true) != 0) {
      LEAF_ALERT("BH1750 transaction failed");
      return -1;
    }
    return 0;
  }

  int read(uint16_t *result_raw) 
  {
    unsigned long start = millis();
    uint8_t buf[2];
    int count = 0;
    int wantbytes = 2;
    int16_t raw;
  
    wire->requestFrom((uint8_t)address, (uint8_t)wantbytes);
    while (count < wantbytes) {
      while (!wire->available()) {
	unsigned long now = millis();
	if ((now - start) > 1000) {
	  LEAF_ALERT("Timeout waiting for BH1750 byte %d of %d\n", count+1, wantbytes);
	  return -1;
	}
      }
      buf[count] = wire->read();
    
      count++;
    }

    raw = (buf[0]<<8)|buf[1];
    if (result_raw) *result_raw=raw;
    return 0;
  }

  virtual bool poll()
  {
    if (!found) return false;

    LEAF_ENTER(L_DEBUG);

    // 
    // read the current channel, return "no change" if failed
    // 
    uint16_t new_raw;
    if (read(&new_raw)!=0) {
      LEAF_RETURN(false);
    }
    LEAF_NOTICE("BH1750 %d", (int)new_raw);

    if (new_raw != raw) {
      raw = new_raw;
      LEAF_RETURN(true);
    }
    
    LEAF_RETURN(false);
  }

  void status_pub()
  {
    if (!found) return;

    mqtt_publish("status/lux", String(raw, DEC));
  }

};


// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
