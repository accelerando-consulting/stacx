//
//@************************ class BatteryLevelLeaff ******************************
//
// This class encapsulates an analog input sensor that publishes measured
// voltage values to MQTT
//
//  Attenuation       Measurable input voltage range
//  ADC_ATTEN_DB_0    100 mV ~ 950 mV
//  ADC_ATTEN_DB_2_5  100 mV ~ 1250 mV
//  ADC_ATTEN_DB_6    150 mV ~ 1750 mV
//  ADC_ATTEN_DB_11   150 mV ~ 2450 mV
//
#pragma once

class BatteryLevelLeaf : public Leaf
{
protected:
  static const int oversample = 5;
  int raw;
  int history[oversample];
  int history_pos = 0;
  int value=-1;
  int inputPin=-1;
  int resolution=12;
  int attenuation=3;
  unsigned long last_sample=0;
  unsigned long last_report=0;
  int sample_interval_ms=1000;
  int report_interval_sec=60;
  int vdivHigh=0,vdivLow=1;
  float scaleFactor=1;
  int delta=1;

public:
  BatteryLevelLeaf(String name, pinmask_t pins, int vdivHigh=0, int vdivLow=1, int resolution=12, int attenuation=3)  : Leaf("battery", name, pins)
  {
    report_interval_sec = 60;
    sample_interval_ms = 12000;
    delta = 10;
    last_report = 0;
    this->vdivLow = vdivLow;
    this->vdivHigh = vdivHigh;
    this->resolution = resolution;
    this->attenuation = attenuation;
    scaleFactor = (vdivLow+vdivHigh)/vdivLow;
    for (int n=0;n<oversample;n++) {
      history[n]=-1;
    }
  
    FOR_PINS({inputPin=pin;});
  };

  virtual void setup(void) 
  {
    Leaf::setup();
    analogReadResolution(resolution);
    analogSetAttenuation((adc_attenuation_t)attenuation); 
    LEAF_NOTICE("%s claims pin %d", base_topic.c_str(), inputPin);
    adcAttachPin(inputPin);
    
    LEAF_NOTICE("Analog input divider is [%d:%d] => scale factor %.3f", vdivHigh,vdivLow, scaleFactor);
  }


  virtual void status_pub()
  {
    mqtt_publish("status/battery", String((int)value));
  };

  virtual bool sample(void) 
  {
    // time to take a new sample
    int new_raw = 0;

    history[history_pos++] = analogReadMilliVolts(inputPin);
    if (history_pos >= oversample) history_pos = 0;
    int s = 0;
    for (int n=0; n<oversample;n++) {
      if (history[n]<0) continue;
      new_raw+=history[n];
      s++;
    }
    new_raw /= s;
    
    float delta_pc = (raw?(100*(raw-new_raw)/raw):0);
    bool changed =
      (last_sample == 0) ||
      (raw < 0) ||
      ((raw > 0) && (abs(delta_pc) > delta));
    LEAF_INFO("Sampling Analog input on pin %d => %d", inputPin, new_raw);

    if (changed) {
      raw = new_raw;
      value = raw * scaleFactor;
      LEAF_NOTICE("Battery level on pin %d => %d (%dmV) (change=%.1f%%)", inputPin, raw, value, delta_pc);
    }
    return changed;
  }
  
  virtual void loop(void) {
    Leaf::loop();
    bool changed = false;
    unsigned long now = millis();

    if ((last_sample == 0) ||
      (now >= (last_sample + sample_interval_ms))
	) {
      changed = sample();
      last_sample = now;
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
