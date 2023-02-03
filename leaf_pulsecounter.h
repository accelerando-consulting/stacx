//
//@**************************** class PulseCounterLeaf ******************************
//
// This class encapsulates an interrupt driven pulse counter
//

void ARDUINO_ISR_ATTR counterEdgeISR(void *arg);
void ARDUINO_ISR_ATTR counterRiseISR(void *arg);
void ARDUINO_ISR_ATTR counterFallISR(void *arg);
void IRAM_ATTR onPulseCounterTimer();

Leaf *pulseCounterTimerContext = NULL;

// analog pulse-counting must join in whatever mutex discipline the analog leaves use
#include "leaf_analog.h"

const char *pulseCounter_mode_names[]={"DISABLED","RISING","FALLING","CHANGE","ONLOW","ONHIGH",NULL};


class PulseCounterLeaf : public Leaf
{
protected:

  bool publish_stats = false;
  unsigned long lastCountTime = 0;
  unsigned long lastCount=0;

  unsigned long last_calc = 0;
  unsigned long last_calc_count = 0;



  unsigned long interrupts = 0;
  unsigned long noises = 0;
  unsigned long misses = 0;
  unsigned long bounces = 0;
  unsigned long lastEdgeMicro = 0;
  unsigned long prevEdgeMicro = 0;
  unsigned long lastFallMicro = 0;
  unsigned long lastRiseMicro = 0;
  unsigned long pulseWidthSum = 0;
  unsigned long pulseIntervalSum = 0;
  unsigned long noiseWidthSum = 0;
  unsigned long noiseIntervalSum = 0;
  unsigned long bounceIntervalSum = 0;
  bool level=LOW;
  bool attached=false;


  char msgBuf[4096];
  int msgBufLen=0;
  int msgBufOverflow = 0;

public:

  int counterPin = -1;
  String mode_str="CHANGE";
  int mode = CHANGE;
  bool pullup = false;
  unsigned long rises=0;
  unsigned long falls=0;
  unsigned long lastrises=0;
  unsigned long lastfalls=0;

  unsigned long count=0;

  int analog_resolution = 12;
  //  Attenuation       Measurable input voltage range
  //  ADC_ATTEN_DB_0    100 mV ~ 950 mV
  //  ADC_ATTEN_DB_2_5  100 mV ~ 1250 mV
  //  ADC_ATTEN_DB_6    150 mV ~ 1750 mV
  //  ADC_ATTEN_DB_11   150 mV ~ 2450 mV
  int analog_attenuation = 3; //ADC_ATTEN_DB_11
  int analog_level_upper = 2048+20;
  int analog_level_lower = 2048-20;
  bool analog_state = LOW;
  hw_timer_t * analog_timer = NULL;

  // hysteresis is implemented as:
  //   when analog_state==LOW, level must rise above upper
  //   when analog_state==HIGH level must fall below lower


  int rate_interval_ms=10000;
  int noise_interval_us=5;
  int debounce_interval_ms=10;

  PulseCounterLeaf(String name, pinmask_t pins, int mode=CHANGE, bool pullup=false)
    : Leaf("pulsecounter", name, pins)
    , Debuggable(name)
  {
    LEAF_ENTER(L_DEBUG);
    this->mode = mode;
    this->pullup = pullup;
    this->do_heartbeat = false;
    LEAF_LEAVE;
  }

  void setAnalogResolution(int r) { analog_resolution=r;
#ifdef ESP32
    analogReadResolution(r);
#endif
  }
  void setAnalogAttenuation(int a) {
    analog_attenuation=a;
#ifdef ESP32
    analogSetAttenuation((adc_attenuation_t)a);
#endif
  }

  void bumpInterrupts()
  {
    interrupts++;
  }

  void reset(bool reset_isr=false) {
    bool reattach = false;

    if (reset_isr && attached) {
      reattach=true;
      detach();
    }

    interrupts=count=lastCount=lastCountTime=bounces=0;
    noises=misses=lastFallMicro=lastRiseMicro=0;
    lastEdgeMicro=prevEdgeMicro=0;
    pulseWidthSum=noiseWidthSum=pulseIntervalSum=noiseIntervalSum=bounceIntervalSum=0;
    msgBufLen = 0;
    msgBuf[0]='\0';

    if (reattach) {
      attach();
    }
  }

