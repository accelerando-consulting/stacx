#ifndef _TRAIT_WIRENODE_H
#define _TRAIT_WIRENODE_H

class WireNode 
{
protected:
  byte address;
  TwoWire *wire;
  
  // TODO: pass the wire bus as a parameter

  int read_register(byte reg, int timeout=1000) 
  {
    byte v;
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
