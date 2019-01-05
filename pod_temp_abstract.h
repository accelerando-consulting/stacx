//
//@**************************** class DHTPod ******************************
// 
// This class encapsulates a temp/humidity sensor that publishes measured
// environment values to MQTT
// 

class AbstractTempPod : public Pod
{
public:
  float humidity;
  float temperature;
  unsigned long last_sample;
  unsigned long last_report;
  int sample_interval_ms;
  int report_interval_sec;
  int delta;
 
  AbstractTempPod(String name, pinmask_t pins) : Pod("dht", name, pins) {
    ENTER(L_INFO);
    temperature = humidity = 0;
    report_interval_sec = 300;
    sample_interval_ms = 60000;
    delta = 5;
    last_sample = 0;
    last_report = 0;
    
    LEAVE;
  }

  void status_pub() 
  {
      mqtt_publish("status/temperature", String(temperature,1));
      mqtt_publish("status/humidity", String(humidity, 1));
      mqtt_publish("status/temperature/integer", String(temperature,0));
      mqtt_publish("status/humidity/integer", String(humidity, 0));
  }
  
  virtual bool poll(float *h, float *t, const char **status) 
  {
    // Don't call this, you must override in subclass
  }
  
  
  void loop(void) {
    Pod::loop();
    unsigned long now = millis();
    bool changed = false;

    if ((mqttConnected && (last_sample == 0)) ||
	((sample_interval_ms + last_sample) <= now)
      ) {
      INFO("Sampling DHT");
      // time to take a new sample
      float h,t;
      const char *status;
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
    }
    
    if ( (mqttConnected && (last_report == 0)) ||
	 changed ||
	 ((last_report + report_interval_sec * 1000) <= now)
      ) {
      // Publish a report every N seconds, or if changed by more than d%
      status_pub();
      last_report = now;
    }
    
    //LEAVE;
  }
};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