  void attach()
  {
    LEAF_ENTER(L_INFO);
    pinMode(counterPin, pullup?INPUT_PULLUP:INPUT);
    if (mode == RISING) {
      attachInterruptArg(counterPin, counterRiseISR, this, RISING);
    }
    else if (mode == FALLING) {
      attachInterruptArg(counterPin, counterFallISR, this, FALLING);
    }
    else if ((mode == ONHIGH) || (mode==ONLOW)) {
      // use analog voodoo
      setAnalogResolution(analog_resolution);
      setAnalogAttenuation((adc_attenuation_t)analog_attenuation);
      adcAttachPin(counterPin);

      analog_timer = timerBegin(0, 80, true);
      timerAttachInterrupt(analog_timer, &onPulseCounterTimer, true);
      timerAlarmWrite(analog_timer, 1000, true);
      timerAlarmEnable(analog_timer);
      pulseCounterTimerContext = this;
    }
    else {
      attachInterruptArg(counterPin, counterEdgeISR, this, mode);
    }
    attached=true;
    LEAF_LEAVE;
  }

  void detach()
  {
    LEAF_ENTER(L_INFO);
    switch (mode) {
    case ONHIGH:
    case ONLOW:
      if (analog_timer) {
	timerEnd(analog_timer);
	analog_timer=NULL;
      }
      break;
    default:
      detachInterrupt(counterPin);
    }

    attached=false;
    pinMode(counterPin, pullup?INPUT_PULLUP:INPUT);
    LEAF_LEAVE;
  }

  virtual void stop()
  {
    LEAF_ENTER(L_INFO);
    if (attached) detach();
    LEAF_LEAVE;
  }

  virtual void setup(void) {
    static int mode_name_max = ONHIGH;

    LEAF_ENTER(L_INFO);
    Leaf::setup();

    String prefix = leaf_name+"_";
    msgBuf[0]='\0';

    registerCommand(HERE,"pulse","simulate a counter pulse");
    registerCommand(HERE,"reset","reset counter status");
    registerCommand(HERE,"stats","publish statistics");
    registerCommand(HERE,"test","poll in a tight loop for pulses (payload=seconds)");

    //
    // Load preferences and/or set defaults
    //
    registerLeafStrValue("mode", &mode_str, "Interrupt trigger mode (DISABLED,RISING,FALLING,CHANGE,ONLOW,ONHIGH)");

    registerLeafIntValue("analog_level_upper", &analog_level_upper);
    registerLeafIntValue("analog_level_lower", &analog_level_lower);
    registerLeafIntValue("db_ms", &debounce_interval_ms,"Threshold (milliseconds) for debounce");
    registerLeafIntValue("noise_us", &noise_interval_us, "Threshold (microseconds) for low-pass noise filter");
    registerLeafIntValue("report", &rate_interval_ms, "Report rate (milliseconds)");

    registerLeafBoolValue("publish_stats", &publish_stats, "Publish periodic statistics");
    reset(false);

    FOR_PINS({counterPin=pin;});
    LEAF_NOTICE("%s claims pin %d as INPUT (mode=%s, report=%lums noise=%luus debounce=%lums)", describe().c_str(),
		counterPin, (mode<=mode_name_max)?pulseCounter_mode_names[mode]:"invalid", rate_interval_ms, noise_interval_us,debounce_interval_ms);

    attach();

    LEAF_LEAVE;
  }

  virtual bool valueChangeHandler(String topic, Value *v) {
    LEAF_HANDLER(L_NOTICE);


    WHEN("mode", {
      if (mode_str.length()) {
	for (int i=0;pulseCounter_mode_names[i]; i++) {
	  if (strcasecmp(pulseCounter_mode_names[i],mode_str.c_str())==0) {
	    mode = i;
	    LEAF_NOTICE("Mode override %s", pulseCounter_mode_names[i]);
	    break;
	  }
	}
      }
      })
    else handled = Leaf::valueChangeHandler(topic, v);

    LEAF_HANDLER_END;
  }


