bn#pragma once

#include "trait_wirenode.h"

// SMBus is I2C plus a hinky CRC mechanism

class SMBusNode: public WireNode, virtual public Debuggable
{
public:
  SMBusNode(String name, byte _address=0, TwoWire *_wire=&Wire)
    : WireNode(name, _address, _wire)
    , Debuggable(name)
  {
  }

  /** @brief Generates a CRC byte (8-bit) for SM-BUS Packet Error Correction
   *         using X8+X2+X1+1 for calculation
   *
   *  Algorithm for calculating CRC-8 is as follows:
   *    - Initialize shift register with all zeros as contents
   *    - Shift left the message until a 1 comes out
   *    - XOR the contents of the register with the low eight bits of the CRC Polynomial
   *    - Continue until the whole message has passed though the shift register
   *    - Last value out of the shift regeister is the PEC byte. 
   *
   *  @param acc_in  -  byte CRC value, or 0 for start of new CRC
   *  @param data_in - New byte to be added to CRC calculation
   *  @return        - The generated CRC used to send for PEC or continue calculating CRC
   */
  byte doCRC(byte acc_in, byte data_in) 
  {
    const byte CRCPOLY = 0x07;
    byte crc = 0;                  // Initialize PEC variable to 0
    crc = acc_in ^ data_in;        // XOR existing PEC or 0 (byteI_n) and new data (dataIn)
    for (int i=0;i<8;i++) {      // Step through each bit of the modified value 
      if (crc & 0x80) {          // Check to see if MSB is HIGH 
	crc = (crc<<1) & 0xFF;   // If MSB is HIGH, shift contents up
	crc ^= CRCPOLY;          // Then XOR with the crcPoly variable
      }
      else {
	crc = (crc<<1)&0xFF;     // If MSB is LOW, shift contents up
      }
    }
    LEAF_TRACE("doCRC acc_in=0x%02x (%d) data_in=0x%02x (%d) => 0x%02x (%d)",
	      (unsigned int)acc_in, (unsigned int)acc_in,
	      (unsigned int)data_in, (unsigned int)data_in,
	      (unsigned int)crc, (unsigned int)crc);

    return crc;                  // Once all bits shifted through, return PEC
  }

  bool verifyReadCRC(byte candidate_crc, byte *data, int count) 
  {
    // read CRC is over the chip address and the data, but oddly not the register address
    //byte readCA = (0xA7 & (address<<1))&0xFF;
    byte addr = address;
    byte readCA = 0xA7 | ((addr << 1)&0xFF);

    LEAF_TRACE("verifyReadCRC count=%u address=%u readCA=%u candidate=%u",
	      count, (unsigned int)addr, (unsigned int)readCA, (unsigned int)candidate_crc);
    
    byte crc = doCRC(0, readCA);
    for (int i=0; i<count; i++) {
      crc = doCRC(crc, data[i]);
    }
    if (crc == candidate_crc) {
      return true;
    }
    LEAF_WARN("CRC mismatch on read of %d, received 0x%02x (%d) calculated 0x%02x (%d)",
	      count, (int)candidate_crc, (int)candidate_crc, (int)crc, (int)crc);
    return false;
  }

  byte generateWriteCRC(byte reg, byte *b, int count) 
  {
    // write CRC is over the chip address, register address and data
    byte writeCA = (0xA0 | (address<<1))&0xFF;

    byte crc = doCRC(0, writeCA);
    crc = doCRC(crc, reg);
    for (int i=0; i<count; i++) {
      crc = doCRC(crc, b[i]);
    }
    return crc;
  }
  
  bool readByte(int reg, byte *b) 
  {
    unsigned long start = millis();

    wire->beginTransmission(address);
    wire->write(reg);
    wire->endTransmission();
    wire->requestFrom((int)address, (int)2);

    if (!wire->available()) {
      LEAF_WARN("Data byte not available at 0x%02x:0x%02x", (int)address, reg);
      return false;
    }
    byte input = wire->read();

    if (!wire->available()) {
      LEAF_WARN("CRC byte not available at 0x%02x:0x%02x", (int)address, reg);
      return false;
    }
    byte candidate_crc = wire->read();

    LEAF_DEBUG("Read [byte,CRC] pair from 0x%02x:0x%02x: [0x%02x, 0x%02x]", (int)address, (int)reg, (int)input, (int)candidate_crc);

    if (verifyReadCRC(candidate_crc, &input, 1)) {
      if (b) {
	*b = input;
      }
      return true;
    }
    LEAF_ALERT("Read CRC error at address 0x%02x register 0x%02x", (int)address, (int)reg);
    return false;
  }
  
  bool readSingleBytes(int reg, int count, byte *buf)
  {
    bool result = true;
    for (int i=0; i<count; i++) {
      result &= readByte(reg+i, buf+i);
    }
    return result;
  }

  bool writeByte(int reg, byte b) 
  {
    byte crc = generateWriteCRC(reg, &b, 1);

    wire->beginTransmission(address);
    wire->write(reg);
    wire->write(b);
    wire->write(crc);
    if (wire->endTransmission()) {
      return true;
    }
    LEAF_ALERT("Write transaction failed at device 0x%02x register 0x%02x (data=0x%02x)",
	       (int)address, (int)reg, (int)b);
    return false;
  }
  

};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
