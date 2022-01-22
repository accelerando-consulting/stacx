//
//@**************************** class DHTLeaf ******************************
//
// This class encapsulates an analog input sensor publishes measured
// voltage values to MQTT
//
#pragma once

#define ANALOG_INPUT_CHAN_MAX 4

#ifdef ESP32
portMUX_TYPE adc1Mux = portMUX_INITIALIZER_UNLOCKED;
#endif

class AnalogInputLeaf : public Leaf
{
protected:
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
  int delta;
  int fromLow, fromHigh;
  float toLow, toHigh;

public:
  AnalogInputLeaf(String name, pinmask_t pins, int in_min=0, int in_max=1023, float out_min=0, float out_max=100, bool asBackplane = false) : Leaf("analog", name, pins)
  {
    report_interval_sec = 2;
    sample_interval_ms = 200;
    delta = 10;
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
    LEAF_INFO("Analog input leaf has %d channels", channels);
    for (int c=0; c<channels;c++) {
      LEAF_INFO("  Channel %d is pin %d", c+1, inputPin[c]);
    }
    LEAF_INFO("Analog input mapping [%d:%d] => [%.3f,%.3f]", fromLow, fromHigh, toLow, toHigh);
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
    //mqtt_publish("status/value", values);
  };

  virtual bool sample(int c)
  {
    // time to take a new sample
#ifdef ESP32
    portENTER_CRITICAL(&adc1Mux);
#endif
    int new_raw = analogRead(inputPin[c]);
#ifdef ESP32
    portEXIT_CRITICAL(&adc1Mux);
#endif
    float delta_pc = (raw[c]?(100*(raw[c]-new_raw)/raw[c]):0);
    bool changed =
      (last_sample[c] == 0) ||
      (raw[c] < 0) ||
      ((raw[c] > 0) && (abs(delta_pc) > delta));
    LEAF_DEBUG("Sampling Analog input %d on pin %d => %d", c+1, inputPin[c], new_raw);
    raw_n[c]++;
    raw_s[c]+=new_raw;

    if (changed) {
      raw[c] = new_raw;
      LEAF_NOTICE("Analog input #%d on pin %d => %d (change=%.1f%% n=%d mean=%d)", c+1, inputPin[c], raw[c], delta_pc, raw_n[c], raw_s[c]/raw_n[c]);
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
      if ((mqttConnected && (last_sample[c] == 0)) ||
	  (now >= (last_sample[c] + sample_interval_ms))
	) {
	//LEAF_DEBUG("taking a sample for channel %d", c);
	changed |= sample(c);
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
	 (mqttConnected && (last_report == 0))
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
