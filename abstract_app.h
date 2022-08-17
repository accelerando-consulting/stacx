//
// This class implements the core behaviours for application logic
//
#pragma once

#ifndef ESP8266
#define RTC_SIG 0xFEEDFACE
RTC_DATA_ATTR uint32_t saved_sig = 0;
#endif

class AbstractAppLeaf : public Leaf
{
protected:

public:
  AbstractAppLeaf(String name, String target)
    : Leaf("app", name)
  {
    LEAF_ENTER(L_INFO);
    this->do_heartbeat = true;
    this->do_status = true;
    this->tap_targets = target;
    this->impersonate_backplane = true;

    LEAF_LEAVE;
  }

#ifndef ESP8266
  virtual void load_sensors(){};
  virtual void save_sensors(){};
#endif
  
  virtual void setup(void) {
    Leaf::setup();
    LEAF_ENTER(L_INFO);

    heartbeat_interval_seconds = (unsigned long)getIntPref("heartbeat_interval_seconds", (int)heartbeat_interval_seconds, "Interval (seconds) for periodic heartbeat");
    getIntPref("log_level", &debug_level, "Log verbosity level (ALERT=0,WARN=2,NOTICE=2,INFO=3,DEBUG=4)");

#ifndef ESP8266
    if (wake_reason.startsWith("deepsleep/")) {
      if (saved_sig == RTC_SIG) {
	load_sensors();
	LEAF_NOTICE("Recovered saved sensors from RTC memory");
      }
      else {
	LEAF_NOTICE("No saved sensor values");
      }
    }
#endif
    
    LEAF_LEAVE;
  }

  virtual void loop() 
  {
    Leaf::loop();
  }

  virtual void start()
  {
    Leaf::start();
    LEAF_ENTER(L_NOTICE);

#ifndef ESP8266
    if (wake_reason == "other") {  // cold boot
      if (ipLeaf && !ipLeaf->canRun()) ipLeaf->start();
      if (pubsubLeaf && !pubsubLeaf->canRun()) pubsubLeaf->start();
    }
#endif
    LEAF_VOID_RETURN;
  }

  virtual void mqtt_do_subscribe() {
    LEAF_ENTER(L_NOTICE);
    Leaf::mqtt_do_subscribe();
#ifdef BUILD_NUMBER
    mqtt_publish("status/build", String(BUILD_NUMBER));
#endif
    LEAF_LEAVE;
  }

  virtual bool mqtt_receive(String type, String name, String topic, String payload) 
  {
    return Leaf::mqtt_receive(type, name, topic, payload);
  }

  virtual void pre_reboot()
  {
    LEAF_NOTICE("pre_reboot");
    ACTION("REBOOT");
#ifndef ESP8266
    save_sensors();
#endif
  }

  virtual void pre_sleep(int duration=0)
  {
    LEAF_ENTER(L_NOTICE);
#if !defined(ESP8266) && defined(helloPin)
    digitalWrite(helloPin, HELLO_OFF);
    gpio_hold_en((gpio_num_t)helloPin);
#endif

#ifndef ESP8266
    // stash current sensor values in RTC memory
    save_sensors();
#endif
    LEAF_LEAVE;
  }

  virtual void post_sleep()
  {
#if !defined(ESP8266) && defined(helloPin)
    digitalWrite(helloPin, HELLO_OFF);
    gpio_hold_dis((gpio_num_t)helloPin);
#endif
  }

};

  

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
