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
  bool persist=false;
  Ticker actuatorOffTimer;

  static const bool PERSIST_OFF=false;
  static const bool PERSIST_ON=true;
  ActuatorLeaf(String name, String target, pinmask_t pins, bool persist=false, bool invert=false, int intermittent_rate_ms = 0, int intermittent_duty_percent=50) : Leaf("actuator", name, pins){
    state = false;
    this->target=target;
    this->intermittent_rate = intermittent_rate_ms;
    this->intermittent_duty = intermittent_duty_percent;
    this->persist = persist;
    this->pin_invert = invert;
  }

  virtual void setup(void) {
    LEAF_ENTER(L_NOTICE);
    
    Leaf::setup();
    enable_pins_for_output();
    clear_pins();
    install_taps(target);
    if (persist && prefsLeaf) {
      state = prefsLeaf->getInt(leaf_name);
    }
    int actPin = -1;
    FOR_PINS({actPin=pin;});
    LEAF_NOTICE("%s claims pin %d as OUTPUT%s", base_topic.c_str(), actPin, pin_invert?" (inverted)":"");
    LEAF_LEAVE;
  }

  virtual void start(void) {
    LEAF_ENTER(L_NOTICE);
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
      if (persist && prefsLeaf) {
	prefsLeaf->putInt(leaf_name, state?1:0);
      }
      status_pub();
    }
  }

  virtual bool mqtt_receive(String type, String name, String topic, String payload) {
    LEAF_ENTER(L_DEBUG);
    bool handled = Leaf::mqtt_receive(type, name, topic, payload);
    bool state = false;
    if (payload == "on") state=true;
    else if (payload == "true") state=true;
    else if (payload == "high") state=true;
    else if (payload == "1") state=true;

    LEAF_NOTICE("RECV %s/%s => [%s <= %s]", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());

    WHEN("set/actuator",{
      LEAF_INFO("Updating actuator via set operation");
      setActuator(state);
    })
    WHEN("cmd/toggle",{
      LEAF_INFO("Updating actuator via toggle operation");
      setActuator(!state);
    })
    WHEN("cmd/off",{
      LEAF_INFO("Updating actuator via off operation");
      setActuator(false);
    })
    WHEN("cmd/on",{
      LEAF_INFO("Updating actuator via on operation");
      setActuator(true);
    })
    WHEN("cmd/oneshot",{
      LEAF_INFO("Updating actuator for one-shot operation");
      setActuator(true);
      actuatorOneshotContext = this;
      actuatorOffTimer.once_ms(payload.toInt(), [](){
	if (actuatorOneshotContext) {
	  actuatorOneshotContext->setActuator(false);
	  actuatorOneshotContext = NULL;
	}
      });
    })
    ELSEWHEN("set/intermittent/rate",{
      LEAF_INFO("Updating intermittent rate via set operation");
      intermittent_rate = payload.toInt();
      status_pub();
    })
    ELSEWHEN("set/intermittent/duty",{
      LEAF_INFO("Updating intermittent rate via set operation");
      mqtt_publish("status/intermittent/duty", String(intermittent_duty, DEC), true);
      status_pub();
    })
    ELSEWHEN("status/actuator",{
      // This is normally irrelevant, except at startup where we
      // recover any previously retained status of the actuator.
      if (state != this->state) {
	LEAF_INFO("Restoring previously retained actuator status");
	setActuator(state);
      }
    })
    ELSEWHEN("status/intermittent/rate",{
      // This is normally irrelevant, except at startup where we
      // recover any previously retained status of the actuator.
      int value = payload.toInt();
      if (value != intermittent_rate) {
	LEAF_INFO("Restoring previously retained intermittent interval (%dms)", value);
	intermittent_rate = value;
      }
    })
    ELSEWHEN("status/intermittent/duty",{
      // This is normally irrelevant, except at startup where we
      // recover any previously retained status of the actuator.
      int value = payload.toInt();
      if (value != intermittent_duty) {
	LEAF_INFO("Restoring previously retained intermittent duty cycle (%d%%)", value);
	intermittent_duty = value;
      }
    })
    else {
      if ((type == "app") || (type=="shell")) {
	LEAF_ALERT("Did not handle %s", topic.c_str());
      }
    }

    LEAF_LEAVE;
    return handled;
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
