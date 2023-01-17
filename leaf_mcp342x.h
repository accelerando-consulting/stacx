//
//@**************************** class MCP342xLeaf ******************************
//
// This class encapsulates the MCP342[124] differential ADC
//
#pragma once

#include <Wire.h>
#include "trait_pollable.h"
#include "trait_wirenode.h"

class MCP342xLeaf : public Leaf, public WireNode, public Pollable
{
protected:
  int channels;
  int gain;
  int precision;

  bool found;
  int channel=1;
  uint8_t config;
  uint8_t config_in;
  int fsr;

  uint32_t raw[4];
  float volts[4];
  float peak[4];
  float rms[4];

  float rms_hist[4][100];
  
public:
  MCP342xLeaf(String name, int address=0x68,
	      int channels=2, int gain=1, int precision=12
    )
    : Leaf("mcp342x", name, NO_PINS)
    , WireNode(address)
    , Pollable(10, 1)
    , Debuggable(name)
  {
    LEAF_ENTER(L_INFO);
    found = false;
    this->channels=channels;
    this->gain=gain;
    this->precision=precision;
    this->channel=0;
    LEAF_LEAVE;
  }

  virtual void setup(void) {
    Leaf::setup();

    LEAF_ENTER(L_NOTICE);
    //wire->begin();

    // continuous mode, default settings
    config = 0x10;

    if (!probe(address)) {
      LEAF_ALERT("   MCP3422 NOT FOUND at 0x%02x", address);
      address=0;
      LEAF_VOID_RETURN;
    }

    if (write_config(config) != 0) {
      LEAF_ALERT("    MCP3422 config write failed");
      found=false;
      return;
    }
    NOTICE("    MCP3422 at 0x%02x", address);
    found=true;
    
    set_precision(precision);
    set_gain(gain);
    
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
    LEAF_NOTICE("mcp342x_write_config addr=%02x cfg=0x%02x\n", address, (int)b);
    wire->beginTransmission(address);

    int rc = Wire.write(b);
    if (rc != 0) {
      LEAF_ALERT("MCP342x config write failled, returned %02x", rc);
      return -1;
    }
    if (wire->endTransmission(true) != 0) {
      LEAF_ALERT("MCP342x transaction failed");
      return -1;
    }
    config = b;
    return 0;
  }

  int set_channel(uint8_t c) 
  {
    LEAF_DEBUG("   set_channel %d\n", (int)c);
    // set bits 6-5 to 00 (default channel 1)
    channel = c;
  
    config &= ~0x60;
    config |= ((channel-1) & 0x03)<<5;
    if (write_config(config) != 0) {
      LEAF_ALERT("MCP342x config write failed");
    }
    return 0;
  }

  int set_precision(int bits) 
  {
    // clear bits 3-2 to 00 (default channel 1)
    config &= ~0x06;
    precision = bits;
    fsr = 1<<(bits-1);

    switch (bits) {
    case 12:
      // bits 3-2 = 00  = 12bits 240sps
      break;
    case 14:
      // bits 3-2 = 01  = 14bits 60sps
      config |= 0x02;
      break;
    case 16:
      // bits 3-2 = 10 = 16bits 15sps
      config |= 0x04;
      break;
    case 18:
      // bits 3-2 = 11 = 18 bits 3.75sps
      config |= 0x06;
      break;
    default:
      LEAF_ALERT("Invalid sample precision command");
    }
  
    if (write_config(config) != 0) {
      LEAF_ALERT("MCP342x config write failed");
    }
    LEAF_NOTICE("set_precision %dbit FSR=%d\n", (int)bits, (int)fsr);
    return 0;
  }

  int set_gain(int gain) 
  {
    // clear bits 1-0 to 00 (default gain x1)
    config &= ~0x03;
    gain = gain;

    switch (gain) {
    case 1:
      // bits 1-0 = 00  = 1x
      break;
    case 2:
      // bits 1-0 = 01  = 2x
      config |= 0x01;
      break;
    case 4:
      // bits 1-0 = 10 = 4x
      config |= 0x02;
      break;
    case 8:
      // bits 1-0 = 11 = 8x
      config |= 0x03;
      break;
    default:
      LEAF_ALERT("Invalid gain value %d\n", gain);
    }
  
    if (write_config(config) != 0) {
      LEAF_ALERT("MCP342x config write failed");
    }
    LEAF_NOTICE("set_gain %dx\n", (int)gain);
    return 0;
  }

