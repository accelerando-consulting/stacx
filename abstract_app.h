//
// This class implements the core behaviours for application logic
//
#pragma once

#ifndef ESP8266
#define RTC_SIG 0xFEEDFACE
RTC_DATA_ATTR uint32_t saved_sig = 0;
#endif

#ifndef APP_PUBLISH_VERSION
#define APP_PUBLISH_VERSION false
#endif
#ifndef APP_USE_LTE
#define APP_USE_LTE true
#endif
#ifndef APP_USE_LTE_GPS
#define APP_USE_LTE_GPS false
#endif
#ifndef APP_USE_WIFI
#define APP_USE_WIFI false
#endif

#ifndef APP_LOG_BOOTS
#define APP_LOG_BOOTS false
#endif



class AbstractAppLeaf : public Leaf
{
protected:
  bool app_publish_version = APP_PUBLISH_VERSION;
  bool app_use_lte = APP_USE_LTE;
  bool app_use_lte_gps = APP_USE_LTE_GPS;
  bool app_use_wifi = APP_USE_WIFI;
  String qa_id="";
  bool app_use_brownout = true;
  bool app_log_boots = APP_LOG_BOOTS;

public:
  AbstractAppLeaf(String name, String targets=NO_TAPS)
    : Leaf("app", name)
    , Debuggable(name)
  {
    this->do_heartbeat = true;
    this->do_status = true;
    this->tap_targets = targets;
    this->impersonate_backplane = true;
  }

#ifndef ESP8266
  virtual void load_sensors(){};
  virtual void save_sensors(){};
#endif

