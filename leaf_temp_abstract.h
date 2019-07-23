//
//@**************************** class DHTLeaf ******************************
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
    
 
  unsigned long last_sample;
  unsigned long last_report;
  int sample_interval_ms;
  int report_interval_sec;
  int delta;
 
  AbstractTempLeaf(String name, pinmask_t pins) : Leaf("dht", name, pins) {
    ENTER(L_INFO);
    temperature = humidity = NAN;
    ppmCO2 = ppmeCO2 = ppmtVOC = NAN;
    rawH2 = rawEthanol = NAN;

    report_interval_sec = 60;
    sample_interval_ms = 2000;
    delta = 2;
    last_sample = 0;
    last_report = 0;
    
    LEAVE;
  }

  virtual void status_pub() 
  {
    ENTER(L_DEBUG);

    if (!isnan(temperature)) {
      mqtt_publish("status/temperature", String(temperature,1));
      mqtt_publish("status/temperature/integer", String(temperature,0));
    }
    if (!isnan(humidity)) {
      mqtt_publish("status/humidity", String(humidity, 1));
      mqtt_publish("status/humidity/integer", String(humidity, 0));
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

    LEAVE;
  }
  
  virtual bool poll(float *h, float *t, const char **status) 
  {
    // Don't call this, you must override in subclass
  }
  
  
  void loop(void) {
    Leaf::loop();
    //ENTER(L_INFO);
    
    unsigned long now = millis();
    bool changed = false;
    bool sleep = false;

    if ((mqttConnected && (last_sample == 0)) ||
	((sample_interval_ms + last_sample) <= now)
      ) {
      DEBUG("Sampling dht %s/%s", this->leaf_type.c_str(), this->leaf_name.c_str());
      // time to take a new sample
      float h,t;
      const char *status = "";

      if (poll(&h, &t, &status)) {
	DEBUG("h=%.1f t=%.1f (%s)", h, t, status);
	changed = (last_sample == 0) || (humidity == 0) || (temperature == 0) || 
	  (abs(100*(humidity-h)/humidity) > delta) ||
	  (abs(100*(temperature-t)/temperature) > delta) ;

	temperature = t;
	humidity = h;
      }
      else {
	ALERT("Poll failed %s", base_topic.c_str());
      }
      last_sample = now;
      //sleep = true;
    }
    
    if ( (mqttConnected && (last_report == 0)) ||
	 changed ||
	 ((last_report + report_interval_sec * 1000) <= now)
      ) {
      // Publish a report every N seconds, or if changed by more than d%
      status_pub();
      last_report = now;
      //sleep = true;
    }

    if (sleep) {
      initiate_sleep_ms(sample_interval_ms);
    }
    //LEAVE;
  }
};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