  int convert(uint8_t buf[4], int precision, int gain, uint32_t *raw_r, float *result_r, uint8_t *config_r) 
  {
    uint8_t cfg;
    uint32_t raw;
    int32_t result;
  
    if (precision == 18) {
      // 18 bit mode, 3 bytes of ADC + 1 config
      raw = (buf[0]<<16)|(buf[1]<<8)|buf[2];
      cfg = buf[3];
      //Serial.printf("\n    mcp3422_convert: raw %dbit value is 0x%02x%02x%02x (%lu) config_in=0x%02x\n",
      //precision,
      //buf[0],buf[1],buf[2], raw, (int)cfg);
    }
    else {
      // 16,16,12 bite modes all 2 bytes ADC + 1 config
      raw = (buf[0]<<8)|buf[1];
      cfg = buf[2];
      //Serial.printf("\n    mcp3422_convert: raw %dbit value is 0x%02x%02x (%lu) config_in=0x%02x\n",
      //precision, buf[0],buf[1], raw, (int)cfg);
    }

    if (config_r) {
      *config_r = cfg;
    }

    int32_t msb = 1<<(precision-1);
    uint32_t mask = msb-1;
    if (raw & msb) {
      // twos complement negation is flip all the bits and add one, but we
      // need to cope with precision of 12,14,16 or 18 bts
      //Serial.printf("\tInverting uint32 %x\n", raw);

      raw = ~raw;
      //Serial.printf("\tFlipped bits are %x\n", raw);

      raw = raw & mask;
      //Serial.printf("\tLowest %d significant bits masked off using %x are %x\n", precision, mask, raw);

      raw = raw + 1;
      //Serial.printf("\tADD ONE TO RAW GIVING %x\n", raw);
    
      result = 0-raw;
      //Serial.printf("\tValue interpreted as negative, inverted giving %d\n", result);
    }
    else {
      // positive value
      result = raw;
    }

    if ((result > msb) || (result < -msb)) {
      // value out of range, probably no connection
      //Serial.printf("\tTreating value 0x%x as out of range (msb=0x%x)\n", result, msb);
      return -1;
    }
    if (raw_r) {
      *raw_r = raw;
      //Serial.printf("    returned raw result is 0x%x\n", *raw_r);
    }

    float volts = (result * 2.048) / (msb * gain);
    //Serial.printf("    mcp3422_convert raw value 0x%x@%dbps with gain %d (fsr %d) => %3fv\n\n", raw, precision, gain, msb, volts);
    if (result_r) {
      *result_r = volts;
    }

    return 0;
  }

  int read(uint32_t *result_raw, float *result) 
  {
    //Serial.printf("mcp3422_read channel = %d\n", mcp3422_channel);
  
    unsigned long start = millis();
    uint8_t buf[4];
    int count = 0;
    int wantbytes = (precision==18)?4:3;
    int32_t raw;
  
    wire->requestFrom((uint8_t)address, (uint8_t)wantbytes);
    while (count < 3) {
      while (!wire->available()) {
	unsigned long now = millis();
	if ((now - start) > 1000) {
	  LEAF_ALERT("Timeout waiting for MCP342x byte %d of %d\n", count+1, wantbytes);
	  return -1;
	}
      }
      buf[count] = wire->read();
    
      count++;
    }

    if ((buf[wantbytes-1] & 0x80)==1) {
      INFO("No new conversion, config=0x%02x\n", (int)buf[wantbytes-1]);
      return -1;
    }

    return convert(buf, precision, gain, result_raw, result, &config_in);
  }


  virtual bool poll()
  {
    if (!found) return false;

    LEAF_ENTER(L_DEBUG);

    // 
    // read the current channel, return "no change" if failed
    // 
    uint32_t new_raw;
    float new_volts;
    if (read(&new_raw, &new_volts)!=0) {
      LEAF_RETURN(false);
    }
    LEAF_NOTICE("MCP channel %d: %.4f", channel, new_volts);
    

    // round-robin read the enabled channels
    //if (++channel>channels) channel=1;

    // 
    // If the value has changed, return true
    // 
    if (new_raw != raw[channel-1]) {
      raw[channel-1] = new_raw;
      volts[channel-1] = new_volts;
      LEAF_RETURN(true);
    }
    
    
    LEAF_RETURN(false);
  }

  void status_pub()
  {
    if (!found) return;

    char msg[64];
    int pos = 0;

    pos+=snprintf(msg, sizeof(msg), "[ ");
    for (int c=0;c<channels;c++) {
      pos += snprintf(msg+pos, sizeof(msg)-pos, "%.4f%c]", volts[c], (c==channels-1)?' ':',');
    }
    snprintf(msg+pos, sizeof(msg)-pos, "]");
    
    mqtt_publish("status/volts", msg);
  }

};


// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
