//@***************************** class LightLeaf ******************************


class LightLeaf : public Leaf
{
public:
  bool state;
  int flash_rate;
  int flash_duty;

  LightLeaf(String name, pinmask_t pins, int flash_rate_ms = 0, int flash_duty_percent=50) : Leaf("light", name, pins){
    state = false;
    flash_rate = flash_rate_ms;
    flash_duty = flash_duty_percent;
  }

  virtual void setup(void) {
    Leaf::setup();
    enable_pins_for_output();
  }

  virtual void mqtt_subscribe() {
    LEAF_ENTER(L_NOTICE);
    Leaf::mqtt_subscribe();
    _mqtt_subscribe(base_topic+"/set/light");
    _mqtt_subscribe(base_topic+"/status/light");
    LEAF_LEAVE;
  }

  virtual void status_pub()
  {
    if (flash_rate) {
      mqtt_publish("status/light", "flash", true);
      mqtt_publish("status/flash/rate", String(flash_rate, DEC), true);
      mqtt_publish("status/flash/duty", String(flash_duty, DEC), true);
    }
    else {
      mqtt_publish("status/light", state?"lit":"unlit", true);
      mqtt_publish("status/flash/rate", "", true);
      mqtt_publish("status/flash/duty", "", true);
    }
  }

  void setLight(bool lit) {
    const char *litness = lit?"lit":"unlit";
    LEAF_NOTICE("Set light relay to %s", litness);
    if (lit) {
      // The relay is active low
      clear_pins();
    } else {
      set_pins();
    }
    state = lit;
    status_pub();
  }

  virtual bool mqtt_receive(String type, String name, String topic, String payload) {
    LEAF_ENTER(L_INFO);
    bool handled = Leaf::mqtt_receive(type, name, topic, payload);
    bool lit = false;
    if (payload == "on") lit=true;
    else if (payload == "true") lit=true;
    else if (payload == "lit") lit=true;
    else if (payload == "high") lit=true;
    else if (payload == "1") lit=true;

    WHEN("set/light",{
      LEAF_INFO("Updating light via set operation");
      setLight(lit);
    })
    ELSEWHEN("set/flash/rate",{
      LEAF_INFO("Updating flash rate via set operation");
      flash_rate = payload.toInt();
      status_pub();
    })
    ELSEWHEN("set/flash/duty",{
      LEAF_INFO("Updating flash rate via set operation");
      mqtt_publish("status/flash/duty", String(flash_duty, DEC), true);
      status_pub();
    })
    ELSEWHEN("status/light",{
      // This is normally irrelevant, except at startup where we
      // recover any previously retained status of the light.
      if (lit != state) {
	LEAF_INFO("Restoring previously retained light status");
	setLight(lit);
      }
    })
    ELSEWHEN("status/flash/rate",{
      // This is normally irrelevant, except at startup where we
      // recover any previously retained status of the light.
      int value = payload.toInt();
      if (value != flash_rate) {
	LEAF_INFO("Restoring previously retained flash interval (%dms)", value);
	flash_rate = value;
      }
    })
    ELSEWHEN("status/flash/duty",{
      // This is normally irrelevant, except at startup where we
      // recover any previously retained status of the light.
      int value = payload.toInt();
      if (value != flash_duty) {
	LEAF_INFO("Restoring previously retained flash duty cycle (%d%%)", value);
	flash_duty = value;
      }
    })

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
