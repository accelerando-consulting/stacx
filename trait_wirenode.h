#ifndef _TRAIT_WIRENODE_H
#define _TRAIT_WIRENODE_H

#include <Wire.h>

class WireNode 
{
  
protected:
  byte address;
  TwoWire *wire;
  
  void setWireBus(TwoWire *w) 
  {
    wire=w;
  }

  bool probe(int addr) {
    DEBUG("WireNode probe 0x%x", (int)address);
    int v = read_register16(0x00, 500);
    if (v < 0) {
      DEBUG("No response from i2c address %02x", (int)address);
      return false;
    }
    return true;
  }

  int read_register(byte reg, int timeout=1000) 
  {
    int v;
    unsigned long start = millis();
    
    wire->beginTransmission(address);
    wire->write(reg);
    wire->endTransmission();

    wire->requestFrom((int)address, (int)1);
    while (!wire->available()) {
      unsigned long now = millis();
      if ((now - start) > timeout) return -1;
    }
    v = wire->read();
    return v;
  }

  int read_register16(byte reg, int timeout=1000) 
  {
    int v;
    unsigned long start = millis();
    
    wire->beginTransmission(address);
    wire->write(reg);
    wire->endTransmission();

    wire->requestFrom((int)address, (int)2);
    while (!wire->available()) {
      unsigned long now = millis();
      if ((now - start) > timeout) return -1;
    }
    v = (wire->read()<<8);

    while (!wire->available()) {
      unsigned long now = millis();
      if ((now - start) > timeout) return -1;
    }
    v |= wire->read();
    return v;
  }

  void write_register(byte reg, byte value, int timeout=1000) 
  {
    unsigned long start = millis();

    wire->beginTransmission(address);
    wire->write(reg);
    wire->write(value);
    wire->endTransmission();
  }

  void set_bit(byte &v, byte b) 
  {
    v |= (1<<b);
  }

  void clear_bit(byte &v, byte b) 
  {
    v &= ~(1<<b);
  }

}
;

#endif
// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
