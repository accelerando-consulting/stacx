//
// This class implements the button-to-relay feedback for an on-off light switch
// (it fails-safe when the network is offline, allowing manual control)
//

#define SWITCH_MODE_BISTABLE 0
#define SWITCH_MODE_MOMENTARY 1
#define SWITCH_MODE_TIMER 2
#define SWITCH_MODE_COUNT 3

static const char *switch_mode_names[SWITCH_MODE_COUNT] = {
  "bistable",
  "momentary",
  "timer"
};

class SwitchLeaf : public Leaf
{
  int mode;
  String target;

  Leaf *inputLeaf = NULL;
  Leaf *outputLeaf = NULL;

  bool unknown_mode = true;
  bool unknown_delay = true;
  int switch_interval = 60;
  
public:
    SwitchLeaf(String name, String target, int mode=0)
	  : Leaf("switch", name, 0) {
    LEAF_ENTER(L_INFO);
    this->target = target;
    this->mode = mode;
    LEAF_LEAVE;
  }

  virtual void setup(void) {
    Leaf::setup();
    LEAF_ENTER(L_ALERT);
    this->install_taps(target);
    LEAF_LEAVE;
  }

  virtual void mqtt_do_subscribe() {
    Leaf::mqtt_do_subscribe();
    LEAF_ENTER(L_ALERT);
    mqtt_subscribe("/set/mode");
    mqtt_subscribe("/status/mode");
    mqtt_subscribe("/set/delay");
    mqtt_subscribe("/status/delay");
    LEAF_LEAVE;
  }

  void set_mode(int new_mode)
  {
    LEAF_ENTER(L_DEBUG);

    String mode_name = "UNKNOWN";
    if ((new_mode >= 0) && (new_mode < SWITCH_MODE_COUNT)) {
      mode_name = switch_mode_names[new_mode];
    }
    mode_name.toUpperCase();
    if (new_mode == mode) {
      LEAF_ALERT("Mode set to %d (%s) but was already in this mode\n", new_mode, mode_name);
      return;
    }
    LEAF_ALERT("SET %s MODE", mode_name);

#if 0
    if (new_mode == WHATEVER) {
      doStuff();
    }
#endif
    if (0) {
    }
else {
      LEAF_ALERT("Unhandled mode value %d", new_mode);
      LEAF_LEAVE;
      return;
    }
    mode = new_mode;
    mqtt_publish("status/mode", String(switch_mode_names[mode]),0,true);

    LEAF_LEAVE;
  }

  virtual void loop(void)
  {
    Leaf::loop();

    time_t now = time(NULL);

#ifdef NOTYET
    switch (mode) {
    case WHATEVER:
    default:
      LEAF_ALERT("WTF unhandled mode %d", mode);
    }
#endif    
  }

  virtual bool mqtt_receive(String type, String name, String topic, String payload) {
    LEAF_ENTER(L_DEBUG);
    bool handled = Leaf::mqtt_receive(type, name, topic, payload);

    LEAF_ALERT("%s %s %s %s", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());

    WHEN("set/mode", {
	LEAF_ALERT("Received command to change mode");
	int value = parseSwitchModeName(payload);
	if ( (value >= 0) && (value < SWITCH_MODE_COUNT) ) {
	  LEAF_ALERT("Changing to %s mode", switch_mode_names[value]);
	  set_mode(value);
	}
	else {
	  LEAF_ALERT("Unhandled set-mode value %s", payload.c_str());
	}
      })
    ELSEWHEN("status/mode", {
	LEAF_ALERT("RECEIVED MODE STATUS %s", payload.c_str());
	if (unknown_mode) {
	  // This module does not yet know its mode, recover last mode from
	  // persistant status topic
	  int value = parseSwitchModeName(payload);
	  if ( (value >= 0) && (value < SWITCH_MODE_COUNT) ) {
	    String name = switch_mode_names[value];
	    name.toUpperCase();
	    LEAF_ALERT("RESTORING %s MODE", name.c_str());
	    set_mode(value);
	    unknown_mode=false;
	  }
	  else {
	    LEAF_ALERT("UNHANDLED RETAINED MODE VALUE %s", payload.c_str());
	  }
	}
      })
    else {
      LEAF_DEBUG("did not consume type=%s name=%s topic=%s payload=%s", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());
    }


    RETURN(handled);
  }

private:
  int parseSwitchModeName(const String name) {
    int i, value = -1;
    String payload = name;
    payload.toLowerCase();
    for (i=0; i< SWITCH_MODE_COUNT; i++) {
      if (payload == switch_mode_names[i]) {
	value = i;
	break;
      }
    }
    if (i >= SWITCH_MODE_COUNT) {
      ALERT("invalid mode input [%s]", payload.c_str());
    }
    return value;
  }



};



// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
