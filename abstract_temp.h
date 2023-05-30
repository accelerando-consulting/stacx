//
//@********************** class AbstractTempLeaf ************************
// 
// This class encapsulates a temp/humidity sensor that publishes measured
// environment values to MQTT
// 
#pragma once 

class AbstractTempLeaf : public Leaf
{
public:
  float humidity;
  float humidity_threshold=1;
  float temperature;
  float temperature_threshold=0.5;
  float ppmCO2;
  float ppmCO2_threshold=50;
  float ppmeCO2;
  float ppmtVOC;
  float rawH2;
  float rawEthanol;
  float pressure;
 
  unsigned long last_sample;
  unsigned long last_report;
  int sample_interval_ms;
  int report_interval_sec;
  int delta;
 
  AbstractTempLeaf(String name, pinmask_t pins)
    : Leaf("temp", name, pins)
    , Debuggable(name)
  {
    LEAF_ENTER(L_INFO);
    temperature = humidity = NAN;
    ppmCO2 = ppmeCO2 = ppmtVOC = NAN;
    rawH2 = rawEthanol = NAN;
    pressure = NAN;

    report_interval_sec = 60;
    sample_interval_ms = 2000;
    delta = 2;
    last_sample = 0;
    last_report = 0;
    
    LEAF_LEAVE;
  }

  virtual void status_pub() 
  {
    LEAF_ENTER(L_DEBUG);

    if (!isnan(temperature)) {
      mqtt_publish("status/temperature", String(temperature,1));
      mqtt_publish("status/temperature/integer", String(temperature,0));
    }
    if (!isnan(humidity)) {
      mqtt_publish("status/humidity", String(humidity, 1));
      mqtt_publish("status/humidity/integer", String(humidity, 0));
    }
    if (!isnan(pressure)) {
      mqtt_publish("status/pressure", String(pressure, 1));
      mqtt_publish("status/pressure/integer", String(pressure, 0));
    }
    if (!isnan(ppmCO2)) {
      mqtt_publish("status/ppmCO2", String(ppmCO2, 1));
    }
    if (!isnan(ppmeCO2)) {
      mqtt_publish("status/ppmeCO2", String(ppmeCO2,1));
    }
    if (!isnan(ppmtVOC)) {
      mqtt_publish("status/ppmtVOC", String(ppmtVOC,1));
    }
    if (!isnan(rawH2)) {
      mqtt_publish("status/rawH2", String(rawH2,1));
    }
    if (!isnan(rawEthanol)) {
      mqtt_publish("status/rawEthanol", String(rawEthanol,1));
    }

    LEAF_LEAVE;
  }
  
  virtual bool poll(float *h, float *t, const char **status) 
  {
    LEAF_ALERT("Nono don't call AbstractTempLeaf::poll!");
    // Don't call this, you must override in subclass
    return false;
  }

  virtual bool hasChanged(float h, float t) 
  {
    bool changed = false;

    // no valid reading 
    if (isnan(h) && isnan(t)) return false;

    // first valid reading
    if (isnan(humidity) || isnan(temperature)) return true;
    if ((humidity == 0) && (h!=0)) return true;
    if ((temperature == 0) && (t!=0)) return true;

    float percent = fabs(100*(humidity-h)/humidity);
    //LEAF_DEBUG("Humidity changed by %.1f%% (%.1f => %.1f)", percent, humidity, h);
    if (percent >= delta) {
      LEAF_INFO("Humidity changed by %.1f%% (%.1f => %.1f)", percent, humidity, h);
      changed =true;
    }
    percent = fabs(100*(temperature-t)/temperature);
    //LEAF_DEBUG("Humidity changed by %.1f%% (%.1f => %.1f)", percent, humidity, h);
    if (percent >= delta) {
      LEAF_INFO("Temperature changed by %.1fC (%.1f => %.1f)", percent, temperature, t);
      changed =true;
    }
    return changed;
  }
  
  void loop(void) {
    LEAF_ENTER(L_TRACE);
    Leaf::loop();
    
    unsigned long now = millis();
    bool changed = false;
    bool sleep = false;

    if ((last_sample == 0) ||
	((sample_interval_ms + last_sample) <= now)
      ) {
      //LEAF_DEBUG("Sampling %s/%s", this->leaf_type.c_str(), this->leaf_name.c_str());
      // time to take a new sample
      float h=NAN, t=NAN;
      const char *status = "";

      if (poll(&h, &t, &status)) {
	//LEAF_DEBUG("h=%.1f t=%.1f (%s)", h, t, status);
	changed = hasChanged(h,t);
	if (!isnan(t)) temperature = t;
	if (!isnan(h)) humidity = h;
      }
      last_sample = now;
      //sleep = true;
    }
    
    if ( (pubsubLeaf && pubsubLeaf->isConnected() && (last_report == 0)) ||
	 changed ||
	 ((last_report + report_interval_sec * 1000) <= now)
      ) {
      // Publish a report every N seconds, or if changed by more than d%
      status_pub();
      last_report = now;
      //sleep = true;
    }

    if (sleep) {
      pubsubLeaf->initiate_sleep_ms(sample_interval_ms);
    }
    LEAF_LEAVE;
  }
};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