  virtual void setup(void) {
    Leaf::setup();
    LEAF_ENTER(L_INFO);

    registerBoolValue("identify", &identify, "Enable the device identification blink", ACL_GET_SET, VALUE_NO_SAVE);
    registerBoolValue("app_publish_version", &app_publish_version, "Publish version information at first connect");
    registerBoolValue("app_log_boots", &app_log_boots, "Log reboot reasons to flash");

    registerLeafBoolValue("use_lte", &app_use_lte, "Enable use of 4G (LTE) modem");
    registerLeafBoolValue("use_lte_gps", &app_use_lte_gps, "Enable use of 4G (LTE) modem (for GPS only)");
    registerLeafBoolValue("use_wifi", &app_use_wifi, "Enable use of WiFi");
    registerLeafStrValue("qa_id", &qa_id); // unlisted, ID for automated test
    registerLeafBoolValue("use_brownout", &app_use_brownout, "Enable use of brownout detector");


#ifndef ESP8266
    if (wake_reason.startsWith("deepsleep/")) {
      if (saved_sig == RTC_SIG) {
	load_sensors();
	LEAF_DEBUG("Recovered saved sensors from RTC memory");
      }
      else {
	LEAF_DEBUG("No saved sensor values");
      }
    }
#endif


    if (ipLeaf && ipLeaf->canRun()) {
      LEAF_NOTICE("Application will use network transport %s", ipLeaf->describe().c_str());
    }
    else {
      LEAF_NOTICE("No IP active yet, choosing a transport layer to activate");

      // IP is not currently activated, choose one of the available IP leaves to activate

      AbstractIpLeaf *lte = NULL;
      if (app_use_lte || app_use_lte_gps) {
	lte = (AbstractIpLeaf *)find("lte","ip");
      }

      if (lte && app_use_lte_gps) {
	stacx_heap_check(HERE);
	LEAF_WARN("LTE will be enabled but for GPS only");
	lte->permitRun();
	// non-persistently disable ip autoconnect for this leaf
	lte->setup();
	//lte->setValue("lte_ip_autoconnect", "off", true, false);
	lte->setValue("ip_enable_gps_only", "on", true, false);
	stacx_heap_check(HERE);
      }

      bool ip_valid = false;
      if (app_use_lte) {
	AbstractPubsubLeaf *lte_pubsub =(AbstractPubsubLeaf *)find("ltemqtt", "pubsub");

	if (lte && lte_pubsub) {
	  stacx_heap_check(HERE);
	  LEAF_WARN("Selecting LTE as preferred communications");
	  stacxSetComms(lte, lte_pubsub);
	  stacx_heap_check(HERE);
	  ip_valid=true;
	}
	else {
	  LEAF_ALERT("Cannot find LTE ip/pubsub leaf-pair");
	}
      }
      else {
	LEAF_NOTICE("Use of LTE for comms is not enabled");
      }

      AbstractIpLeaf *wifi = (AbstractIpLeaf *)find("wifi","ip");
      if (wifi && app_use_wifi && app_use_lte && lte && lte->canRun()) {
	// The wifi leaf is enabled, but so is LTE.  We presume wifi is to be for SECONDARY comms,
	// which means it does not become the primary pubsub channel
	//
	// Wifi will be used for debugging and for OTA updates only
	//
	stacx_heap_check(HERE);
	LEAF_WARN("Enabling WiFi leaf as secondary comms, for service operations only");
	if (wifi->hasPriority())  {
	  wifi->usePriority("service");
	}
	wifi->permitRun();
	wifi->setup();
	AbstractPubsubLeaf *wifi_pubsub =(AbstractPubsubLeaf *)find("wifimqtt", "pubsub");
	if (wifi_pubsub && wifi_pubsub->hasPriority())  {
	  LEAF_WARN("Enabling WiFi pubsub for service operations only");
	  wifi_pubsub->usePriority("service");
	  wifi->setComms(wifi, wifi_pubsub);
	  wifi_pubsub->setComms(wifi, wifi_pubsub);
	  wifi_pubsub->permitRun();
	  wifi_pubsub->setup();
	}
	stacx_heap_check(HERE);
      }
      else if (wifi && app_use_wifi) {
	// Wifi is the primary comms method
	AbstractPubsubLeaf *wifi_pubsub =(AbstractPubsubLeaf *)find("wifimqtt", "pubsub");

	if (!wifi || !wifi_pubsub) {
	  LEAF_ALERT("Cannot find WiFi ip/pubsub leaf-pair");
	}
	else {
	  LEAF_WARN("Selecting WiFi as preferred communications");
	  // The wifi is the primary comms
	  stacx_heap_check(HERE);
	  stacxSetComms(wifi, wifi_pubsub);
	  stacx_heap_check(HERE);
	  ip_valid = true;
	}
      }
      else if (wifi && !app_use_wifi) {
	LEAF_NOTICE("Use of WiFi for comms is not enabled");
      }

      if (!ip_valid) {
	// no comms preference, start the null comms modules if present
	AbstractIpLeaf *nullip=(AbstractIpLeaf *)find("nullip","ip");
	AbstractPubsubLeaf *nullps=(AbstractPubsubLeaf *)find("nullmqtt","pubsub");
	if (nullip && nullps) {
	  LEAF_WARN("Configuring null communications (local messages only)");
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
    unsigned long uptime = millis();
    saved_uptime_sec = uptime/1000;
  }

  virtual void start()
  {
    Leaf::start();
    LEAF_ENTER_STR(L_INFO, String(__FILE__));

    if (app_log_boots) {
      char buf[80];
      snprintf(buf, sizeof(buf), "boot %d wake %s ran_for=%d uptime=%lu clock=%lu",
	       boot_count,
	       wake_reason.c_str(),
	       saved_uptime_sec,
	       (unsigned long)millis(),
	       (unsigned long)time(NULL));
      WARN("%s", buf);
      message("fs", "cmd/appendl/" STACX_LOG_FILE, buf);
    }

#ifndef ESP8266
    if (wake_reason == "other") {  // cold boot
      if (ipLeaf && !ipLeaf->canRun()) ipLeaf->start();
      if (pubsubLeaf && !pubsubLeaf->canRun()) pubsubLeaf->start();
    }
#endif
    LEAF_VOID_RETURN;
  }

  virtual void mqtt_do_subscribe() {
    Leaf::mqtt_do_subscribe();
    LEAF_ENTER(L_INFO);
    if (app_publish_version) {
#ifdef BUILD_NUMBER
      mqtt_publish("status/build", String(BUILD_NUMBER));
#endif
#ifdef FIRMWARE_VERSION
      mqtt_publish("status/firmware", String(FIRMWARE_VERSION));
#endif
#if HARDWARE_VERSION>=0
      mqtt_publish("status/hardware", String(HARDWARE_VERSION));
#endif
    }
    LEAF_LEAVE;
  }

  virtual void pre_reboot(String reason)
  {
    LEAF_NOTICE("pre_reboot");
    ACTION("REBOOT");
#ifndef ESP8266
    save_sensors();
#endif
    if (app_log_boots) {
      char buf[80];
      snprintf(buf, sizeof(buf), "reboot %s uptime=%lu clock=%lu",
	       reason.c_str(),
	       (unsigned long)millis(),
	       (unsigned long)time(NULL));
      message("fs", "cmd/appendl/" STACX_LOG_FILE, buf);
    }
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

  virtual bool valueChangeHandler(String topic, Value *val)
  {
    LEAF_HANDLER(L_INFO);

    WHEN("identify", {
	set_identify(identify);
	mqtt_publish("status/identify", ABILITY(identify));
      })
    ELSEWHEN("app_use_brownout", {
	LEAF_ALERT("Set brownout detection %s", ABILITY(app_use_brownout));
	if (app_use_brownout) {
	  enable_bod();
	}
	else {
	  disable_bod();
	}
      })
    else {
      handled = Leaf::valueChangeHandler(topic, val);
    }

    LEAF_HANDLER_END;
  }

};



// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
