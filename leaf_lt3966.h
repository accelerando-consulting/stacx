//
//@************************** class LT3966Leaf ****************************
// 
// This class encapsulates a quad-channel booost-buck LED dimmer with current
// monitoring, PWM dimming and analog dimming.
//
//
#pragma once

#include "trait_smbusnode.h"
#include "trait_pollable.h"

class LT3966Leaf : public Leaf, public SMBusNode, public Pollable
{
  static const int channel_count = 4;
  static const int CHANNEL_ONE = 0;
  static const int CHANNEL_TWO = 1;
  static const int CHANNEL_THREE = 2;
  static const int CHANNEL_FOUR = 3;
  static const int REG_GLBCFG = 0x00 ;
  static const int REG_CH1 = 0x10 ;
  static const int REG_CH2 = 0x10 ;
  static const int REG_CH3 = 0x10 ;
  static const int REG_CH4 = 0x10 ;
  static const int REG_ADCCFG = 0x50 ;
  static const int REG_ADCVIN = 0x51 ;
  static const int REG_ADCCHN = 0x55 ;

protected:
  float cs_ohms = 0.33;
  float vs_factor = 1022.6 / 22.6; // 1M - 22k6 divider
  
  // Raw registers
  // (See the next block below for parsed out values)
  byte part_id[3]={0,0,0};
  byte GLBCFG=0;
  byte CHAN1[5];
  byte CHAN2[5];
  byte CHAN3[5];
  byte CHAN4[5];
  byte ADCCFG=0xC0;
  byte ADCVIN;
  byte ADCCHN[8];

  // Individual configuration/state values parsed from the above registers
  bool WDTFLAG=false;
  bool WDTEN=false;
  bool MPHASE=false;
  bool CLKOUT=false;

  bool channel_enable[LT3966Leaf::channel_count];
  bool channel_fault[LT3966Leaf::channel_count];
  bool channel_use_pwm[LT3966Leaf::channel_count];
  int channel_pwm_cycle_length[LT3966Leaf::channel_count]; // 64-8192 clock cycles
  int channel_pwm[LT3966Leaf::channel_count]; // 0--8191
  int channel_dim[LT3966Leaf::channel_count]; // 0--255

  bool adc_run;
  bool adc_auto;
  byte adc_clock_divider;
  byte adc_target;
  
  float input_voltage = NAN;
  float channel_voltage[LT3966Leaf::channel_count];
  float channel_current[LT3966Leaf::channel_count];

  // change flags
  bool global_changed;
  bool channel_changed[LT3966Leaf::channel_count];
  bool adc_changed;
  bool adc_channel_changed[LT3966Leaf::channel_count];
  bool vin_changed;

public:
  LT3966Leaf(String name, int address=95)
    : Leaf("lt3966", name, NO_PINS)
    , SMBusNode(name, address)
    , Pollable(10,1)
    , Debuggable (name) 
  {
    for (int c=0; c<LT3966Leaf::channel_count; c++) {
      channel_enable[c]=false;
      channel_use_pwm[c]=false;
      channel_pwm[c]=8191;
      channel_dim[c]=255;
      channel_current[c] = NAN;
      channel_voltage[c] = NAN;
    }
    sample_interval_ms = 10000;
    report_interval_sec = 60;
  }

