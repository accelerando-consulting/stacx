//
//@**************************** class AnalogACLeaf ******************************
// 
// This class encapsulates an analog input sensor that publishes measured
// current values to MQTT, using an ACS712 hall-effect sensor, or a biased
// current transformer and burden resistor (typically 100Ohm).
//
#pragma once 

#include "leaf_analog.h"

hw_timer_t * timer = NULL;

#define ANALOG_AC_CHAN_MAX 4
int adcPin[ANALOG_AC_CHAN_MAX];
volatile uint32_t intCount;
volatile uint32_t sampleCount[ANALOG_AC_CHAN_MAX];
volatile int minRead[ANALOG_AC_CHAN_MAX];
volatile int maxRead[ANALOG_AC_CHAN_MAX];

#define SAMPLE_HISTORY_SIZE 20
uint16_t raw_buf[SAMPLE_HISTORY_SIZE];
uint16_t raw_head=0;
uint16_t raw_count=0;
uint32_t raw_total=0;

  
uint16_t oversample = 0;

void IRAM_ATTR onTimer() 
{
  portENTER_CRITICAL_ISR(&adc1Mux);
  intCount++;
  for (int c=0; (c<ANALOG_AC_CHAN_MAX) && (adcPin[c]>=0); c++) {
    int value = analogRead(adcPin[c]);
    if (value < 0) {
      ALERT("WTF this should never be negative");
      return;
    }

    if (c==0) {
      raw_buf[raw_head]=value;
      raw_head = (raw_head+1)%SAMPLE_HISTORY_SIZE;
      if (raw_count < SAMPLE_HISTORY_SIZE) raw_count++;
      raw_total++;

      if ((oversample != 0) && (raw_count >= oversample)) {
	int n;
	uint32_t s= 0;
	for (n=0; (n<oversample);n++) {
	  s += raw_buf[(raw_head + SAMPLE_HISTORY_SIZE - n)%SAMPLE_HISTORY_SIZE];
	}
	value = s/n;
      }
    }
    sampleCount[c]++;
    if ((minRead[c] < 0) || (value < minRead[c])) minRead[c] = value;
    if ((maxRead[c] < 0) || (value > maxRead[c])) maxRead[c] = value;
  }
  portEXIT_CRITICAL_ISR(&adc1Mux);
}

class AnalogACLeaf : public AnalogInputLeaf
{
public:
  static const int chan_max = ANALOG_AC_CHAN_MAX;
  static const int poly_max = 4;
protected:
  int dp = 3;
  int channels = 0;

  int count[chan_max];
  int raw_min[chan_max];
  int raw_max[chan_max];

  float polynomial_coefficients[AnalogACLeaf::poly_max];
  
  unsigned long raw_n[chan_max];
  unsigned long raw_s[chan_max];
  unsigned long value_n[chan_max];
  unsigned long value_s[chan_max];
  unsigned long interval_start[chan_max];

public:
  AnalogACLeaf(String name, pinmask_t pins, int in_min=0, int in_max=4095, float out_min=0, float out_max=100, bool asBackplane = false) : AnalogInputLeaf(name, pins, in_min, in_max, out_min, out_max, asBackplane) 
  {
    // preference default values, updated from prefs in setup()
    report_interval_sec = 5;
    sample_interval_ms = 500;

    // set an initial identity (y=x) relationship between raw and cooked values
    polynomial_coefficients[0]=0;
    polynomial_coefficients[1]=1;
    for (int n=2;n<AnalogACLeaf::poly_max; n++) {
      polynomial_coefficients[n]=0;
    }

    // set up adc channel(s)
    FOR_PINS({
	int c = channels++;
	adcPin[c]=pin;
	if (channels < ANALOG_AC_CHAN_MAX) adcPin[channels]=-1;
	count[c]=0;
	raw_min[c]=-1;
	raw_max[c]=-1;
	interval_start[c]=micros();
      });
  }

  float cook_value(int raw) 
  {
    // simple linear fit
    //return (raw * milliamps_per_lsb) + milliamps_per_lsb_zero_correct;

    // quadratic fit derived from excel
    float x = raw;
    float y = 0;
    for (int n=0; n<AnalogACLeaf::poly_max;n++) {
      if (polynomial_coefficients[n]==0) continue;
      y += polynomial_coefficients[n]*pow(x,n);
    }
    return y;
  }
  
