//@***************************** class LightLeaf ******************************

class LightLeaf;

struct blipRestoreContext
{
  class LightLeaf *blip_leaf;
} blip_restore_context;

class LightLeaf : public Leaf
{
public:
  String target;
  bool state=false;
  int flash_rate;
  int flash_duty;
  bool persist=false;
  bool pubsub_persist=false;
  static const bool PERSIST_OFF=false;
  static const bool PERSIST_ON=true;
  String extender;
  String extender_pin;
  Ticker blipRestoreTimer;
  LightLeaf(String name, String target, pinmask_t pins, bool persist=false, bool invert=false, int flash_rate_ms = 0, int flash_duty_percent=50)
    : Leaf("light", name, pins)
    , Debuggable(name)
  {
    state = false;
    this->target=target;
    this->flash_rate = flash_rate_ms;
    this->flash_duty = flash_duty_percent;
    this->persist = persist;
    this->pin_invert = invert;
  }

  LightLeaf(String name, String target, String light_target, bool persist=false, bool invert=false, int flash_rate_ms = 0, int flash_duty_percent=50)
    : LightLeaf("light", name, NO_PINS,persist,invert,flash_rate_ms,flash_duty_percent)
      //: Leaf("light", name, pins)
      //, Debuggable(name)
  {
    this->tap_targets = target;
    int pos = light_target.indexOf(":");
    if (pos > 0) {
      extender = light_target.substring(0,pos);
      extender_pin = light_target.substring(pos+1);
    }
    else {
      extender = target;
      extender_pin = light_target;
    }
  }

  virtual void setup(void) {
    Leaf::setup();
    enable_pins_for_output();
    install_taps(target);
#if USE_PREFS
    if (persist && prefsLeaf) {
      state = prefsLeaf->getInt(leaf_name);
    }
#endif // USE_PREFS

    registerLeafBoolValue("light", &state, "set status of light output");
    registerLeafBoolValue("pubsub_persist", &pubsub_persist, "use persistent mqtt to save status");
    registerLeafIntValue("flash_rate", &flash_rate, "control flashing rate");
    registerLeafIntValue("flash_duty", &flash_duty, "control flashing duty cycle (percent)");
    registerCommand(HERE, "toggle", "flip the status of light output");
    registerCommand(HERE, "off", "turn the light off");
    registerCommand(HERE, "on", "turn the light on");
    registerCommand(HERE, "blip", "blip the light on for N milliseconds");
  }

  virtual bool valueChangeHandler(String topic, Value *v)
  {
    bool handled=false;

    WHENEITHER("state","light",setLight(VALUE_AS_BOOL(v)))
    else handled = Leaf::valueChangeHandler(topic, v);

    status_pub();
    return handled;
  }


  virtual bool commandHandler(String type, String name, String topic, String payload) {
    bool handled=false;
    bool lit = parsePayloadBool(payload);

    WHEN    ("toggle",setLight(!state))
    ELSEWHEN("off"   ,setLight(false))
    ELSEWHEN("on"    ,setLight(true))
    ELSEWHEN("blip"  ,blipLight(payload.toInt()))
    else handled = Leaf::commandHandler(type, name, topic, payload);
    
    return handled;
  }

  virtual void start(void) {
    Leaf::start();
    setLight(state);
  }

  virtual bool parsePayloadBool(String payload)
  {
    return Leaf::parsePayloadBool(payload)||(payload=="lit");
  }

  virtual void mqtt_do_subscribe() {
    LEAF_ENTER(L_DEBUG);
    Leaf::mqtt_do_subscribe();

    if (pubsub_persist) {
      // use mqtt retained publish as a persistence mechanism, fetch persistent state from broker at start
      mqtt_subscribe("status/light", HERE);
      mqtt_subscribe("status/flash/rate", HERE);
      mqtt_subscribe("status/flash/duty", HERE);
    }

    if (do_status) status_pub();

    LEAF_LEAVE;
  }

  virtual void status_pub()
  {
    if (flash_rate) {
      mqtt_publish("status/light", "flash", 0, true, L_NOTICE, HERE);
      mqtt_publish("status/flash/rate", String(flash_rate, DEC), 0, true, L_NOTICE, HERE);
      mqtt_publish("status/flash/duty", String(flash_duty, DEC), 0, true, L_NOTICE, HERE);
    }
    else {
      mqtt_publish("status/light", state?"lit":"unlit", 0, true, L_NOTICE, HERE);
    }
  }

  void setLight(bool lit) {
    const char *litness = lit?"lit":"unlit";
    LEAF_NOTICE("Set light relay to %s", litness);

    if (lit) {
      if (extender_pin.length()) {
	message(extender, "cmd/set", extender_pin);
      }
      else {
	set_pins();
      }
    } else {
      if (extender_pin.length()) {
	message(extender, "cmd/clear", extender_pin);
      }
      else {
	clear_pins();
      }
    }
    state = lit;
#if USE_PREFS
    if (persist && prefsLeaf) {
      prefsLeaf->putInt(leaf_name, state?1:0);
    }
#endif // USE_PREFS
    status_pub();
  }

  void blipStop()
  {
    clear_pins();
  }

  void blipLight(int duration)
  {
    set_pins();
    blip_restore_context.blip_leaf=this;
    blipRestoreTimer.once_ms(duration, []() {
      blip_restore_context.blip_leaf->blipStop();
    });
  }

  virtual bool mqtt_receive(String type, String name, String topic, String payload, bool direct=false) {
    LEAF_ENTER(L_DEBUG);
    bool handled = false;

    LEAF_INFO("%s [%s]", topic.c_str(), payload.c_str());

    WHEN("status/light",{
      // This is normally irrelevant, except at startup where we
      // recover any previously retained status of the light.
      bool lit = parsePayloadBool(payload);
      if (lit != state) {
	//LEAF_INFO("Restoring previously retained light status");
	setLight(lit);
      }
    })
    ELSEWHEN("status/flash/rate",{
      // This is normally irrelevant, except at startup where we
      // recover any previously retained status of the light.
      int value = payload.toInt();
      if (value != flash_rate) {
	//LEAF_INFO("Restoring previously retained flash interval (%dms)", value);
	flash_rate = value;
      }
    })
    ELSEWHEN("status/flash/duty",{
      // This is normally irrelevant, except at startup where we
      // recover any previously retained status of the light.
      int value = payload.toInt();
      if (value != flash_duty) {
	//LEAF_INFO("Restoring previously retained flash duty cycle (%d%%)", value);
	flash_duty = value;
      }
    })
    else {
      handled = Leaf::mqtt_receive(type, name, topic, payload, direct);
    }

    LEAF_LEAVE;
    return handled;
  };

  virtual void loop()
  {

    if (flash_rate > 0) {
      // Flashing is enabled

      if (flash_duty <= 0) {
	// Flash duty cycle is 0%, which is the same as "off"
	if (state == HIGH) {
	  setLight(LOW);
	}
      }
      else if (flash_duty >= 100) {
	// Flash duty cycle is 100%, which is the same as "on"
	if (state == LOW) {
	  setLight(HIGH);
	}
      }
      else {
	// Flash rate is 'flash_rate' with flash_duty% ON, remainder off
	unsigned long pos = millis() % flash_rate;
	if (pos >= (flash_rate * flash_duty / 100)) {
	  // We are in the OFF part of the flash cycle
	  if (state != LOW) {
	    clear_pins();
	    state = LOW;
	  }
	}
	else {
	  // We are in the high part of the cycle
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
