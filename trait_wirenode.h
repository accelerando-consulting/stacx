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
  static const bool BYTE_ORDER_LE = true;
  static const bool BYTE_ORDER_BE = false;
  

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

  virtual bool probe(byte addr, codepoint_t where=undisclosed_location, int level=L_NOTICE) {
    __LEAF_DEBUG_AT__(CODEPOINT(where), level, 
		      "WireNode probe 0x%x", (int)addr);
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

  int read_register(byte reg, int timeout=1000, int address=-1)
  {
    LEAF_ENTER_INT(L_DEBUG, (int)reg);
    int v;
    unsigned long start = millis();
    if (address < 0) address = this->address;

    wire->beginTransmission(address);
    wire->write(reg);
    wire->endTransmission();

    wire->requestFrom((int)address, (int)1);
    while (!wire->available()) {
      unsigned long now = millis();
      if ((now - start) > timeout) return -1;
    }
    v = wire->read();
    LEAF_INT_RETURN(v);
  }

  int read_register16(byte reg, int timeout=1000, bool little_endian=false, int address = -1)
  {
    LEAF_ENTER_INT(L_DEBUG, (int)reg);
    int v;
    if (address < 0) address = this->address;
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
    LEAF_INT_RETURN(v);
  }

  int read_register24(byte reg, int timeout=1000, int address = -1)
  {
    int v=0;
    if (address < 0) address = this->address;
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

  void write_register(byte reg, byte value, int timeout=1000, int address = -1)
  {
    LEAF_ENTER_INTPAIR(L_DEBUG, (int)reg, (int)value);
    if (address < 0) address = this->address;
    unsigned long start = millis();

    wire->beginTransmission(address);
    wire->write(reg);
    wire->write(value);
    wire->endTransmission();
    LEAF_VOID_RETURN;
  }

  void write_register16(byte reg, uint16_t value, int timeout=1000, bool little_endian = false, int address = -1)
  {
    if (address < 0) address = this->address;
    unsigned long start = millis();

    wire->beginTransmission(address);
    wire->write(reg);
    if (little_endian) {
      wire->write(value&0XFF);
      wire->write((value>>8)&0xFF);
    }
    else {
      wire->write((value>>8)&0xFF);
      wire->write(value&0XFF);
    }
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
