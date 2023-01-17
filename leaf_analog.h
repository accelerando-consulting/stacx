//
//@************************ class AnalogInputLeaf ******************************
//
// This class encapsulates an analog input sensor publishes measured
// voltage values to MQTT
//
//  Attenuation       Measurable input voltage range
//  ADC_ATTEN_DB_0    100 mV ~ 950 mV
//  ADC_ATTEN_DB_2_5  100 mV ~ 1250 mV
//  ADC_ATTEN_DB_6    150 mV ~ 1750 mV
//  ADC_ATTEN_DB_11   150 mV ~ 2450 mV
//
#pragma once

#ifdef ESP8266
#undef ANALOG_USE_MUTEX
#define ANALOG_USE_MUTEX 0
#else
#ifndef ANALOG_USE_MUTEX
#define ANALOG_USE_MUTEX 1
#endif
//#include <hal/adc_hal.h>
#endif


#define ANALOG_INPUT_CHAN_MAX 4

#if ANALOG_USE_MUTEX
SemaphoreHandle_t analog_mutex = NULL;
unsigned long analog_mutex_skip_count = 0;

bool analogHoldMutex(codepoint_t where=undisclosed_location, TickType_t timeout=0) 
{
  if (analog_mutex==NULL) {
    SemaphoreHandle_t new_mutex = xSemaphoreCreateMutex();
    if (!new_mutex) {
      ALERT_AT(CODEPOINT(where), "Analog semaphore create failed");
      return(false);
    }
    NOTICE_AT(CODEPOINT(where), "Created new mutex for analog unit access");
    analog_mutex = new_mutex;
  }
  if (xSemaphoreTake(analog_mutex, timeout) != pdTRUE) {
    ++analog_mutex_skip_count;
    return(false);
  }
  return true;
}

void analogReleaseMutex(codepoint_t where) 
{
  if (xSemaphoreGive(analog_mutex) != pdTRUE) {
    ALERT_AT(CODEPOINT(where), "Modem port mutex release failed");
  }
}
#else
#define analogHoldMutex(...) (true)
#define analogReleaseMutex(...) {}
#endif


class AnalogInputLeaf : public Leaf
{
protected:
  int resolution;
  int attenuation;
  int raw[ANALOG_INPUT_CHAN_MAX];
  int raw_n[ANALOG_INPUT_CHAN_MAX];
  int raw_s[ANALOG_INPUT_CHAN_MAX];
  int value[ANALOG_INPUT_CHAN_MAX];
  int inputPin[ANALOG_INPUT_CHAN_MAX];
  int channels = 0;
  unsigned long last_sample[ANALOG_INPUT_CHAN_MAX];
  unsigned long last_report;
  int sample_interval_ms;
  int report_interval_sec;
  int dp;
  String unit;
  int delta,epsilon;
  int fromLow, fromHigh;
  float toLow, toHigh;

public:
  void setDecimalPlaces(int p) { dp = p; }
  void setResolution(int r) { resolution=r;
#ifdef ESP32
    analogReadResolution(r);
#endif
  }
  void setAttenuation(int a) {
    attenuation=a;
#ifdef ESP32
    analogSetAttenuation((adc_attenuation_t)a);
#endif
  }
  
  AnalogInputLeaf(String name, pinmask_t pins, int resolution = 12, int attenuation = 3, int in_min=0, int in_max=4096, float out_min=0, float out_max=4096, bool asBackplane = false)
    : Leaf("analog", name, pins)
    , Debuggable(name)
  {
    report_interval_sec = 600;
    sample_interval_ms = 200;
    epsilon = 50; // raw change threshold
    delta = 10; // percent change threshold
    this->resolution = resolution;
    this->attenuation = attenuation; // ADC_ATTEN_DB_11 => 11db, 3.55x, 150-2450mV
    last_report = 0;
    dp = 2;
    unit = "";
    fromLow = in_min;
    fromHigh = in_max;
    toLow = out_min;
    toHigh = out_max;
    impersonate_backplane = asBackplane;
  
    FOR_PINS({
	int c = channels++;
	inputPin[c]=pin;
	raw[c]=-1;
	raw_n[c]=0;
	raw_s[c]=0;
	value[c]=0;
	last_sample[c]= 0;
      });
    
  };

