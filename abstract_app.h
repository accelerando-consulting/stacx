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
  bool app_publish_version = false;
  bool app_use_lte = true;
  bool app_use_lte_gps = false;
  bool app_use_wifi = false;

public:
  AbstractAppLeaf(String name, String target=NO_TAPS)
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

    heartbeat_interval_seconds = (unsigned long)getIntPref("heartbeat_interval_sec", (int)heartbeat_interval_seconds, "Interval (seconds) for periodic heartbeat");
    blink_enable = getIntPref("blink_enable", 1, "Enable the device identification blink");
    if (debug_level >= 0) {
      getIntPref("debug_level", &debug_level, "Log verbosity level (ALERT=0,WARN=2,NOTICE=2,INFO=3,DEBUG=4)");
    }
    getBoolPref("debug_flush", &debug_flush, "Flush stream after every log message");
    getBoolPref("debug_lines", &debug_lines, "Include line numbers log messages");
    getBoolPref("app_publish_version", &app_publish_version, "Publish version information at first connect");

    getBoolPref("app_use_lte", &app_use_lte, "Enable use of 4G (LTE) modem");
    getBoolPref("app_use_lte_gps", &app_use_lte_gps, "Enable use of 4G (LTE) modem (for GPS only)");
    getBoolPref("app_use_wifi", &app_use_wifi, "Enable use of WiFi");

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

    if (ipLeaf->canRun()) {
      LEAF_NOTICE("Application will use network transport %s", ipLeaf->describe().c_str());
    }
    else {
      LEAF_NOTICE("No IP active yet, choosing a transport layer to activate");

      // IP is not currently activated, choose one of the available IP leaves to activate

      AbstractIpLTELeaf *lte = NULL;
      if (app_use_lte || app_use_lte_gps) {
	lte = (AbstractIpLTELeaf *)find("lte","ip");
      }
    
      if (lte && app_use_lte_gps) {
	LEAF_NOTICE("LTE will be used for GPS only");
	lte->permitRun();
	lte->setup();
      }

      bool ip_valid = false;
      if (app_use_lte) {
	AbstractPubsubLeaf *lte_pubsub =(AbstractPubsubLeaf *)find("ltemqtt", "pubsub");
	
	if (lte && lte_pubsub) {
	  LEAF_WARN("Selecting LTE as preferred communications");
	  stacxSetComms(lte, lte_pubsub);
	  ip_valid=true;
	}
	else {
	  LEAF_ALERT("Cannot find LTE ip/pubsub leaf-pair");
	}
      }
      else {
	LEAF_NOTICE("Use of LTE for comms is not enabled");
      }
    
      AbstractIpLTELeaf *wifi = (AbstractIpLTELeaf *)find("wifi","ip");
      if (wifi && app_use_wifi && app_use_lte && lte && lte->canRun()) {
	// The wifi leaf is enabled, but so is LTE.  We presume wifi is to be for SECONDARY comms,
	// which means it does not advertise its presence to a pubsub leaf.
	//
	// Wifi will be used for debugging and for OTA updates only
	//
	LEAF_WARN("Enabling WiFi leaf as secondary comms, for Debug and OTA only");
	wifi->permitRun();
      }
      else if (wifi && app_use_wifi) {
	// Wifi is the primary comms method
	AbstractPubsubLeaf *wifi_pubsub =(AbstractPubsubLeaf *)find("wifimqtt", "pubsub");

	if (!wifi || !wifi_pubsub) {
	  LEAF_ALERT("Cannot find WiFi ip/pubsub leaf-pair");
	}
	else {

	  LEAF_NOTICE("Selecting WiFi as preferred communications");
	  // The wifi is the primary comms
	  stacxSetComms(wifi, wifi_pubsub);
	  ip_valid = true;
	}
      }
      else if (wifi && !app_use_wifi) {
	LEAF_NOTICE("Use of WiFi for WiFi comms is not enabled");
      }

      if (!ip_valid) {
	// no comms preference, start the null comms modules if present
	AbstractIpLeaf *nullip=(AbstractIpLeaf *)find("nullip","ip");
	AbstractPubsubLeaf *nullps=(AbstractPubsubLeaf *)find("nullmqtt","pubsub");
	if (nullip && nullps) {
	  LEAF_NOTICE("Configuring null communications (local messages only)");
	  stacxSetComms(nullip, nullps);
	  ip_valid=true;
	}
      }

      if (!ip_valid) {
	LEAF_ALERT("No valid communications module selected (not even null)");
      }
    }
    
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
    if (app_publish_version) {
#ifdef BUILD_NUMBER
      mqtt_publish("status/build", String(BUILD_NUMBER));
#endif
#ifdef FIRMWARE_VERSION
      mqtt_publish("firmware", String(FIRMWARE_VERSION));
#endif
#ifdef HARDWARE_VERSION    
      mqtt_publish("hardware", String(HARDWARE_VERSION));
#endif
    }
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
