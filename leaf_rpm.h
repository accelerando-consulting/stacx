//
//@************************ class RPMInputLeaf ******************************
//
// This class encapsulates an interrupt driven RPM signal measurement
//

class RPMInputLeaf : public Leaf
{
protected:
  int pwmInputPin = -1;
  bool pullup = false;

  unsigned long interrupts = 0;
  unsigned long positive_pulse_width = 0;
  unsigned long negative_pulse_width = 0;
  unsigned long pulse_interval = 0;
  unsigned long lastEdgeMicro = 0;
  unsigned long prevEdgeMicro = 0;
  unsigned long report_interval_msec = 5000;
  unsigned long stats_interval_sec = 60;
  unsigned long last_report = 0;
  unsigned long last_stats = 0;
  
  bool attached=false;

public:

  PWMInputLeaf(String name, pinmask_t pins, bool pullup=false)
    : Leaf("pwminput", name, pins)
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

    interrupts=0;
    lastEdgeMicro=prevEdgeMicro=0;
    positive_pulse_width=negative_pulse_width=pulse_interval=0;

    if (reattach) {
      attach();
    }
  }

  void attach()
  {
    LEAF_ENTER(L_INFO);
    pinMode(pwmInputPin, pullup?INPUT_PULLUP:INPUT);
    attachInterruptArg(pwmInputPin, pwminputEdgeISR, this, CHANGE);
    attached=true;
    LEAF_LEAVE;
  }

  void detach()
  {
    LEAF_ENTER(L_INFO);
    detachInterrupt(pwmInputPin);
    attached=false;
    pinMode(pwmInputPin, pullup?INPUT_PULLUP:INPUT);
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
    registerLeafIntValue("stats_interval_sec", &stats_interval_sec, "Report rate for statistics (seconds)");
    reset(false);

    FOR_PINS({pwmInputPin=pin;});
    LEAF_NOTICE("%s claims pin %d as INPUT", describe().c_str(), pwmInputPin);
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
    bool newLevel = digitalRead(pwmInputPin);

    ++interrupts;
    if (lastEdgeMicro == 0) {
      // this is our first edge, we cannot make any decisions yet
      lastEdgeMicro = unow;
      return;
    }

    unsigned long pulse_width_us = unow - lastEdgeMicro;
    if (prevEdgeMicro > 0) {
      pulse_interval = unow - prevEdgeMicro;
    }

    // record a (potentially) significant pulse
    prevEdgeMicro = lastEdgeMicro;
    lastEdgeMicro = unow;

    if (newLevel == HIGH) {
      negative_pulse_width=pulse_width_us;
    }
    else {
      positive_pulse_width=pulse_width_us;
    }
  }

  virtual void status_pub()
  {
    LEAF_ENTER(L_INFO);
    publish("status/pulsewidth", String(positive_pulse_width)+","+String(negative_pulse_width));
    LEAF_LEAVE;
  }

  virtual void stats_pub()
  {
    LEAF_ENTER(L_INFO);
    publish("stats/interrupts", String(interrupts),L_NOTICE,HERE);
    publish("stats/positive_pulse_width", String(positive_pulse_width),L_NOTICE,HERE);
    publish("stats/negative_pulse_width", String(negative_pulse_width),L_NOTICE,HERE);
    publish("stats/pulse_interval", String(pulse_interval),L_NOTICE,HERE);
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
	(now >= (last_report + report_interval_msec))) {
      last_report = now;
      status_pub();
    }

    if (stats_interval_sec &&
	(now >= (last_stats + (stats_interval_sec*1000)))) {
      last_stats = now;
      stats_pub();
    }
  }
  

};

void ARDUINO_ISR_ATTR pwminputEdgeISR(void *arg) {
  ((PWMInputLeaf *)arg)->pulse();
}

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
