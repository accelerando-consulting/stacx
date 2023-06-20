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
  float temperature;
  float ppmCO2;
  float ppmeCO2;
  float ppmtVOC;
  float rawH2;
  float rawEthanol;
  float pressure;

  unsigned long last_sample;
  unsigned long last_report;
  int sample_interval_ms;
  int report_interval_sec;
  float temperature_change_threshold = 0.5;
  float humidity_change_threshold = 1;
  float ppmCO2_change_threshold=50;
  int temperature_oversample = 0;
  int humidity_oversample = 0;
  float *temperature_history = NULL;
  float *humidity_history = NULL;
  int temperature_history_head = 0;
  int humidity_history_head = 0;


  AbstractTempLeaf(String name, pinmask_t pins, float temperature_change_threshold=NAN, float humidity_change_threshold=NAN, int temperature_oversample=0, int humidity_oversample=0)
    : Leaf("temp", name, pins)
    , Debuggable(name)
  {
    LEAF_ENTER(L_INFO);
    temperature = humidity = NAN;
    ppmCO2 = ppmeCO2 = ppmtVOC = NAN;
    rawH2 = rawEthanol = NAN;
    pressure = NAN;
    if (!isnan(temperature_change_threshold)) {
      this->temperature_change_threshold = temperature_change_threshold;
    }
    if (!isnan(humidity_change_threshold)) {
      this->humidity_change_threshold = humidity_change_threshold;
    }
    this->temperature_oversample = temperature_oversample;
    this->humidity_oversample = humidity_oversample;
    if (temperature_oversample) {
      temperature_history = (float *)malloc(temperature_oversample * sizeof(float));
      if (!temperature_history) temperature_oversample=0;
    }
    for (int i=0; i<temperature_oversample; i++) {
      temperature_history[i] = NAN;
    }

    if (humidity_oversample) {
      humidity_history = (float *)malloc(humidity_oversample * sizeof(float));
      if (!humidity_history) humidity_oversample=0;
    }
    for (int i=0; i<humidity_oversample; i++) {
      humidity_history[i] = NAN;
    }

    report_interval_sec = 600;
    sample_interval_ms = 2000;
    last_sample = 0;
    last_report = 0;

    LEAF_LEAVE;
  }

  virtual void setup()
  {
    Leaf::setup();
    LEAF_NOTICE("TempLeaf %s uses temperature threshold %.2f with oversampling factor %d",
		getNameStr(), temperature_change_threshold, temperature_oversample);
    LEAF_NOTICE("TempLeaf %s uses humidity threshold %.2f with oversampling factor %d",
		getNameStr(), humidity_change_threshold, humidity_oversample);

    registerLeafCommand(HERE,"poll", "poll the sensor");
  }


  virtual bool commandHandler(String type, String name, String topic, String payload) {
    LEAF_HANDLER(L_INFO);

    WHEN("poll", {
	float h;
	float t;
	poll(&h, &t, NULL);
    })
    else {
      handled = Leaf::commandHandler(type, name, topic, payload);
    }
    LEAF_BOOL_RETURN(handled);
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

  virtual bool poll(float *h, float *t, const char **status) = 0;

  virtual bool hasChanged(float h, float t)
  {
    bool changed = false;

    // no valid reading
    if (isnan(h) && isnan(t)) return false;

    // first valid reading
    if (isnan(humidity) || isnan(temperature)) return true;
    if ((humidity == 0) && (h!=0)) return true;
    if ((temperature == 0) && (t!=0)) return true;

    float delta = fabs(humidity-h);
    //LEAF_DEBUG("Humidity changed by %.1f%% (%.1f => %.1f)", delta, humidity, h);
    if (delta > humidity_change_threshold) {
      LEAF_INFO("Humidity changed by %.1f%% (%.1f => %.1f) threshold=%.2f", delta, humidity, h, humidity_change_threshold);
      changed =true;
    }
    delta = fabs(temperature-t);
    //LEAF_DEBUG("Temperature changed by %.1fC (%.1f => %.1f)", delta, temperature, t);
    if (delta > temperature_change_threshold) {
      LEAF_INFO("Temperature changed by %.1fC (%.1f => %.1f) threshold=%.2f", delta, temperature, t, temperature_change_threshold);
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

	// take a running average if enabled
	if (temperature_oversample) {
	  temperature_history[temperature_history_head] = t;
	  temperature_history_head = (temperature_history_head+1)%temperature_oversample;
	  float s=0;
	  int n=0;
	  for (int i=0; i<temperature_oversample;i++) {
	    if (isnan(temperature_history[i])) continue;
	    s+=temperature_history[i];
	    n++;
	  }
	  if (n) {
	    LEAF_INFO("Running average of %d temperature samples %.3f => %.3f",
		      n, t, s/n);
	    t = s/n;
	  }
	}

	if (humidity_oversample) {
	  humidity_history[humidity_history_head] = h;
	  humidity_history_head = (humidity_history_head+1)%humidity_oversample;
	  float s=0;
	  int n=0;
	  for (int i=0; i<humidity_oversample;i++) {
	    if (isnan(humidity_history[i])) continue;
	    s+=humidity_history[i];
	    n++;
	  }
	  if (n) {
	    LEAF_INFO("Running average of %d humidity samples %.3f => %.3f",
		      n, h, s/n);
	    h = s/n;
	  }
	}

	changed = hasChanged(h,t);
	if (!isnan(t)) temperature = t;
	if (!isnan(h)) humidity = h;
      }
      last_sample = now;
      //sleep = true;
    }

    bool do_report = false;
    // Publish a report every N seconds, or if changed by more than d%

    if (changed) {
      LEAF_INFO("Environmental readings have changed");
      do_report = true;
    }
    else if (pubsubLeaf && pubsubLeaf->isConnected() && (last_report == 0)) {
      LEAF_INFO("Initial environmental report");
      do_report = true;
    }
    else if ((last_report + report_interval_sec * 1000) <= now) {
      LEAF_INFO("Periodic environmental report");
      do_report = true;
    }

    if (do_report) {
      status_pub();
      last_report = now;
    }

    LEAF_LEAVE;
  }
};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
