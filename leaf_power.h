static const char *planets[]={"Netpune","Uranus","Jupiter","Mars","Earth"};

#define SleepyLeaf PowerLeaf

class PowerLeaf : public Leaf
{
public:
  //
  // Declare your leaf-specific instance data here
  //
  bool use_sleep = true;
  int sleep_timeout_sec = 15;
  int sleep_duration_sec = 15;
  unsigned long last_activity_ms = 0;
  float warp_factor = 0;

  //
  // Leaf constructor method(s)
  // Call the superclass constructor to handle common arguments (type, name, pins)
  //
  PowerLeaf(String name, String target=NO_TAPS, float warp_factor=0)
    : Leaf("power", name)
    , Debuggable(name)
  {
    tap_targets = target;
  }

  //
  // Arduino Setup function
  // Use this to configure any IO resources
  //
  void setup(void) {
    Leaf::setup();
    LEAF_ENTER(L_INFO);

    registerBoolValue("use_sleep", &use_sleep); // used to temporarily disable sleep
    registerFloatValue("sleep_warp_factor", &warp_factor); // used for power-consumption testing
    registerIntValue("sleep_timeout_sec", &sleep_timeout_sec);
    registerIntValue("sleep_duration_sec", &sleep_duration_sec);

    // exponentially increase sleep duration after each reboot in the test
    // cycle
    if (warp_factor) {
      for (int i=0; i<boot_count-1; i++) {
	sleep_duration_sec *= warp_factor;
      }
      LEAF_NOTICE("Sleep duration for boot %d will be %d", boot_count, (int)sleep_duration_sec);
    }
    LEAF_LEAVE;
  }

  void initiate_sleep_ms(int ms)
  {
    time_t now;
    time(&now);
    const char *ctimbuf = ctime(&now);
    int leaf_index;

    LEAF_NOTICE("Prepare for deep sleep at unix time %llu (%s)\n", (unsigned long long)now, ctimbuf);
    if (!use_sleep) {
      LEAF_NOTICE("Sleep is disabled by configuration use_sleep");
      return;
    }

    // Apply sleep in reverse order, highest level leaf first
    for (leaf_index=0; leaves[leaf_index]!=NULL; leaf_index++);
    int leaf_count = leaf_index;
    for (leaf_index=leaf_count-1; leaf_index>=0; leaf_index--) {
      if (leaves[leaf_index]) {
	leaves[leaf_index]->pre_sleep(ms/1000);
      }
    }

    // kill the hello world leds
    post_error_state = POST_IDLE;
    idle_pattern(0,0);

    if (ms == 0) {
      LEAF_NOTICE("Initiating indefinite deep sleep #%d (wake source GPIO0)", boot_count);
      ACTION("SLEEP indef");
    }
    else {
      time_t then;
      time(&then);
      then += (ms/1000);
      ctimbuf = ctime(&then);
      LEAF_NOTICE("Initiating deep sleep #%d (wake sources GPIO0 plus timer %dms), alarm at %s", boot_count, ms, ctimbuf);
      ACTION("SLEEP %d", (int)(ms/1000))
    }
    DBGPRINTLN("\n\nASLEEP\n");
    DBGFLUSH();
    if (ms != 0) {
      // zero means forever
      esp_sleep_enable_timer_wakeup(ms * 1000ULL);
    }

#if defined(ESP32) && !defined(ARDUINO_ESP32C3_DEV)    
    esp_sleep_enable_ext0_wakeup((gpio_num_t)0, 0);
#endif
    
    esp_deep_sleep_start();
  }

  //
  // MQTT message callback
  // (Use the superclass callback to ignore messages not addressed to this leaf)
  //
  virtual bool mqtt_receive(String type, String name, String topic, String payload, bool direct=false)
  {
    bool handled = false;

    if (topic=="cmd/lowpower") {
      int ms = payload.toInt();
      initiate_sleep_ms(ms);
    }
    else {
      handled = Leaf::mqtt_receive(type, name, topic, payload, direct);
    }

    return handled;
  }

  //
  // Arduino loop function
  //
  void loop(void) {
    Leaf::loop();

    static int warps = sleep_timeout_sec/5;
    static unsigned long last_warp = 0;

    if (warp_factor != 0) {

      const char *planet = planets[(boot_count-1)%5];
      unsigned long now = millis();
      unsigned long time_since_activity = now - last_activity_ms;

      if (now > (last_warp + 5000)) {
	last_warp = now;
	LEAF_WARN("%d warp%s to %s", warps, (warps==1)?"":"s", planet);
	--warps;
      }

      if ((sleep_timeout_sec > 0) && (time_since_activity > (sleep_timeout_sec*1000))) {
	LEAF_WARN("%s!  Bonus Sleep Stage (%d sec).", planet, sleep_duration_sec);
	last_activity_ms = now;
	LEAF_NOTICE("Time to go to sleep (exceeded timeout of %u sec, will sleep %u)",
		    sleep_timeout_sec, sleep_duration_sec);
	char buf[80];
	//this->initiate_sleep_ms((int)sleep_duration_sec*1000); // 0 means forever
	publish("cmd/lowpower", String((int)sleep_duration_sec*1000,10));
      }
    }

  }


};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