  virtual bool sample(void)
  {
    LEAF_ENTER(L_DEBUG);
    bool result=false;

    if (mode==ONHIGH || mode==ONLOW) {
      if (!analogHoldMutex(HERE)) return false;
      int new_raw = analogRead(counterPin);
      int new_raw_mv = analogReadMilliVolts(counterPin);
      analogReleaseMutex(HERE);
      LEAF_NOTICE("Analog read on pin %d (res=%d att=%d) <= %d (%dmV)", counterPin, analog_resolution, analog_attenuation, new_raw, new_raw_mv);
      if ((analog_state == LOW) && (new_raw >= analog_level_upper)) {
	LEAF_NOTICE("analog level went HIGH");
	analog_state = HIGH;
	result=true;
      }
      else if ((analog_state == HIGH) && (new_raw <= analog_level_lower)) {
	LEAF_NOTICE("analog level went LOW");
	analog_state = LOW;
	result=true;
      }
    }
    else {
      bool new_level = digitalRead(counterPin);
      if (new_level != level) {
	pulse(new_level?1:0);
	result=true;
      }
    }

    LEAF_BOOL_RETURN(result);
  }

  void storeMsg(int level, const char *fmt, ...)
  {
    if (level > debug_level) return;

    char buf[128];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    if (n < (sizeof(msgBuf)-msgBufLen)) {
      memcpy(msgBuf+msgBufLen, buf, n+1);
      msgBufLen += n;
    }
    else {
      ++msgBufOverflow;
    }
  }

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


  void pulse(int inject_level=-1) {
    unsigned long unow = micros();
    unsigned long now = millis();
    unsigned long pulse_width_us, pulse_interval_us;
    bool newLevel;

    ++interrupts;
    pulse_width_us = unow - lastEdgeMicro;
    storeMsg(L_INFO, "%luus EDGE width=%luus\n", unow, pulse_width_us);

    if (lastEdgeMicro == 0) {
      // this is our first edge, we cannot make any decisions yet
      storeMsg(L_INFO, "  WARN f1rst edge at unow=%lu!\n", unow);
      lastEdgeMicro = prevEdgeMicro = unow;
      return;
    }

    if (noise_interval_us && (pulse_width_us <= noise_interval_us)) {
      // This pulse was too short, ignore it, and edit the pulse out of the
      // edge history, reverting to our previous notion of the last
      // significant edge

      ++noises;
      if (inject_level != -1) {
	LEAF_NOTICE("Injected a level value of %d", inject_level);
	newLevel = inject_level;
      }
      else {
	newLevel = digitalRead(counterPin);
      }
      pulse_interval_us = lastEdgeMicro - prevEdgeMicro;
      lastEdgeMicro=prevEdgeMicro;
      //prevEdgeMicro = 0;

      storeMsg(L_INFO, "  ALERT %s noise pulse width=%luus interval=%luus\n", newLevel?"negative":"positive", pulse_width_us, pulse_interval_us);
      noiseWidthSum += pulse_width_us;
      noiseIntervalSum += pulse_interval_us;
      level = newLevel;
      return;
    }

    // We've just seen a level change that passed through the noise filter,
    // was it rising or falling?
    newLevel = digitalRead(counterPin);

    // record a (potentially) significant pulse
    pulse_interval_us = unow - prevEdgeMicro;
    prevEdgeMicro = lastEdgeMicro;
    lastEdgeMicro = unow;

    if (newLevel == level) {
      // pulse was so short we missed its other edge
      ++misses;

      if (level == LOW) {
	storeMsg(L_INFO, "  double low (missed high edge?) fall=%lu rise=%lu\n", unow-lastFallMicro, unow-lastRiseMicro);
	lastFallMicro = unow;
      }
      else {
	storeMsg(L_INFO, "  double high (missed low edge?) fall=%lu rise=%lu\n", unow-lastFallMicro, unow-lastRiseMicro);
	lastRiseMicro = unow;
      }
      return;
    }
    level = newLevel;

    // OK, we have an actionable edge (ignoring short or half-seen pulses)
    storeMsg(L_INFO, "%luus %cEDGE width=%luus interval=%luus: ", unow, level?'+':'-', pulse_width_us, pulse_interval_us);

    if (level == LOW) {
      // Input fell.  This might be a pulse.  Or it might be the start of a a noise burst
      //
      // Ignore (debounce) successive negative  edges that are too close together
      //
      // Do not necesarily count this as a pulse (yet), we will ignore pulses that are too short (below noise_interval_us)
      //
      pulse_interval_us = unow - lastFallMicro;
      storeMsg(L_INFO, "  fall interval=%luus\n", pulse_interval_us);
      if ((pulse_interval_us * 1000) <= debounce_interval_ms) {
	// suppress this edge
	storeMsg(L_ALERT, "  ALERT bounce interval=%luus\n", pulse_interval_us);
	bounceIntervalSum += pulse_interval_us;
	++bounces;
	return;
      }

      // Record the beginning of a "real" negative pulse
      lastFallMicro = unow;
      storeMsg(L_INFO, "\n");
      return;
    }

    //
    // Input rose, consider measure the duration of the negative pulse
    //
    pulse_width_us = unow - lastFallMicro;
    pulse_interval_us = lastFallMicro - lastRiseMicro;
    lastRiseMicro = unow;
    //storeMsg("  INFO rise width=%luus, interval=%luus\n", pulse_width_us, pulse_interval_us);

    // We are interested in negative pulses of around 300uS, skip any that
    // are ludicrously long (or where we have no initial edge recorded)
    //
    if (lastFallMicro == 0) {
      storeMsg(L_INFO, "  f1rst rise!\n");
      return;
    }
    if (pulse_width_us > 1000) {
      // storeMsg("  ALERT TOO LONG (%luus)\n", pulse_width_us);
      storeMsg(L_INFO, "\n");
      return;
    }

    // This pulse is in the range to count as a "real" event, so count it
    count++;
    pulseWidthSum += pulse_width_us;
    pulseIntervalSum += pulse_interval_us;

    storeMsg(L_INFO, "  NOTICE pulse lastFallMicro=%lu lastRiseMicro=%lu width=%luus interval=%luus\n",
	     lastFallMicro, lastRiseMicro, pulse_width_us, pulse_interval_us);
    lastCountTime = now;
  }


