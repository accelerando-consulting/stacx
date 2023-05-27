//@***************************** class ActuatorLeaf ******************************

class ActuatorLeaf;

class ActuatorLeaf *actuatorOneshotContext;

class ActuatorLeaf : public Leaf
{
public:
  String target;
  bool state=false;
  int intermittent_rate;
  int intermittent_duty;
#if USE_PREFS
  bool persist=false;
#endif
  Ticker actuatorOffTimer;

  static const bool PERSIST_OFF=false;
  static const bool PERSIST_ON=true;
  ActuatorLeaf(String name, String target, pinmask_t pins, bool persist=false, bool invert=false, int intermittent_rate_ms = 0, int intermittent_duty_percent=50)
    : Leaf("actuator", name, pins)
    , Debuggable(name)
  {
    state = false;
    this->target=target;
    this->intermittent_rate = intermittent_rate_ms;
    this->intermittent_duty = intermittent_duty_percent;
#if USE_PREFS
    this->persist = persist;
#endif
    this->pin_invert = invert;
  }

  virtual void setup(void) {
    LEAF_ENTER(L_INFO);
    
    Leaf::setup();
    enable_pins_for_output();
    clear_pins();
    install_taps(target);
#if USE_PREFS
    if (persist && prefsLeaf) {
      state = prefsLeaf->getInt(leaf_name);
    }
#endif
    int actPin = -1;
    FOR_PINS({actPin=pin;});
    LEAF_NOTICE("%s claims pin %d as OUTPUT%s", describe().c_str(), actPin, pin_invert?" (inverted)":"");

    registerLeafValue(HERE, "pin_invert", VALUE_KIND_BOOL, &pin_invert, "Invert the sense of the actuator pin");
    LEAF_LEAVE;
  }

  virtual void start(void) {
    LEAF_ENTER(L_INFO);
    Leaf::start();
    setActuator(state);
    LEAF_LEAVE;
  }

  virtual void mqtt_do_subscribe() {
    LEAF_ENTER(L_DEBUG);
    Leaf::mqtt_do_subscribe();
    mqtt_subscribe("set/actuator", HERE);
    mqtt_subscribe("status/actuator", HERE);
    LEAF_LEAVE;
  }

  virtual void status_pub()
  {
    if (intermittent_rate) {
      mqtt_publish("status/actuator", "intermittent");
      mqtt_publish("status/intermittent/rate", String(intermittent_rate, DEC));
      mqtt_publish("status/intermittent/duty", String(intermittent_duty, DEC));
    }
    else {
      mqtt_publish("status/actuator", String(state?"on":"off"));
    }
  }

  void setActuator(bool state) {
    const char *desc = STATE(state);
    LEAF_INFO("Set actuator relay to %s", desc);
    if (state) {
      set_pins();
    } else {
      clear_pins();
    }
    if (this->state != state) {
      this->state = state;
#if USE_PREFS
      if (persist && prefsLeaf) {
	prefsLeaf->putInt(leaf_name, state?1:0);
      }
#endif
      status_pub();
    }
  }

  virtual bool mqtt_receive(String type, String name, String topic, String payload, bool direct=false) {
    LEAF_ENTER(L_DEBUG);
    bool handled = false;
    bool state = parseBool(payload, false);

    LEAF_INFO("RECV %s/%s => [%s <= %s]", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());

    WHEN("set/actuator",{
	//LEAF_INFO("Updating actuator via set operation");
      setActuator(state);
    })
    ELSEWHEN("cmd/toggle",{
	//LEAF_INFO("Updating actuator via toggle operation");
      setActuator(!state);
    })
    ELSEWHEN("cmd/off",{
	//LEAF_INFO("Updating actuator via off operation");
      setActuator(false);
    })
    ELSEWHEN("cmd/on",{
	//LEAF_INFO("Updating actuator via on operation");
      setActuator(true);
    })
    ELSEWHEN("cmd/oneshot",{
      int duration = payload.toInt();
      //LEAF_INFO("Triggering actuator for one-shot operation (%dms)", duration);
      setActuator(true);
      actuatorOneshotContext = this;
      actuatorOffTimer.once_ms(duration, [](){
	if (actuatorOneshotContext) {
	  actuatorOneshotContext->setActuator(false);
	  actuatorOneshotContext = NULL;
	}
      });
    })
    ELSEWHEN("set/intermittent/rate",{
	//LEAF_INFO("Updating intermittent rate via set operation");
      intermittent_rate = payload.toInt();
      status_pub();
    })
    ELSEWHEN("set/intermittent/duty",{
	//LEAF_INFO("Updating intermittent rate via set operation");
      mqtt_publish("status/intermittent/duty", String(intermittent_duty, DEC), true);
      status_pub();
    })
    ELSEWHEN("status/actuator",{
      // This is normally irrelevant, except at startup where we
      // recover any previously retained status of the actuator.
      if (state != this->state) {
	//LEAF_INFO("Restoring previously retained actuator status");
	setActuator(state);
      }
    })
    ELSEWHEN("status/intermittent/rate",{
      // This is normally irrelevant, except at startup where we
      // recover any previously retained status of the actuator.
      int value = payload.toInt();
      if (value != intermittent_rate) {
	//LEAF_INFO("Restoring previously retained intermittent interval (%dms)", value);
	intermittent_rate = value;
      }
    })
    ELSEWHEN("status/intermittent/duty",{
      // This is normally irrelevant, except at startup where we
      // recover any previously retained status of the actuator.
      int value = payload.toInt();
      if (value != intermittent_duty) {
	//LEAF_INFO("Restoring previously retained intermittent duty cycle (%d%%)", value);
	intermittent_duty = value;
      }
    })
    else {
      handled = Leaf::mqtt_receive(type, name, topic, payload, direct);
    }

    if (!handled && ((type == "app") || (type=="shell")))  {
      LEAF_ALERT("Did not handle %s", topic.c_str());
    }

    LEAF_BOOL_RETURN(handled);
  };

  virtual void loop()
  {

    if (intermittent_rate > 0) {
      // Intermittent mode is enabled

      if (intermittent_duty <= 0) {
	// Intermittent duty cycle is 0%, which is the same as "off"
	if (state == HIGH) {
	  setActuator(LOW);
	}
      }
      else if (intermittent_duty >= 100) {
	// Intermittent duty cycle is 100%, which is the same as "on"
	if (state == LOW) {
	  setActuator(HIGH);
	}
      }
      else {
	// Intermittent rate is 'intermittent_rate' with intermittent_duty% ON, remainder off
	unsigned long pos = millis() % intermittent_rate;
	if (pos >= (intermittent_rate * intermittent_duty / 100)) {
	  // We are in the OFF part of the intermittent cycle
	  if (state != LOW) {
	    clear_pins();
	    state = LOW;
	  }
	}
	else {
	  // We are in the high part of the intermittent cycle
	  if (state != HIGH) {
	    set_pins();
	    state = HIGH;
	  }
	}
      }
    }


  }


};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
