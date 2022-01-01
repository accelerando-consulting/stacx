//
//@**************************** class AnalogACLeaf ******************************
// 
// This class encapsulates an analog input sensor that publishes measured
// voltage values to MQTT
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

  
uint16_t oversample = 3;

void IRAM_ATTR onTimer() 
{
  portENTER_CRITICAL_ISR(&adc1Mux);
  intCount++;
  for (int c=0; (c<ANALOG_AC_CHAN_MAX) && (adcPin[c]>=0); c++) {
    int value = analogRead(adcPin[c]);

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
protected:
  int dp = 3;
  int channels = 0;

  int count[chan_max];
  int raw_min[chan_max];
  int raw_max[chan_max];

  float milliamps_per_lsb;               // pref=adcac_m, this is the m in y=mx+c where x=adc y=mA
  float milliamps_per_lsb_zero_correct;  // pref=adcac_c, this is the c in y=mx+c where x=adc y=mA
  
  unsigned long value_n[chan_max];
  unsigned long value_s[chan_max];
  unsigned long interval_start[chan_max];

public:
  AnalogACLeaf(String name, pinmask_t pins, int in_min=0, int in_max=4095, float out_min=0, float out_max=100, bool asBackplane = false) : AnalogInputLeaf(name, pins, in_min, in_max, out_min, out_max, asBackplane) 
  {
    // preference default values, updated from prefs in setup()
    report_interval_sec = 10;
    sample_interval_ms = 1000;
    milliamps_per_lsb=0;              
    milliamps_per_lsb_zero_correct=0;

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
    return (raw * milliamps_per_lsb) + milliamps_per_lsb_zero_correct;
  }
  
  void setup() 
  {
    AnalogInputLeaf::setup();
    milliamps_per_lsb = getPref("adcac_m", "3.07").toFloat();
    milliamps_per_lsb_zero_correct = getPref("adcac_c","-70").toFloat();
    sample_interval_ms = getPref("adcac_sample_ms", "1000").toInt();
    report_interval_sec = getPref("adcac_report_sec", "10").toInt();
    
    NOTICE("Using adc value calculation mA = adc * %f + %f", milliamps_per_lsb, milliamps_per_lsb_zero_correct);

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

      unsigned long interval_usec = micros()-interval_start[c];
      //snprintf(buf, sizeof(buf), "{\"count\": %d, \"interval_usec\": %lu, \"raw_min\": %d, \"raw_max\":%d, \"min\": %f, \"max\":%f }",
      //count[c], interval_usec, raw_min[c], raw_max[c], min[c], max[c]);
      snprintf(buf, sizeof(buf), "{\"count\": %d, \"interval_usec\": %lu }",
	       count[c], interval_usec, raw_min[c],delta,raw_max[c]);
      mqtt_publish("status/samples"+String(c+1,10), buf);
      snprintf(buf, sizeof(buf), "{\"raw\": %d:%d:%d }",
	       raw_min[c],delta,raw_max[c]);
      mqtt_publish("status/raw"+String(c+1,10), buf);

      float mean = value_s[c]/value_n[c];
      value_n[c] = value_s[c] = 0;

      char fmt[8];
      char topic[64];
      char payload[64];
      snprintf(fmt, sizeof(fmt), "%%.%df", dp);
      snprintf(topic, sizeof(topic), "status/milliamps%d", c+1);
      snprintf(payload, sizeof(payload), fmt, mean);
      mqtt_publish(topic, payload);
      //reset(c);
      
    }
    LEAVE;
  }

  virtual bool sample(int c) 
  {
    LEAF_ENTER(L_NOTICE);
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
    LEAF_DEBUG("raw value range [%d:%d] (%d) => %.3fmA", raw_min[c], raw_max[c], delta, mA);
    LEAF_DEBUG("Raw buffer has %d samples (%lu)", (int)raw_count, raw_total);
    raw_total=0;
    
    if (newSamples > 0) {
      count[c] += newSamples;

      raw[c] = delta;
      value[c] = mA;
      value_s[c] += value[c];
      value_n[c]++;
    }
    reset(c);
    LEAF_RETURN(false);
  }
  
};


// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
