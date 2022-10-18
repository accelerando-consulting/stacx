//
//@**************************** class PulseCounterLeaf ******************************
//
// This class encapsulates an interrupt driven pulse counter
//

void ARDUINO_ISR_ATTR counterISR(void *arg);


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

  char msgBuf[4096];
  int msgBufLen=0;
  int msgBufOverflow = 0;
  
public:
  int counterPin = -1;
  int mode = CHANGE;
  bool pullup = false;

  unsigned long count=0;

  // see setup() for defaults
  int rate_interval_ms;
  int noise_interval_us;
  int debounce_interval_ms;
  
  PulseCounterLeaf(String name, pinmask_t pins, int mode=FALLING, bool pullup=false) : Leaf("pulsecounter", name, pins) {
    LEAF_ENTER(L_DEBUG);
    this->mode = mode;
    this->pullup = pullup;
    this->do_heartbeat = false;
    LEAF_LEAVE;
  }

  void reset(void) {
    interrupts=count=lastCount=lastCountTime=bounces=0;
    noises=misses=lastFallMicro=lastRiseMicro=0;
    lastEdgeMicro=prevEdgeMicro=0;
    pulseWidthSum=noiseWidthSum=pulseIntervalSum=noiseIntervalSum=bounceIntervalSum=0;
    msgBufLen = 0;
    msgBuf[0]='\0';
  }

  virtual void setup(void) {
    static const char *mode_names[]={"DISABLED","RISING","FALLING","CHANGE","ONLOW","ONHIGH"};
    static int mode_name_max = ONHIGH;
      
    LEAF_ENTER(L_INFO);
    Leaf::setup();

    String prefix = leaf_name+"_";
    msgBuf[0]='\0';
    
    // 
    // Load preferences and/or set defaults
    //
    rate_interval_ms = getPref(prefix+"report", "10000", "Report rate (milliseconds)").toInt();
    noise_interval_us = getPref(prefix+"noise_us", "5", "Threshold (milliseconds) for low-pass noise filter").toInt();
    debounce_interval_ms = getPref(prefix+"db_ms", "10", "Threshold (milliseconds) for debounce").toInt();
    getBoolPref(prefix+"publish_stats", &publish_stats, "Publish periodic statistics");
    reset();

    FOR_PINS({counterPin=pin;});
    LEAF_NOTICE("%s claims pin %d as INPUT (mode=%s, report=%lums noise=%luus debounce=%lums)", base_topic.c_str(),
		counterPin, (mode<=mode_name_max)?mode_names[mode]:"invalid", rate_interval_ms, noise_interval_us,debounce_interval_ms);

    pinMode(counterPin, pullup?INPUT_PULLUP:INPUT);
    attachInterruptArg(counterPin, counterISR, this, mode);

    LEAF_LEAVE;
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
  
  void pulse(void) {
    unsigned long unow = micros();
    unsigned long now = millis();
    unsigned long pulse_width_us, pulse_interval_us;
    bool newLevel;
    
    ++interrupts;
    pulse_width_us = unow - lastEdgeMicro;
    storeMsg(L_TRACE, "%luus EDGE width=%luus\n", unow, pulse_width_us);
    
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
      newLevel = digitalRead(counterPin);
      pulse_interval_us = lastEdgeMicro - prevEdgeMicro;
      lastEdgeMicro=prevEdgeMicro; 
      //prevEdgeMicro = 0;
      
      storeMsg(L_DEBUG, "  ALERT %s noise pulse width=%luus interval=%luus\n", newLevel?"negative":"positive", pulse_width_us, pulse_interval_us);
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
	storeMsg(L_DEBUG, "  double low (missed high edge?) fall=%lu rise=%lu\n", unow-lastFallMicro, unow-lastRiseMicro);
	lastFallMicro = unow;
      }
      else {
	storeMsg(L_TRACE, "  double high (missed low edge?) fall=%lu rise=%lu\n", unow-lastFallMicro, unow-lastRiseMicro);
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
      storeMsg(L_TRACE, "  fall interval=%luus\n", pulse_interval_us);
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
    register_mqtt_cmd("pulse","simulate a counter pulse");
    register_mqtt_cmd("reset","reset counter status");
    register_mqtt_cmd("stats","publish statistics");
    //register_mqtt_value("","");
  }

  void calc_stats() 
  {
    LEAF_ENTER(L_INFO);
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
      obj["count"]=count;
      if (interrupts) obj["interrupts"]=interrupts;
      if (misses) obj["misses"]=misses;
      if (noises) obj["noises"]=noises;
      if (bounces) obj["bounces"]=bounces;
      if (noises) obj["av_noise_width"]=String((float)noiseWidthSum/noises);
      if (noises) obj["av_noise_sep"]=String((float)noiseIntervalSum/noises);
      if (bounces) obj["av_bounce_interval"]=String((float)bounceIntervalSum/bounces);
      if (count) obj["av_pulse_width"]=String((float)pulseWidthSum/count);
      if (count) obj["av_pulse_sep"]=String((float)pulseIntervalSum/count);
      char msg[512];
      serializeJson(doc, msg, sizeof(msg)-2);
      mqtt_publish(String("stats/")+leaf_name, msg);
    }
    LEAF_LEAVE;
  }

  virtual bool mqtt_receive(String type, String name, String topic, String payload) {
    LEAF_ENTER(L_INFO);
    bool handled = false;
    
    if (type == "app") {
      LEAF_NOTICE("RECV %s/%s => [%s <= %s]", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());
    }
    
    WHEN("cmd/pulse", {
	LEAF_NOTICE("Simulated pulse");
	pulse();
    })
    ELSEWHEN("cmd/stats", {
	calc_stats();
    })
    ELSEWHEN("cmd/reset",{
	LEAF_NOTICE("Reset requested");
	reset();
      })
    else {
      handled |= Leaf::mqtt_receive(type, name, topic, payload);
    }
      
    LEAF_RETURN(handled);
  }

  virtual void loop(void) {
    Leaf::loop();
    unsigned long now = millis();

    if (msgBufLen > 0) {
      LEAF_NOTICE("Delayed messages of size %d", msgBufLen);
      Serial.print(msgBuf);
      msgBufLen = 0;
      if (msgBufOverflow) {
	Serial.printf("  ALERT %d messages lost\n", (int)msgBufOverflow);
	msgBufOverflow=0;
      }

    }

    if (now > (last_calc + rate_interval_ms)) {
      calc_stats();
    }

    // not an else case
    if (count > lastCount) {
      LEAF_INFO("count change %lu => %lu", lastCount, count);
      lastCount=count;
      status_pub();
    }

  }

};

void ARDUINO_ISR_ATTR counterISR(void *arg) {
  PulseCounterLeaf *leaf = (PulseCounterLeaf *)arg;
  leaf->pulse();
}

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