  virtual void status_pub()
  {
    //LEAF_NOTICE("count=%lu", count);
    publish("status/count", String(count));
  }

  void mqtt_do_subscribe()
  {
    Leaf::mqtt_do_subscribe();
    //register_mqtt_value("","");
  }

  void calc_stats(bool always=false)
  {
    LEAF_ENTER(L_DEBUG);
    unsigned long now = millis();
    int elapsed = now - last_calc;
    int delta = count - last_calc_count;
    if (delta > 0) {
      LEAF_NOTICE("%d counts in %dms (%.1f c/min, ~%.3f Hz)", delta, elapsed, delta * 60000.0 / elapsed, delta * 1000.0 / elapsed);
    }

    LEAF_INFO("    interrupts=%lu misses=%lu noises=%lu bounces=%lu counts=%lu", interrupts, misses, noises, bounces, count);
    if (noises) {
      LEAF_INFO("    average noise width=%.1fus sep=%.1fus (n=%lu)", (float)noiseWidthSum/noises, (float)noiseIntervalSum/noises, noises);
    }
    if (bounces) {
      LEAF_INFO("    average bounce interval=%.1fus (n=%lu)", (float)bounceIntervalSum/bounces, bounces);
    }
    if (delta > 0) {
      LEAF_INFO("    average pulse width=%.1fus sep=%.1fus", (float)pulseWidthSum/count, (float)pulseIntervalSum/count);
      last_calc_count = count;
    }
    last_calc = now;
    // TODO: calculate pulse rate at various CIs (currently must be done by consumer module
    if (publish_stats) {
      DynamicJsonDocument doc(512);
      JsonObject obj = doc.to<JsonObject>();
      bool useful = (count>0);
      obj["count"]=count;
      if (interrupts) {
	obj["interrupts"]=interrupts;
	useful=true;
      }
      if (misses) {
	obj["misses"]=misses;
	useful=true;
      }
      if (noises) {
	obj["noises"]=noises;
	useful=true;
      }

      if (bounces) {
	obj["bounces"]=bounces;
	useful=true;
      }

      if (noises) {
	obj["av_noise_width"]=String((float)noiseWidthSum/noises);
	useful=true;
      }

      if (noises) {
	obj["av_noise_sep"]=String((float)noiseIntervalSum/noises);
	useful=true;
      }

      if (bounces) {
	obj["av_bounce_interval"]=String((float)bounceIntervalSum/bounces);
	useful=true;
      }

      if (count) {
	obj["av_pulse_width"]=String((float)pulseWidthSum/count);
	useful=true;
      }

      if (count) {
	obj["av_pulse_sep"]=String((float)pulseIntervalSum/count);
	useful=true;
      }

      if (always || useful) {
	char msg[512];
	serializeJson(doc, msg, sizeof(msg)-2);
	mqtt_publish(String("stats/")+leaf_name, msg);
      }
    }
    LEAF_LEAVE;
  }

