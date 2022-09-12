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
uint16_t raw_buf[SAMPLE_HISTORY_SIZE][ANALOG_AC_CHAN_MAX];
uint16_t raw_head[ANALOG_AC_CHAN_MAX];
uint16_t raw_count[ANALOG_AC_CHAN_MAX];
uint32_t raw_total[ANALOG_AC_CHAN_MAX];

int wave_pos = 0;
#define WAVE_SAMPLE_COUNT 640
int8_t wave_buf[WAVE_SAMPLE_COUNT];

  
uint16_t analog_ac_oversample = 0;
bool analog_ac_wavedump = false;

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

    raw_buf[c][raw_head[c]]=value;
    raw_head[c] = (raw_head[c]+1)%SAMPLE_HISTORY_SIZE;
    if (raw_count[c] < SAMPLE_HISTORY_SIZE) raw_count[c]++;
    raw_total[c]++;

    if (analog_ac_wavedump && (wave_pos < WAVE_SAMPLE_COUNT)) {
      wave_buf[wave_pos++]=(value-1700);
    }

    if ((analog_ac_oversample != 0) && (raw_count[c] >= analog_ac_oversample)) {
      int n;
      uint32_t s= 0;
      for (n=0; (n<analog_ac_oversample);n++) {
	s += raw_buf[c][(raw_head[c] + SAMPLE_HISTORY_SIZE - n)%SAMPLE_HISTORY_SIZE];
      }
      value = s/n;
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
  float bandgap = 10;

  int count[chan_max];
  int raw_min[chan_max];
  int raw_max[chan_max];

  float polynomial_coefficients[AnalogACLeaf::poly_max];
  
  unsigned long raw_n[chan_max];
  unsigned long raw_s[chan_max];
  unsigned long value_n[chan_max];
  unsigned long value_s[chan_max];
  unsigned long interval_start[chan_max];
  float last_value = NAN;

public:
  AnalogACLeaf(String name, pinmask_t pins, int in_min=0, int in_max=4095, float out_min=0, float out_max=100, bool asBackplane = false) : AnalogInputLeaf(name, pins, in_min, in_max, out_min, out_max, asBackplane) 
  {
    // preference default values, updated from prefs in setup()
    report_interval_sec = 30;
    sample_interval_ms = 5000;

    // set an initial identity (y=x) relationship between raw and cooked values
    polynomial_coefficients[0]=0;
    polynomial_coefficients[1]=1;
    for (int n=2;n<AnalogACLeaf::poly_max; n++) {
      polynomial_coefficients[n]=0;
    }
    intCount = 0;

    // set up adc channel(s)
    FOR_PINS({
	int c = channels++;
	adcPin[c]=pin;
	if (channels < ANALOG_AC_CHAN_MAX) adcPin[channels]=-1;
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

    String prefix=String("adc_")+get_name()+"_";
    
    getBoolPref(prefix+"wavedump", &analog_ac_wavedump);
    getIntPref(prefix+"sample_ms", &sample_interval_ms, "AC ADC sample interval (milliseconds)");
    getIntPref(prefix+"report_sec", &report_interval_sec, "AC ADC report interval (secondsf");
    analog_ac_oversample = getIntPref(prefix+"oversample", (int)analog_ac_oversample, "AC ADC oversampling (number of samples to average, 0=off)");
    getFloatPref(prefix+"bandgap", &bandgap, "Ignore AC ADC changes below this value");
    LEAF_NOTICE("Analog AC parameters sample_ms=%d report_sec=%d oversample=%d bandgap=%.3f",
		sample_interval_ms, report_interval_sec, analog_ac_oversample, bandgap);
    

    char msg[80];
    int len = 0;
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
    LEAF_NOTICE("Raw conversion formula: y = %s", msg);

    for (int c=0; c<channels; c++) {
      adcAttachPin(adcPin[c]);
      int value = analogRead(adcPin[c]);
      LEAF_NOTICE("Analog AC channel %d is pin GPIO%d (initial value %d)", c+1, adcPin[c], value);

      
      // clear the oversampling buffer for this channel
      memset(raw_buf[c], 0, SAMPLE_HISTORY_SIZE*sizeof(uint16_t));
      raw_head[c] = 0;
      raw_count[c] = 0;
      raw_total[c] = 0;
      
      // clear the state machine for this channel
      reset(c);
    }
    
    timer = timerBegin(0, 10, true);
    timerAttachInterrupt(timer, &onTimer, true);
    timerAlarmWrite(timer, 500, true);
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
    sampleCount[c] = 0;
    wave_pos=0;
    portEXIT_CRITICAL(&adc1Mux);

    count[c] = 0;
    raw_max[c] = raw_min[c] = -1;
    interval_start[c] = micros();
  }

  virtual void status_pub() 
  {
    LEAF_ENTER(L_DEBUG);
    char buf[160];
    
    for (int c=0; c<channels; c++) {
      if (value_n[c]==0) continue;

      float mean = value_s[c]/value_n[c];
      LEAF_INFO("Channel %d status: mean=%.3f from %d samples", c, mean, value_n[c]);
      value_n[c] = value_s[c] = 0;
      int delta = raw_n[c]?(raw_s[c]/raw_n[c]):(raw_max[c]-raw_min[c]);
      raw_n[c] = raw_s[c] = 0;

      if (!isnan(last_value) && (fabs(last_value-mean) < bandgap))  {
	// Ignore a change below the significance threshold
	continue;
      }
      last_value = mean;

      // Dump out one cycle of the raw samples (bogo-oscilloscope!!)
      if (analog_ac_wavedump) {
	for (int i=0;i<wave_pos;i++) {
	  Serial.printf("%d,%d\n",i,(int)wave_buf[i]);
	}
      }

      
      LEAF_NOTICE("ADC STATUS %s:%d avg range %d avg milliamps=%.1f", get_name().c_str(), c, delta, mean);
      //Serial.printf("%d\n", (int)mean);

      char fmt[8];
      char topic[64];
      char payload[64];
      snprintf(fmt, sizeof(fmt), "%%.%df", dp);
      snprintf(topic, sizeof(topic), "status/current/%d", c+1);
      snprintf(payload, sizeof(payload), fmt, mean);
      //LEAF_NOTICE("Formatted milliamps %f as [%s]", mean, payload);
      mqtt_publish(topic, payload);

      reset(c);
      
    }
    LEAVE;
  }

  virtual bool sample(int c)
  {
    LEAF_ENTER_INT(L_DEBUG, c);
    int newSamples;
    int min,max;
    static unsigned long last_sample_us = 0;
    unsigned long now_us = micros();
    
    portENTER_CRITICAL(&adc1Mux);
    newSamples = sampleCount[c] - count[c];
    min = minRead[c];
    max = maxRead[c];
    if (newSamples > 0) {
      if ((minRead[c]>0) && ((raw_min[c]<0) || (minRead[c]<raw_min[c])))  
	raw_min[c] = minRead[c];
      if ((maxRead[c]>0) && ((raw_max[c]<0) || (maxRead[c]>raw_max[c])))
	raw_max[c] = maxRead[c];
    }
    portEXIT_CRITICAL(&adc1Mux);

    if (last_sample_us > 0) {
      // we know a precise interval in microseconds between this and the previous sample
      unsigned long n = sampleCount[c];
      unsigned long duration_us = now_us - last_sample_us;
      int ctr = (max+min)/2;
      float samples_per_cycle = n * 20000 / duration_us;       // one 50hz cycle is 20000 uS 
      
      LEAF_INFO("ADC SAMPLE ch%d n=%lu r=[%d:%d] ctr=%d t=%lu s/c=%.1f",
		  c, n, min, max,
		  ctr, duration_us, samples_per_cycle);
    }
    last_sample_us = now_us;

    int delta = raw_max[c]-raw_min[c];
    float mA = cook_value(delta);
    if (mA < 0) mA=0;
    
    if (newSamples <= 0) {
      LEAF_NOTICE("ADC %s:%d no new samples for this channel", get_name().c_str(), c);
      LEAF_BOOL_RETURN(false);
    }

    count[c] += newSamples;

    raw[c] = delta;
    raw_s[c] += delta;
    raw_n[c]++;
    
    value[c] = mA;
    value_s[c] += value[c];
    value_n[c]++;
    
    LEAF_INFO("ADC %s:%d raw value range [%d:%d] (d=%d, c=%d n=%d) => %.3fmA",
		get_name().c_str(), c, raw_min[c], raw_max[c], delta, raw_min[c]+(int)(delta/2), newSamples, mA);
    reset(c);

    LEAF_BOOL_RETURN(false);
  }

  bool mqtt_receive(String type, String name, String topic, String payload)
  {
    bool handled = false;

    WHEN("set/oversample",{
	analog_ac_oversample = payload.toInt();
	LEAF_NOTICE("Set oversample to %d", analog_ac_oversample);
      })
    else {
      handled = Leaf::mqtt_receive(type, name, topic, payload);
    }
    
    return handled;
  }
  
};


// local Variables:
// mode: C++
// c-basic-offset: 2
// End:

