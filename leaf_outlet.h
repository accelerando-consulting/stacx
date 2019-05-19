

class OutletLeaf : public Leaf
{
public:
  bool state;

  OutletLeaf(String name, pinmask_t pins) : Leaf("outlet", name, pins){
    state = false;
  }

  void setup(void) {
    Leaf::setup();
    enable_pins_for_output();
  }

  void mqtt_subscribe() {
    ENTER(L_NOTICE);
    Leaf::mqtt_subscribe();
    _mqtt_subscribe(base_topic+"/set/outlet");
    _mqtt_subscribe(base_topic+"/status/outlet");
    LEAVE;
  }
	
  void setOutlet(bool on) {
    const char *newstate = on?"on":"off";
    NOTICE("Set outlet relay to %s", newstate);
    if (newstate) {
      set_pins();
    } else {
      clear_pins();
    }
    state = on;
    mqtt_publish("status/outlet", newstate, true);
  }

  void status_pub() 
  {
      INFO("Refreshing device status");
      setOutlet(state);
  }

  bool mqtt_receive(String type, String name, String topic, String payload) {
    ENTER(L_INFO);
    bool handled = Leaf::mqtt_receive(type, name, topic, payload);
    bool on = false;
    if (payload == "1") on=true;
    else if (payload == "on") on=true;
    else if (payload == "true") on=true;
    else if (payload == "high") on=true;

    WHEN("set/outlet",{
      INFO("Updating outlet via set operation");
      setOutlet(on);
      })
    ELSEWHEN("status/outlet",{
      // This is normally irrelevant, except at startup where we
      // recover any previously retained status of the outlet.
      if (on != state) {
	INFO("Restoring previously retained outlet status");
	setOutlet(on);
      }
    })

    LEAVE;
    return handled;
  };
    
};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