  virtual void do_stats()
  {
    calc_stats(true);
  }

  virtual bool mqtt_receive(String type, String name, String topic, String payload, bool direct=false) {
    LEAF_ENTER(L_INFO);
    bool handled = false;

    if ((type == "app")|| (type=="shell")) {
      LEAF_NOTICE("RECV %s/%s => [%s <= %s]", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());
    }

    WHEN("cmd/pulse", {
	LEAF_NOTICE("Simulated pulse");
	pulse(payload.toInt());
    })
    WHEN("cmd/sample", {
	LEAF_NOTICE("Manually triggered sample");
	sample();
    })
    ELSEWHEN("cmd/count", {
	LEAF_NOTICE("Simulated count");
	count++;
	pulseWidthSum += 10;
	pulseIntervalSum += 100000;
    })
    ELSEWHEN("cmd/test", {
	pulse_test(payload.toInt());
    })
    ELSEWHEN("cmd/reset",{
	LEAF_NOTICE("Reset requested");
	reset(true);
      })
    else {
      handled = Leaf::mqtt_receive(type, name, topic, payload, direct);
    }

    LEAF_RETURN(handled);
  }

  virtual void loop(void) {
    Leaf::loop();

    unsigned long now = millis();
    if (now > (last_calc + rate_interval_ms)) {
      calc_stats();
    }
    // not an else case
    if (count > lastCount) {
      LEAF_INFO("count change %lu => %lu falls=%lu rises=%lu", lastCount, count, falls, rises);
      lastCount=count;
      status_pub();
    }

#if 0
    if ((rises != lastrises) || (falls!=lastfalls)) {
      LEAF_NOTICE("%lu rises %lu falls");
      lastrises=rises;
      lastfalls=falls;
    }
#endif

    if (msgBufLen > 0) {
      LEAF_NOTICE("Delayed messages of size %d", msgBufLen);
      DBGPRINT(msgBuf);
      msgBufLen = 0;
      if (msgBufOverflow) {
	DBGPRINTF("  ALERT %d messages lost\n", (int)msgBufOverflow);
	msgBufOverflow=0;
      }
    }
  }

};

void ARDUINO_ISR_ATTR counterEdgeISR(void *arg) {
  PulseCounterLeaf *leaf = (PulseCounterLeaf *)arg;
  leaf->pulse();
}

void ARDUINO_ISR_ATTR counterRiseISR(void *arg) {
  PulseCounterLeaf *leaf = (PulseCounterLeaf *)arg;
  leaf->rises++;
  leaf->count++;
}

void ARDUINO_ISR_ATTR counterFallISR(void *arg) {
  PulseCounterLeaf *leaf = (PulseCounterLeaf *)arg;
  leaf->count++;
  leaf->falls++;
}

void IRAM_ATTR onPulseCounterTimer()
{
  PulseCounterLeaf *leaf = (PulseCounterLeaf *)pulseCounterTimerContext;

  int new_raw = analogRead(leaf->counterPin);
  leaf->bumpInterrupts();

  if ((leaf->analog_state == LOW) && (new_raw >= leaf->analog_level_upper)) {
    leaf->count++;
    leaf->rises++;
  }
  else if ((leaf->analog_state == HIGH) && (new_raw <= leaf->analog_level_lower)) {
    leaf->count++;
    leaf->falls++;
  }
}

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
