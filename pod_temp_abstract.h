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

  void mqtt_subscribe() {
    ENTER(L_NOTICE);
    _mqtt_subscribe(base_topic+"/cmd/status");
    LEAVE;
  }
  
  bool mqtt_receive(String type, String name, String topic, String payload) {
    if (!Pod::mqtt_receive(type, name, topic, payload)) return false;
    ENTER(L_DEBUG);
    bool handled = false;
    
    WHEN("cmd/status",{
      INFO("Refreshing device status");
      mqtt_publish("status/temperature", String(temperature,1));
      mqtt_publish("status/humidity", String(humidity, 1));
    });

    LEAVE;
    return handled;
  };

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

    if (changed || (last_report + report_interval_sec * 1000) <= now) {
      // Publish a report every N seconds, or if changed by more than d%
      mqtt_publish("status/temperature", String(temperature,1));
      mqtt_publish("status/humidity", String(humidity, 1));
      last_report = now;
    }
    
    //LEAVE;
  }
};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
