//
//@************************ class AnalogInputLeaf ******************************
//
// This class encapsulates an analog input sensor publishes measured
// voltage values to MQTT
//
#pragma once

//#include <hal/adc_hal.h>

#define ANALOG_INPUT_CHAN_MAX 4

#ifdef ESP32
portMUX_TYPE adc1Mux = portMUX_INITIALIZER_UNLOCKED;
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
  void setResolution(int r) { resolution=r; analogReadResolution(r); }
  void setDecimalPlaces(int p) { dp = p; }
  void setAttenuation(int a) { attenuation=a; analogSetAttenuation((adc_attenuation_t)a);}
     
  AnalogInputLeaf(String name, pinmask_t pins, int in_min=0, int in_max=4096, float out_min=0, float out_max=4096, bool asBackplane = false) : Leaf("analog", name, pins)
  {
    report_interval_sec = 600;
    sample_interval_ms = 200;
    epsilon = 50; // raw change threshold
    delta = 10; // percent change threshold
    resolution = 12;
    attenuation = 3; // ADC_ATTEN_DB_11 => 11db, 3.55x, 150-2450mV
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
    LEAF_NOTICE("Set ADC resolution=%d attenuation=%d", resolution, attenuation);
    analogReadResolution(resolution);
    analogSetAttenuation((adc_attenuation_t)attenuation);
    LEAF_INFO("Analog input leaf has %d channels", channels);
    for (int c=0; c<channels;c++) {
      LEAF_NOTICE("%s channel %d claims pin %d", base_topic.c_str(), c+1, inputPin[c]);
      adcAttachPin(inputPin[c]);
      //analogSetPinAttenuation(inputPin[c], (adc_attenuation_t)3/*ADC_ATTEN_DB_11*/); // 11db, 3.55x, 150-2450mV
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
#ifdef ESP32
    portENTER_CRITICAL(&adc1Mux);
#endif
    int new_raw = analogRead(inputPin[c]);
    int new_raw_mv = analogReadMilliVolts(inputPin[c]);
#ifdef ESP32
    portEXIT_CRITICAL(&adc1Mux);
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
      LEAF_NOTICE("Analog input #%d on pin %d => %d (%dmV) (change=%.1f%% n=%d mean=%d)", c+1, inputPin[c], raw[c], new_raw_mv, delta_pc, raw_n[c], raw_s[c]/raw_n[c]);
    }
    
    return changed;
  }

  virtual bool sample(void) 
  {
    return sample(0);
  }
  
  virtual void loop(void) {
    Leaf::loop();
    bool changed = false;
    unsigned long now = millis();

    for (int c=0; c<channels; c++) {
      if ((pubsubLeaf && pubsubLeaf->isConnected() && (last_sample[c] == 0)) ||
	  (now >= (last_sample[c] + sample_interval_ms))
	) {
	//LEAF_DEBUG("taking a sample for channel %d", c);
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
      status_pub();
      last_report = now;
    }

    //LEAF_LEAVE;
  };

};


// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
