#ifndef _TRAIT_WIRENODE_H
#define _TRAIT_WIRENODE_H

#include <Wire.h>

#ifndef WIRE_UNIT
#define WIRE_UNIT &Wire
#endif

class WireNode: virtual public Debuggable
{
public:
  WireNode(String name, byte _address=0, TwoWire *_wire = WIRE_UNIT)
    : Debuggable(name)
  {
    this->wire = _wire;
    this->address = _address;
  }

protected:
  byte address=0;
  TwoWire *wire=NULL;

  void setWireBus(TwoWire *w)
  {
    wire=w;
  }

  bool setWireClock(uint32_t spd)
  {
    wire->setClock(spd);
    return true;
  }

  virtual bool probe(byte addr) {
    LEAF_NOTICE("WireNode probe 0x%x", (int)addr);
    if ((addr == 0) || (addr > 0x7F))  {
      LEAF_ALERT("Invalid probe address 0x%02x", addr);
      return false;
    }

    wire->beginTransmission(addr);
    int error = wire->endTransmission();
    if (error != 0) {
      LEAF_DEBUG("No response from I2C address %02x, error %d", (int)addr, error);
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

  int read_register16(byte reg, int timeout=1000, bool little_endian=false)
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
    if (little_endian) {
      v = wire->read();
    }
    else {
      v = (wire->read()<<8);
    }
    
    while (!wire->available()) {
      unsigned long now = millis();
      if ((now - start) > timeout) return -1;
    }
    if (little_endian) {
      v |= (wire->read()<<8);
    }
    else {
      v |= wire->read();
    }
    return v;
  }

  int read_register24(byte reg, int timeout=1000)
  {
    int v=0;
    unsigned long start = millis();

    wire->beginTransmission(address);
    wire->write(reg);
    wire->endTransmission();

    wire->requestFrom((int)address, (int)3);
    while (!wire->available()) {
      unsigned long now = millis();
      if ((now - start) > timeout) return -1;
    }
    v = (wire->read()<<16);

    while (!wire->available()) {
      unsigned long now = millis();
      if ((now - start) > timeout) return -1;
    }
    v |= (wire->read()<<8);

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

  void write_register16(byte reg, uint16_t value, int timeout=1000)
  {
    unsigned long start = millis();

    wire->beginTransmission(address);
    wire->write(reg);
    wire->write((value>>8)&0xFF);
    wire->write(value&0XFF);
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
