//
//@************************ class RPMInputLeaf ******************************
//
// This class encapsulates an interrupt driven RPM signal measurement
//

void ARDUINO_ISR_ATTR rpminputEdgeISR(void *arg);

class RPMInputLeaf : public Leaf
{
protected:
  int rpmInputPin = -1;
  bool pullup = false;
  unsigned long report_interval_msec = 5000;
  unsigned long report_interval_msec_low = 5000;
  unsigned long report_interval_low_level = 5;
  unsigned long stats_interval_sec = 60;
  unsigned long min_pulse_usec = 200;

  /* ephemeral state */
  unsigned long pulse_count = 0;
  unsigned long ignored_count = 0;
  unsigned long last_pulse_micro = 0;
  unsigned long last_pulse_count = 0;
  float rpm;

  unsigned long last_report = 0;
  unsigned long last_stats = 0;

  bool attached=false;

public:

  RPMInputLeaf(String name, pinmask_t pins, bool pullup=false)
    : Leaf("rpminput", name, pins)
    , Debuggable(name)
  {
    LEAF_ENTER(L_DEBUG);
    this->pullup = pullup;
    this->do_heartbeat = false;
    LEAF_LEAVE;
  }

  void reset(bool reset_isr=false) {
    bool reattach = false;

    if (reset_isr && attached) {
      reattach=true;
      detach();
    }

    pulse_count=0;
    ignored_count=0;
    last_pulse_micro=last_pulse_count=0;
    rpm=0;

    if (reattach) {
      attach();
    }
  }

  unsigned long pulses_seen()
  {
    return pulse_count - last_pulse_count;
  }

  void attach()
  {
    LEAF_ENTER(L_INFO);
    pinMode(rpmInputPin, pullup?INPUT_PULLUP:INPUT);
    attachInterruptArg(rpmInputPin, rpminputEdgeISR, this, RISING);
    attached=true;
    LEAF_LEAVE;
  }

  void detach()
  {
    LEAF_ENTER(L_INFO);
    detachInterrupt(rpmInputPin);
    attached=false;
    pinMode(rpmInputPin, pullup?INPUT_PULLUP:INPUT);
    LEAF_LEAVE;
  }

  virtual void stop()
  {
    LEAF_ENTER(L_INFO);
    if (attached) detach();
    LEAF_LEAVE;
  }

  virtual void setup(void) {
    LEAF_ENTER(L_INFO);
    Leaf::setup();

    registerCommand(HERE,"reset","reset counter status");
    registerCommand(HERE,"stats","publish statistics");
    registerCommand(HERE,"test","poll in a tight loop for pulses (payload=seconds)");

    //
    // Load preferences and/or set defaults
    //
    registerLeafIntValue("report_interval_msec", &report_interval_msec, "Report rate (milliseconds)");
    registerLeafIntValue("report_interval_msec_low", &report_interval_msec_low, "Report rate (milliseconds) at low speed");
    registerLeafIntValue("report_interval_low_level", &report_interval_low_level, "Number of pulses needed to support rapid refresh");
    registerLeafIntValue("stats_interval_sec", &stats_interval_sec, "Report rate for statistics (seconds)");
    reset(false);

    FOR_PINS({rpmInputPin=pin;});
    LEAF_NOTICE("%s claims pin %d as INPUT", describe().c_str(), rpmInputPin);
    LEAF_LEAVE;
  }