  void setup() 
  {
    AnalogInputLeaf::setup();

    for (int n=0; n<AnalogACLeaf::poly_max; n++) {
      char pref_name[16];
      snprintf(pref_name, sizeof(pref_name), "%s_c%d", this->leaf_name.c_str(), n);
      polynomial_coefficients[n] = getPref(pref_name, (n==1)?"1":"0", "Polynomial coefficients for ADC mapping").toFloat();
    }
    sample_interval_ms = getIntPref("adcac_sample_ms", sample_interval_ms, "AC ADC sample interval (milliseconds)");
    report_interval_sec = getIntPref("adcac_report_sec", report_interval_sec, "AC ADC report interval (secondsf");

    char msg[80];
    int len = 0;
    len += snprintf(msg, sizeof(msg)-len, "Raw conversion formula: y=");
    bool first = true;
    
    for (int n=AnalogACLeaf::poly_max-1;n>=0;n--) {
      if (polynomial_coefficients[n] == 0) continue;
      if (first) {
	first=false;
      }
      else {
	len += snprintf(msg+len, sizeof(msg)-len, " + ");
      }
      switch (n) {
      case 0:
	len += snprintf(msg+len, sizeof(msg)-len, "%f", polynomial_coefficients[n]);
	break;
      case 1:
	len += snprintf(msg+len, sizeof(msg)-len, "%fx", polynomial_coefficients[n]);
	break;
      default:
	len += snprintf(msg+len, sizeof(msg)-len, "%fx^%d", polynomial_coefficients[n], n);
	break;
      }
    }
    LEAF_NOTICE("%s", msg);

    for (int c=0; c<channels; c++) {
      adcAttachPin(adcPin[c]);
      int value = analogRead(adcPin[c]);
      LEAF_NOTICE("Analog AC channel %d is %d (initial value %d)", c+1, adcPin[c], value);
    }
    reset_all();
    
    timer = timerBegin(0, 10, true);
    timerAttachInterrupt(timer, &onTimer, true);
    timerAlarmWrite(timer, 1000, true);
    timerAlarmEnable(timer);

  }

/*
  virtual void loop() 
  {
    static unsigned long last_print=0;
    unsigned long now = millis();
    
    AnalogInputLeaf::loop();

    if (now > (last_print + 30000)) {
      unsigned long uptime = millis();
      LEAF_NOTICE("Processed %u timer events over %ums (avgd %d sps)", intCount, uptime, intCount*1000/uptime);
      last_print = now;
    }
  }
*/
  
  void reset(int c) 
  {
    portENTER_CRITICAL(&adc1Mux);
    minRead[c] = -1;
    maxRead[c] = -1;
    portEXIT_CRITICAL(&adc1Mux);

    count[c] = 0;
    raw_max[c] = raw_min[c] = -1;
    interval_start[c] = 0;
  }

  void reset_all() 
  {
    for (int c=0; c<channels; c++) {
      reset(c);
    }
  }
  
  virtual void status_pub() 
  {
    LEAF_ENTER(L_DEBUG);
    char buf[160];
    
    for (int c=0; c<channels; c++) {
      if (value_n[c]==0) continue;

      float mean = value_s[c]/value_n[c];
      LEAF_INFO("mean=%.3f from %d samples", mean, value_n[c]);
      value_n[c] = value_s[c] = 0;
      int delta = raw_n[c]?(raw_s[c]/raw_n[c]):(raw_max[c]-raw_min[c]);
      raw_n[c] = raw_s[c] = 0;

      LEAF_INFO("ADC avg range %d avg milliamps=%.1f", delta, mean);
      //Serial.printf("%d\n", (int)mean);

      char fmt[8];
      char topic[64];
      char payload[64];
      snprintf(fmt, sizeof(fmt), "%%.%df", dp);
      snprintf(topic, sizeof(topic), "status/milliamps%d", c+1);
      snprintf(payload, sizeof(payload), fmt, mean);
      //LEAF_NOTICE("Formated milliamps %f as [%s]", mean, payload);
      mqtt_publish(topic, payload);

      //reset(c);
      
    }
    LEAVE;
  }

  virtual bool sample(int c)
  {
    LEAF_ENTER(L_DEBUG);
    int newSamples;
    
    portENTER_CRITICAL(&adc1Mux);
    newSamples = sampleCount[c] - count[c];
    if (newSamples > 0) {
      if ((minRead[c]>0) && (raw_min[c]<0) || (minRead[c]<raw_min[c])) 
	raw_min[c] = minRead[c];
      if ((maxRead[c]>0) && (raw_max[c]<0) || (maxRead[c]>raw_max[c]))
	raw_max[c] = maxRead[c];
    }
    portEXIT_CRITICAL(&adc1Mux);

    int delta = raw_max[c]-raw_min[c];
    float mA = cook_value(delta);
    LEAF_INFO("raw value range [%d:%d] (%d) => %.3fmA", raw_min[c], raw_max[c], delta, mA);
    if (mA < 0) mA=0;
    //LEAF_DEBUG("Raw buffer has %d samples (%lu)", (int)raw_count, raw_total);
    raw_total=0;
    
    if (newSamples > 0) {
      count[c] += newSamples;

      raw[c] = delta;
      raw_s[c] += delta;
      raw_n[c]++;
      
      value[c] = mA;
      value_s[c] += value[c];
      value_n[c]++;

      LEAF_INFO("raw value range [%d:%d] (%d, n=%d) => %.3fmA", raw_min[c], raw_max[c], delta, value_n[c], mA);
      LEAF_INFO("Total sample count %d", (int)raw_count);
      LEAF_DEBUG("Sample history [%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d]",
		  (int)raw_buf[0], (int)raw_buf[1], (int)raw_buf[2], (int)raw_buf[3],
		  (int)raw_buf[4], (int)raw_buf[5], (int)raw_buf[6], (int)raw_buf[7],
		  (int)raw_buf[8], (int)raw_buf[9], (int)raw_buf[10], (int)raw_buf[11],
		  (int)raw_buf[12], (int)raw_buf[13], (int)raw_buf[14], (int)raw_buf[15],
		  (int)raw_buf[16], (int)raw_buf[17], (int)raw_buf[18], (int)raw_buf[19]);
      reset(c);
    }
    LEAF_RETURN(false);
  }
  
};


// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