  virtual void setup(void) 
  {
    Leaf::setup();

#ifdef ESP32    
    LEAF_NOTICE("Set ADC resolution=%d attenuation=%d", resolution, attenuation);
    analogReadResolution(resolution);
    analogSetAttenuation((adc_attenuation_t)attenuation);
#endif
    LEAF_INFO("Analog input leaf has %d channels", channels);
    for (int c=0; c<channels;c++) {
      LEAF_NOTICE("%s channel %d claims pin %d", describe().c_str(), c+1, inputPin[c]);
#ifdef ESP32
      adcAttachPin(inputPin[c]);
      //analogSetPinAttenuation(inputPin[c], (adc_attenuation_t)3/*ADC_ATTEN_DB_11*/); // 11db, 3.55x, 150-2450mV
#endif
    }
    LEAF_NOTICE("Analog input mapping [%d:%d] => [%.3f,%.3f]", fromLow, fromHigh, toLow, toHigh);
  }

  virtual float convert(int v)
  {
    if (fromLow < 0) {
      return v;
    }
    // This is the floating point version of Arduino's map() function
    float mv = (v - fromLow) * (toHigh - toLow) / (fromHigh - fromLow) + toLow;
    LEAF_INFO("convert %d => [%d:%d, %.3f:%.3f] => %.3f", v, fromLow, fromHigh, toLow, toHigh, mv);
    return mv;
  };

  virtual void status_pub()
  {
    String raw_values="";
    String values="";
    for (int c=0; c<channels; c++) {
      if (c>0) {
	raw_values+=",";
	values+=",";
      }

      int raw_mean = raw[c];
      if (raw_n[c] > 0) {
	raw_mean = raw_s[c]/raw_n[c];
	if (raw_mean != raw[c]) {
	  LEAF_INFO("Channel %d mean value from %d samples changed %d => %d", c, raw_n[c], raw[c], raw_mean);
	  raw[c] = raw_mean;
	}
	raw_s[c]=raw_n[c]=0;
      }
      
      raw_values += String(raw_mean, 10);
      float mv = convert(raw_mean);
      value[c] = mv;
      values+= String(mv, dp);
    }
    publish("status/raw", raw_values);
    mqtt_publish("status/value", values);
  };

  virtual bool sample(int c)
  {
    // time to take a new sample
#ifdef ESP8266
    int new_raw = analogRead(A0);
#else
    if (!analogHoldMutex(HERE)) return false;
    int new_raw = analogRead(inputPin[c]);
    int new_raw_mv = analogReadMilliVolts(inputPin[c]);
    analogReleaseMutex(HERE);
#endif
    int raw_change = (raw[c] - new_raw);
    float delta_pc = (raw[c]?(100*(raw[c]-new_raw)/raw[c]):0);
    bool changed =
      (last_sample[c] == 0) ||
      (raw[c] < 0) ||
      ((raw[c] > 0) && (abs(raw_change) > epsilon) && (abs(delta_pc) > delta));
    LEAF_DEBUG("Sampling Analog input %d on pin %d => %d", c+1, inputPin[c], new_raw);
    raw_n[c]++;
    raw_s[c]+=new_raw;

    if (changed) {
      raw[c] = new_raw;
      if (last_sample[c] != 0) {
	// first sample is always a change, and percent is bogus
#ifdef ESP8266
	LEAF_NOTICE("Analog input #%d on pin %d => %d (change=%.1f%% n=%d mean=%d)", c+1, inputPin[c], raw[c], delta_pc, raw_n[c], raw_s[c]/raw_n[c]);
#else
	LEAF_NOTICE("Analog input #%d on pin %d => %d (%dmV) (change=%.1f%% n=%d mean=%d)", c+1, inputPin[c], raw[c], new_raw_mv, delta_pc, raw_n[c], raw_s[c]/raw_n[c]);
#endif
      }
    }
    
    return changed;
  }

  virtual bool sample(void) 
  {
    return sample(0);
  }
  
  virtual void loop(void) {
    Leaf::loop();
    LEAF_ENTER(L_DEBUG);
    bool changed = false;
    unsigned long now = millis();

    for (int c=0; c<channels; c++) {
      if ((pubsubLeaf && pubsubLeaf->isConnected() && (last_sample[c] == 0)) ||
	  (now >= (last_sample[c] + sample_interval_ms))
	) {
	LEAF_DEBUG("taking a sample for channel %d", c);
	changed |= this->sample(c);
	last_sample[c] = now;
      }
    }

    //
    // Reasons to report are:
    //   * significant change in value
    //   * report timer has elapsed
    //   * this is the first poll after connecting to MQTT
    //
    if ( changed ||
	 (now >= (last_report + (report_interval_sec * 1000))) ||
	 (pubsubLeaf && pubsubLeaf->isConnected() && (last_report == 0))
      ) {
      // Publish a report every N seconds, or if changed by more than d%
      LEAF_DEBUG("Time to report status (changed=%d, last_report=%lu, connected=%d)", (int)changed, last_report, (int)pubsubLeaf->isConnected());
      if (do_status) {
	status_pub();
      }
      last_report = now;
    }

    LEAF_LEAVE;
  };

};


// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