  virtual void start()
  {
    LEAF_ENTER(L_INFO);
    Leaf::start();
    attach();
    LEAF_LEAVE;
  }

#if LATER
  void pulse_test(int secs)
  {
    LEAF_ENTER(L_NOTICE);

    unsigned long start=millis();
    unsigned long stop = start + ( 1000 * secs);
    int l = 0;
    unsigned long polls=0;
    unsigned long edges=0;
    unsigned long sum= 0;
    bool reattach = attached;

    if (attached) {
      detach();
    }

    l = digitalRead(counterPin);
    LEAF_NOTICE("Testing tight poll on pin %d", counterPin);
    unsigned long start_us=micros();
    int min = 4096;
    int max = 0;

    if ((mode==ONLOW) || (mode==ONHIGH)) {
      if (!analogHoldMutex(HERE)) {
	LEAF_ALERT("Can't get analog mutex");
	return;
      }
    }

    do {
      switch (mode) {
      case ONLOW:
      case ONHIGH: {
	int new_raw = analogRead(counterPin);
	if (new_raw < min) {
	  min=new_raw;
	}
	if (new_raw > max) {
	  max=new_raw;
	}
	sum += new_raw;

	if ((analog_state == LOW) && (new_raw >= analog_level_upper)) {
	  ++edges;
	  pulse();
	}
	else if ((analog_state == HIGH) && (new_raw <= analog_level_lower)) {
	  ++edges;
	  pulse();
	}
      }
	break;
      default:
      {
	int new_l = digitalRead(counterPin);
	if (new_l != l) {
	  ++edges;
	  pulse();
	  l=new_l;
	}
      }
      }
      ++polls;
    } while (millis() <stop);
    unsigned long elapsed_us = micros() - start_us;
    LEAF_NOTICE("Tested for %lu usec, got %lu edges from %lu polls (%.1fus/poll)", elapsed_us, edges, polls, (float)elapsed_us/polls);
    if ((mode==ONLOW) || (mode==ONHIGH)) {
      LEAF_NOTICE("Analog min=%d max=%d mean=%.1f", min, max, (polls>0)?((float)sum/polls):-1);
      analogReleaseMutex(HERE);
    }

    if (reattach) {
      attach();
    }

    LEAF_LEAVE;
  }
#endif

  void pulse(int inject_level=-1) {
    unsigned long unow = micros();
    if (unow < (last_pulse_micro + min_pulse_usec)) {
      ++ignored_count;
      return;
    }

    ++pulse_count;
    last_pulse_micro = unow;
  }

  virtual void status_pub()
  {
    LEAF_ENTER(L_INFO);

    unsigned long now = millis();
    unsigned long elapsed = now - last_report;
    unsigned long delta = pulse_count - last_pulse_count;

    rpm = (60.0 * 1000 * delta) / elapsed;
    LEAF_NOTICE("now=%lu/%lu p/i=%lu/%lu pulse/ms=%lu/%lu => rpm=%.3f",
		now, last_report, pulse_count, ignored_count, delta, elapsed, rpm);

    last_pulse_count = pulse_count;
    // last_report is updated by caller

    publish("status/rpm", String(rpm,2));
    LEAF_LEAVE;
  }

  virtual void stats_pub()
  {
    LEAF_ENTER(L_INFO);
    publish("stats/pulse_count", String(pulse_count),L_NOTICE,HERE);
    publish("stats/ignored_count", String(ignored_count),L_NOTICE,HERE);
    LEAF_LEAVE;
  }

  virtual bool commandHandler(String type, String name, String topic, String payload) {
    LEAF_HANDLER(L_INFO);

    WHEN("pulse", {
	LEAF_NOTICE("Simulated pulse");
	pulse(payload.toInt());
    })
    ELSEWHEN("stats", {
	LEAF_NOTICE("publish stats");
	stats_pub();
    })
#if LATER
    ELSEWHEN("test", {
	pulse_test(payload.toInt());
    })
#endif
    ELSEWHEN("reset",{
	LEAF_NOTICE("Reset requested");
	reset(true);
      })
    else {
      handled = Leaf::commandHandler(type, name, topic, payload);
    }
    LEAF_HANDLER_END;
  }

  virtual void loop(void) {
    Leaf::loop();
    unsigned long now = millis();

    if (report_interval_msec &&
	(pulses_seen() >= report_interval_low_level) &&
	(now >= (last_report + report_interval_msec))
      ) {
      // rapid report (say, 1s refresh)
      status_pub();
      last_report = now;
    }
    else if (report_interval_msec_low &&
	     (now >= (last_report + report_interval_msec_low))
      ) {
      // slow report (say, 5s refresh)
      // at low speed we wait longer to get sufficient pulses
      status_pub();
      last_report = now;
    }

    if (stats_interval_sec &&
	(now >= (last_stats + (stats_interval_sec*1000)))) {
      stats_pub();
      last_stats = now;
    }
  }


};

void ARDUINO_ISR_ATTR rpminputEdgeISR(void *arg) {
  ((RPMInputLeaf *)arg)->pulse();
}

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