  void setup(void) {
    Leaf::setup();
    LEAF_ENTER(L_INFO);

    if (!probe(address)) {
      LEAF_ALERT("   LT3966 NOT FOUND at 0x%02x", address);
      address=0;
      stop();
      LEAF_VOID_RETURN;
    }

    // LT3966 appears not to work if you read multiple bytes, you have to read single bytes
    // as [byte,crc] pairs and verify the CRC of each byte.
    //
    // Not sure if this is an SMBUS thing or a quirk of the LTE3966.
    //
    if (!readSingleBytes(253, 3, part_id))
    {
      LEAF_ALERT("   LT3966 part ID read failed");
      stop();
      LEAF_VOID_RETURN;
    }
      
    LEAF_NOTICE("LT3966 device id 0x%02x%02x%02x", (int)part_id[0], (int)part_id[1], (int)part_id[2]);
    if (part_id[0]!=0x03 || part_id[1]!=0x96  || part_id[2]!=0x60) {
      LEAF_ALERT("   LT3966 part ID mismatch");
      stop();
      LEAF_VOID_RETURN;
    }

    registerLeafBoolValue("wdt_flag", &WDTFLAG, "watchdog timer flag");
    registerLeafBoolValue("wdt_enable", &WDTFLAG, "watchdog timer enable");
    registerLeafBoolValue("mphase", &MPHASE);
    registerLeafBoolValue("clkout", &CLKOUT);
    for (int c=0; c<LT3966Leaf::channel_count; c++) {
      registerLeafBoolValue("channel_enable_"+c, channel_enable+c, "Enable the LED output for channel "+c);
      registerLeafBoolValue("channel_fault_"+c, channel_enable+c, "State of fault detection for channel "+c, ACL_GET_ONLY, VALUE_NO_SAVE);
      registerLeafBoolValue("channel_use_pwm_"+c, channel_use_pwm+c, "Enable PWM dimming for channel "+c);
      registerLeafIntValue("channel_pwm_cycle_length_"+c, channel_pwm_cycle_length+c, "PWM cycle length for channel "+c);
      registerLeafIntValue("channel_pwm_"+c, channel_pwm+c, "PWM dimming vlaue for channel "+c);
      registerLeafIntValue("channel_dim_"+c, channel_dim+c, "Analog dimming vlaue for channel "+c);

      registerLeafFloatValue("channel_voltage_"+c, channel_voltage+c, "Measured output voltage for channel "+c, ACL_GET_ONLY, VALUE_NO_SAVE);
      registerLeafFloatValue("channel_current_"+c, channel_current+c, "Measured output current for channel "+c, ACL_GET_ONLY, VALUE_NO_SAVE);
    }
    registerLeafBoolValue("adc_run", &adc_run, "Enable the ADC converter");
    registerLeafBoolValue("adc_auto", &adc_run, "Enable continuous ADC conversion");
    registerLeafIntValue("adc_clock_divider", &adc_clock_divider, "Clock divider for ADC converter");
    registerLeafIntValue("adc_target", &adc_target, "Enabled ADC Channel");

    registerLeafFloatValue("input_voltage", &input_voltage, "Regulator input voltage", ACL_GET_ONLY, VALUE_NO_SAVE);

    LEAF_LEAVE;
  }

  void start(void) 
  {
    Leaf::start();

    LEAF_ENTER(L_NOTICE);
    writeByte(REG_ADCCFG, ADCCFG);
    writeByte(REG_CH1, 0xF0); // error reporting on
    writeByte(REG_CH1+1, 0); // PWM dimming off
    writeByte(REG_CH2, 0xF0); // error reporting on
    writeByte(REG_CH2+1, 0); // PWM dimming off
    writeByte(REG_CH3, 0xF0); // error reporting on
    writeByte(REG_CH3+1, 0); // PWM dimming off
    writeByte(REG_CH4, 0xF0); // error reporting on
    writeByte(REG_CH4+1, 0); // PWM dimming off
    LEAF_LEAVE;
  }
  

  void loop() 
  {
    pollable_loop();
  }

  void status_pub() 
  {
    String p = getName()+"_";

    if (global_changed) {
      mqtt_publish(p+"wdt_flag", ABILITY(WDTFLAG));
      mqtt_publish(p+"wdt_enabled", ABILITY(WDTEN));
      mqtt_publish(p+"mphase", ABILITY(MPHASE));
      mqtt_publish(p+"clkout", ABILITY(CLKOUT));
    }
    for (int c=0; c<LT3966Leaf::channel_count; c++) {
      // bits 0-3 of GLBCFG are channel DISABLE, that is 0=enabled 1=off
      if (channel_changed[c]) {
	mqtt_publish(p+"channel_enable_"+c, ABILITY(channel_enable[c]));
	mqtt_publish(p+"channel_fault_"+c, TRUTH(channel_fault[c]));
	mqtt_publish(p+"channel_use_pwm_"+c, TRUTH(channel_use_pwm[c]));
	mqtt_publish(p+"channel_pwm_cycle_length_"+c, String(channel_pwm_cycle_length[c]));
	mqtt_publish(p+"channel_pwm_"+c, String(channel_pwm[c]));
	mqtt_publish(p+"channel_dim_"+c, String(channel_dim[c]));
      }
    }
    if (adc_changed) {
      mqtt_publish(p+"adc_run", ABILITY(adc_run));
      mqtt_publish(p+"adc_auto", ABILITY(adc_auto));
      mqtt_publish(p+"adc_clock_divider", String(adc_clock_divider));
      mqtt_publish(p+"adc_target", String(adc_target));
    }

    if (vin_changed) {
      mqtt_publish(p+"input_voltage", String(input_voltage,3));
    }

    for (int c=0; c<LT3966Leaf::channel_count; c++) {
      if (adc_channel_changed[c]) {
	mqtt_publish(p+"channel_voltage_"+c, String(channel_voltage[c],3));
	mqtt_publish(p+"channel_current_"+c, String(channel_current[c],3));
      }
    }
  }
  
  virtual bool poll() 
  {
    LEAF_ENTER(L_INFO);
    bool result = false;
    
    if (!readByte(REG_GLBCFG, &GLBCFG)) {
      LEAF_ALERT("LTE3966 GLBCFG read error");
      LEAF_BOOL_RETURN(false);
    }
    if (!readSingleBytes(REG_CH1, sizeof(CHAN1), CHAN1)) {
      LEAF_ALERT("LTE3966 CHAN1 read error");
      LEAF_BOOL_RETURN(false);
    }
    if (!readSingleBytes(REG_CH2, sizeof(CHAN2), CHAN2)) {
      LEAF_ALERT("LTE3966 CHAN2 read error");
      LEAF_BOOL_RETURN(false);
    }
    if (!readSingleBytes(REG_CH3, sizeof(CHAN3), CHAN3)) {
      LEAF_ALERT("LTE3966 CHAN3 read error");
      LEAF_BOOL_RETURN(false);
    }
    if (!readSingleBytes(REG_CH4, sizeof(CHAN4), CHAN4)) {
      LEAF_ALERT("LTE3966 CHAN4 read error");
      LEAF_BOOL_RETURN(false);
    }
    if (!readByte(REG_ADCCFG, &ADCCFG)) {
      LEAF_ALERT("LTE3966 ADCCFG read error");
      LEAF_BOOL_RETURN(false);
    }
    if (!readByte(REG_ADCVIN, &ADCVIN)) {
      LEAF_ALERT("LTE3966 ADCVIN read error");
      LEAF_BOOL_RETURN(false);
    }
    if (!readSingleBytes(REG_ADCCHN, sizeof(ADCCHN), ADCCHN)) {
      LEAF_ALERT("LTE3966 ADCCHN read error");
      LEAF_BOOL_RETURN(false);
    }

    // Now parse the ADC values from the above registers

    // global config
    global_changed = false;
    UPDATE_STATE_BOOL(WDTFLAG, GLBCFG&0x80, global_changed);
    UPDATE_STATE_BOOL(WDTEN, GLBCFG&0x40, global_changed);
    UPDATE_STATE_BOOL(MPHASE, GLBCFG&0x20, global_changed);
    UPDATE_STATE_BOOL(CLKOUT, GLBCFG&0x10, global_changed);
    if (global_changed) {
      result = true;
      LEAF_INFO("Global config GLBCFG=%02x WDTFLAG=%s WDTEN=%s MPHASE=%s CLKOUT=%s",
		(unsigned int)GLBCFG,
	      TRUTH_lc(WDTFLAG),TRUTH_lc(WDTEN),TRUTH_lc(MPHASE),TRUTH_lc(CLKOUT));
    }
    for (int c=0; c<LT3966Leaf::channel_count; c++) {
      // bits 0-3 of GLBCFG are channel DISABLE, that is 0=enabled 1=off
      channel_changed[c] = false;
      UPDATE_STATE_BOOL(channel_enable[c], !(GLBCFG & (1<<c)), channel_changed[c]);
      if (channel_changed[c]) {
	LEAF_INFO("channel %d enable changed", c+1);
      }
    }
    
    for (int c=0; c<LT3966Leaf::channel_count; c++) {
      byte *chan=NULL;
      switch (c) {
      case CHANNEL_ONE:
	chan=CHAN1;
	break;
      case CHANNEL_TWO:
	chan=CHAN2;
	break;
      case CHANNEL_THREE:
	chan=CHAN3;
	break;
      case CHANNEL_FOUR:
	chan=CHAN4;
	break;
      }
      if (!chan) break;
      LEAF_DEBUG("Channel %d RAW: %02x-%02x-%02x-%02x-%02x",
		c, (unsigned int )chan[0],
		(unsigned int )chan[1],	(unsigned int )chan[2],
		(unsigned int )chan[3],	(unsigned int )chan[4]);
      
      // channel 1 status
      UPDATE_STATE_BOOL(channel_fault[c]  ,chan[0] & 0x0F, channel_changed[c]);
      UPDATE_STATE_BOOL(channel_use_pwm[c],chan[1] & 0x01, channel_changed[c]);

      // pwm cycle is bits 5-7 of chan[2], cycle length is 2^(n+6)
      UPDATE_STATE_INT(channel_pwm_cycle_length[c], 1<<(chan[1]>>5), 0, channel_changed[c]);

      // pwm dimming value is bits 0-4 of chan[2] (lsb) plus bits 0-7 of chan3 (msb)
      UPDATE_STATE_INT(channel_pwm[c], chan[2] | ((chan[3]&0x1F)<<5), 0, channel_changed[c]);

      // analog dimming value is chan[4] [0-255]
      UPDATE_STATE_INT(channel_dim[c], chan[4], 0, channel_changed[c]);

      if (channel_changed[c]) {
	result = true;
	LEAF_INFO("Channel %d %s: fault=%s pwm=%s cycle=%d pwmdim=%d adim=%d",
		  c+1,
		  ABILITY_j(channel_enable[c]),
		  TRUTH_lcj(channel_fault[c]),
		  ABILITY_lcj(channel_use_pwm[c]),
		  channel_pwm_cycle_length[c],
		  channel_pwm[c],
		  channel_dim[c]);
      }
    }

    // ADC Configuration register
    adc_changed = false;
    UPDATE_STATE_BOOL(adc_run,  ADCCFG&0x80, adc_changed);
    UPDATE_STATE_BOOL(adc_auto, ADCCFG&0x40, adc_changed);
    UPDATE_STATE_INT(adc_clock_divider, 1 << ((ADCCFG >> 4) & 0x03), 0, adc_changed);
    UPDATE_STATE_INT(adc_target, ADCCFG & 0x0F, 0, adc_changed);
    if (adc_changed) {
      result = true;
      LEAF_INFO("ADC config: run=%s auto=%s divider=%d target=0x%02x",
		ABILITY_lcj(adc_run), ABILITY_lcj(adc_auto), adc_clock_divider, adc_target);
    }

    // ADC values (all 5mv per LSB)
    vin_changed = false;
    UPDATE_STATE_FLOAT(input_voltage, ADCVIN * 48 * 0.005, 0.01, vin_changed);   // VIN has a 48x scale cfactor
    if (vin_changed) {
      result = true;
      LEAF_INFO("Voltage in: raw=0x%02x %.003fV", (unsigned int)ADCVIN, input_voltage);
    }

    for (int c=0; c<LT3966Leaf::channel_count; c++) {
      adc_channel_changed[c] = false;

      UPDATE_STATE_FLOAT(channel_voltage[c], ADCCHN[2*c] * 0.005 * vs_factor, 0.01, adc_channel_changed[c]);
      UPDATE_STATE_FLOAT(channel_current[c], ADCCHN[2*c+1] * 0.00125 / cs_ohms, 0.01, adc_channel_changed[c]);

      if (adc_channel_changed[c]) {
	result = true;
	LEAF_INFO("Channel %2d: raw=%02x%02x %.003fV %.003fA", c+1,
		  (unsigned int)ADCCHN[2*c], (unsigned int)ADCCHN[2*c+1],
		  channel_voltage[c], channel_current[c]);
      }
    }
      
    LEAF_BOOL_RETURN(result);
  }

};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
