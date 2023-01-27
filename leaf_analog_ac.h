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
volatile uint64_t intCount;
volatile uint32_t sampleCount[ANALOG_AC_CHAN_MAX];
volatile int minRead[ANALOG_AC_CHAN_MAX];
volatile int maxRead[ANALOG_AC_CHAN_MAX];

#define SAMPLE_HISTORY_SIZE 20
uint16_t raw_buf[SAMPLE_HISTORY_SIZE][ANALOG_AC_CHAN_MAX];
uint16_t raw_head[ANALOG_AC_CHAN_MAX];
uint16_t raw_count[ANALOG_AC_CHAN_MAX];
uint32_t raw_total[ANALOG_AC_CHAN_MAX];

#ifdef ANALOG_AC_WAVE_DUMP
int wave_pos = 0;
#define WAVE_SAMPLE_COUNT 640
int8_t wave_buf[WAVE_SAMPLE_COUNT];
#endif

uint16_t analog_ac_oversample = 0;
bool analog_ac_wavedump = false;
unsigned long analog_ac_wtf = 0;

void IRAM_ATTR onTimer() 
{
  if (!analogHoldMutex(HERE)) return;
  
  intCount++;
  for (int c=0; (c<ANALOG_AC_CHAN_MAX) && (adcPin[c]>=0); c++) {
    int value = analogRead(adcPin[c]);
    if (value < 0) {
      //ALERT("WTF this should never be negative");
      ++analog_ac_wtf = 0;
      continue;
    }

    raw_buf[c][raw_head[c]]=value;
    raw_head[c] = (raw_head[c]+1)%SAMPLE_HISTORY_SIZE;
    if (raw_count[c] < SAMPLE_HISTORY_SIZE) raw_count[c]++;
    raw_total[c]++;

#ifdef ANALOG_AC_WAVE_DUMP
    if (analog_ac_wavedump && (wave_pos < WAVE_SAMPLE_COUNT)) {
      wave_buf[wave_pos++]=(value-1700);
    }
#endif
    
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
  analogReleaseMutex(HERE);
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
  bool do_sample=true;
  bool do_timer=true;
  int timer_divider = 10;
  int timer_alarm_interval = 1000;
  

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

  unsigned long poll_count=0;
  unsigned long status_count=0;
  

public:
  AnalogACLeaf(String name, pinmask_t pins, int in_min=0, int in_max=4095, float out_min=0, float out_max=100, bool asBackplane = false)
    : AnalogInputLeaf(name, pins, in_min, in_max, out_min, out_max, asBackplane)
    , Debuggable(name)
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

  void timer_start() 
  {
    LEAF_ENTER(L_NOTICE);
    
    if (!timer) {
      LEAF_NOTICE("creating timer for %s with divider %d", describe().c_str(), timer_divider);
      timer = timerBegin(0, timer_divider, true);
    }
    timerAttachInterrupt(timer, &onTimer, true);

    timerAlarmWrite(timer, timer_alarm_interval, true);
    timerAlarmEnable(timer);

    LEAF_LEAVE;
  }

  void timer_stop() 
  {
    LEAF_ENTER(L_NOTICE);
    
    if (timer) {
      LEAF_NOTICE("destroying timer for %s", describe().c_str());
      timerEnd(timer);
      timer=NULL;
    }

    LEAF_LEAVE;
    
  }
  
  
  void setup() 
  {
    LEAF_ENTER(L_INFO);
    //int was = debug_level;
    //debug_level=4;
    //debug_flush=1;

    AnalogInputLeaf::setup();


    for (int n=0; n<AnalogACLeaf::poly_max; n++) {
      char pref_name[16];
      polynomial_coefficients[n] = (n==1)?1:0;  // default is identity function:  y = 1x + 0
      snprintf(pref_name, sizeof(pref_name), "map_coeff_%d", this->leaf_name.c_str(), n);
      registerLeafFloatValue(pref_name, polynomial_coefficients+n, "Polynomial coefficients for ADC mapping");
    }

    registerLeafBoolValue( "wavedump", &analog_ac_wavedump);
    registerLeafIntValue(  "sample_ms", &sample_interval_ms, "AC ADC sample interval (milliseconds)");
    registerLeafIntValue(  "timer_divider", &timer_divider, "AC ADC hardware timer divider");
    registerLeafIntValue(  "timer_alarm_interval", &timer_alarm_interval, "AC ADC hardware timer alarm interval");
    registerLeafIntValue(  "report_sec", &report_interval_sec, "AC ADC report interval (secondsf");
    registerLeafIntValue(  "oversample", &analog_ac_oversample, "AC ADC oversampling (number of samples to average, 0=off)");
    registerLeafFloatValue("bandgap", &bandgap, "Ignore AC ADC changes below this value");
    registerLeafBoolPref(  "do_sample", &do_sample, "Enable/Suppress AC ADC sampling");
    registerLeafBoolPref(  "do_timer", &do_timer, "Enable/Suppress AC ADC timer");

    LEAF_NOTICE("Analog AC parameters sample_ms=%d report_sec=%d oversample=%d bandgap=%.3f",
		sample_interval_ms, report_interval_sec, analog_ac_oversample, bandgap);
    LEAF_WARN("Analog AC config do_timer=%s do_sample=%s do_status=%s",
		ABILITY(do_timer), ABILITY(do_sample), ABILITY(do_status));
    

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
      if (!analogHoldMutex(HERE)) {
	LEAF_ALERT("Mutex not available");
	continue;
      }
      adcAttachPin(adcPin[c]);
      int value = analogRead(adcPin[c]);
      analogReleaseMutex(HERE);
      LEAF_NOTICE("Analog AC channel %d is pin GPIO%d (initial value %d)", c+1, adcPin[c], value);

      
      // clear the oversampling buffer for this channel
      LEAF_DEBUG("Clearing sample buffer");
      memset(raw_buf[c], 0, SAMPLE_HISTORY_SIZE*sizeof(uint16_t));
      raw_head[c] = 0;
      raw_count[c] = 0;
      raw_total[c] = 0;
      
      // clear the state machine for this channel
      LEAF_DEBUG("Reset state machine");
      reset(c);
    }
    
    if (do_timer) {
      timer_start();
    }
    
    //debug_level = was;
    LEAF_LEAVE;
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
    LEAF_ENTER_INT(L_DEBUG, c);
    
    if (!analogHoldMutex(HERE, 100))
      return;
    
    minRead[c] = -1;
    maxRead[c] = -1;
    sampleCount[c] = 0;
#ifdef ANALOG_AC_WAVE_DUMP
    wave_pos=0;
#endif
    analogReleaseMutex(HERE);

    count[c] = 0;
    raw_max[c] = raw_min[c] = -1;
    interval_start[c] = micros();
    LEAF_VOID_RETURN;
  }

  virtual void status_pub() 
  {
    //int was = debug_level;
    //debug_level=4;
    
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
#ifdef ANALOG_AC_WAVE_DUMP
      if (analog_ac_wavedump) {
	for (int i=0;i<wave_pos;i++) {
	  DBGPRINTF("%d,%d\n",i,(int)wave_buf[i]);
	}
      }
#endif
      
      LEAF_INFO("ADC STATUS %s:%d avg range %d avg milliamps=%.1f", getNameStr(), c, delta, mean);
      //Serial.printf("%d\n", (int)mean);
      ++status_count;

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
    LEAF_DEBUG("analog_ac status_pub finished");
    
    //debug_level=was;
    LEAF_LEAVE;
  }

  virtual bool sample(int c)
  {
    //int was = debug_level;
    //debug_level=4;
    
    LEAF_ENTER_INT(L_DEBUG, c);
    if (!do_sample) {
      LEAF_BOOL_RETURN(false);
    }
    
    int newSamples;
    int min,max;
    static unsigned long last_sample_us = 0;
    unsigned long now_us = micros();
    
    if (!analogHoldMutex(HERE)) {
      LEAF_NOTICE("did not get mutex for sample");
      LEAF_BOOL_RETURN(false);
    }
    ++poll_count;
    newSamples = sampleCount[c] - count[c];
    min = minRead[c];
    max = maxRead[c];
    if (newSamples > 0) {
      if ((minRead[c]>0) && ((raw_min[c]<0) || (minRead[c]<raw_min[c])))  
	raw_min[c] = minRead[c];
      if ((maxRead[c]>0) && ((raw_max[c]<0) || (maxRead[c]>raw_max[c])))
	raw_max[c] = maxRead[c];
    }
    analogReleaseMutex(HERE);

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
    if (mA < 0) {
      LEAF_WARN("Cooked value %f is negative, clipping to zero", mA);
      mA=0;
    }
    
    if (newSamples <= 0) {
      LEAF_NOTICE("ADC %s:%d no new samples for this channel", getNameStr(), c);
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
		getNameStr(), c, raw_min[c], raw_max[c], delta, raw_min[c]+(int)(delta/2), newSamples, mA);
    reset(c);

    //debug_level=was;
    
    LEAF_BOOL_RETURN(false);
  }

  void mqtt_do_subscribe() 
  {
    AnalogInputLeaf::mqtt_do_subscribe();
    registerCommand(HERE,"poll", "poll the current sensor");
    registerCommand(HERE,"timer_start", "start the AC current sensor low level timer");
    registerCommand(HERE,"timer_stop", "stop the AC current sensor low level timer");
    registerCommand(HERE,"stats", "report statistics from the AC current sensor");
    registerCommand(HERE,"config", "report configuration of the AC current sensor");
  }

  virtual void stats_pub() 
  {
    publish("stats/wtf", String(analog_ac_wtf), L_NOTICE, HERE);
    publish("stats/sample_count", String(sampleCount[0]), L_NOTICE, HERE);
    publish("stats/pin", String(adcPin[0]), L_NOTICE, HERE);
    publish("stats/int_count", String(intCount), L_NOTICE, HERE);
    publish("stats/min_read", String(minRead[0]), L_NOTICE, HERE);
    publish("stats/max_read", String(maxRead[0]), L_NOTICE, HERE);
#if ANALOG_USE_MUTEX
    publish("stats/skip_count", String(analog_mutex_skip_count), L_NOTICE, HERE);
#endif
    publish("stats/poll_count", String(poll_count), L_NOTICE, HERE);
    publish("stats/status_count", String(status_count), L_NOTICE, HERE);
  }

  virtual void config_pub() 
  {
    publish("config/do_timer", String(ABILITY(do_timer)), L_NOTICE, HERE);
    publish("config/do_sample",String(ABILITY(do_sample)), L_NOTICE, HERE);
    publish("config/do_status",String(ABILITY(do_status)), L_NOTICE, HERE);
  }
  
  virtual bool mqtt_receive(String type, String name, String topic, String payload, bool direct=false)
  {
    LEAF_ENTER_STRPAIR(L_INFO,topic,payload);
    bool handled = false;

    WHEN("set/oversample",{
	analog_ac_oversample = payload.toInt();
	LEAF_NOTICE("Set oversample to %d", analog_ac_oversample);
      })
    ELSEWHEN("cmd/poll",{
	sample(0);
      })
    ELSEWHEN("cmd/timer_start",{
	timer_start();
      })
    ELSEWHEN("cmd/timer_stop",{
	timer_stop();
      })
    else {
      handled = Leaf::mqtt_receive(type, name, topic, payload, direct);
    }
    
    LEAF_BOOL_RETURN(handled);
  }
};
// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
