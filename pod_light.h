

//@***************************** class LightPod ******************************


class LightPod : public Pod
{
public:
  bool state;

  LightPod(String name, pinmask_t pins) : Pod("light", name, pins){
    state = false;
  }

  void setup(void) {
    Pod::setup();
    enable_pins_for_output();
  }

  void mqtt_subscribe() {
    ENTER(L_NOTICE);
    Pod::mqtt_subscribe();
    _mqtt_subscribe(base_topic+"/set/light");
    _mqtt_subscribe(base_topic+"/status/light");
    LEAVE;
  }
	
  void setLight(bool lit) {
    const char *litness = lit?"lit":"unlit";
    NOTICE("Set light relay to %s", litness);
    if (lit) {
      // The relay is active low
      clear_pins();
    } else {
      set_pins();
    }
    state = lit;
    mqtt_publish("status/light", litness, true);
  }

  bool mqtt_receive(String type, String name, String topic, String payload) {
    ENTER(L_INFO);
    bool handled = Pod::mqtt_receive(type, name, topic, payload);
    bool lit = false;
    if (payload == "on") lit=true;
    else if (payload == "true") lit=true;
    else if (payload == "lit") lit=true;
    else if (payload == "high") lit=true;
    else if (payload == "1") lit=true;

    WHEN("set/light",{
      INFO("Updating light via set operation");
      setLight(lit);
      })
    ELSEWHEN("status/light",{
      // This is normally irrelevant, except at startup where we
      // recover any previously retained status of the light.
      if (lit != state) {
	INFO("Restoring previously retained light status");
	setLight(lit);
      }
    })
    ELSEWHEN("cmd/status",{
      INFO("Refreshing device status");
      setLight(state);
    });

    LEAVE;
    return handled;
  };
    
};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
