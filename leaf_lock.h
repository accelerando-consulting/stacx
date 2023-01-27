class LockLeaf : public Leaf
{
public:
  bool standby;
  bool timedUnlock;
  bool lockState;
  bool failState;
  bool invert;
  unsigned long timedUnlockEnd;

  LockLeaf(String name, pinmask_t pins, bool defaultState = false, bool invertLogic = false) : Leaf("lock", name, pins){
    standby = false;
    timedUnlock = false;
    lockState = defaultState;
    failState = defaultState;
    invert = invertLogic;
    LEAF_INFO("Lock %s failState=%s invert=%s", base_topic.c_str(), TRUTH(defaultState), TRUTH(invertLogic));
  }

  void setup(void) {
    Leaf::setup();
    enable_pins_for_output();

    registerCommand(HERE, "unlock", "unlock the lock temporarily");
    registerValue(HERE, "lock", VALUE_KIND_BOOL, &lockState, "set the lock state");
    registerValue(HERE, "standby", VALUE_KIND_BOOL, &standby, "set standby mode");
  }

  void loop(void) {
    Leaf::loop();

    if ( !standby && timedUnlock && (millis() >= timedUnlockEnd) ) {
      LEAF_NOTICE("Deactivating timed unlock");
      setLock(true);
      timedUnlock = false;
    }
  }

  void mqtt_do_subscribe() {
    LEAF_ENTER(L_INFO);
    Leaf::mqtt_do_subscribe();
    LEAF_LEAVE;
  }

  void setLock(bool locked) {
    const char *lockness = locked?"locked":"unlocked";
    LEAF_NOTICE("Set %s lock relay %sto %s", base_topic.c_str(), invert?"(inverted) ":"", lockness);

    // It will depend on whether your lock is a latch-type (energise to
    // free) or magnet type (energise to lock) on whether or not
    // you want to invert the logic.  You can also tweak it in hardware by
    // wiring to the relay's normally-closed or normally-open terminals.
    // You might also be using an inverting relay driver arrangement.
    //
    // Look this is really confusing, so lets table it
    //
    //  Lock  |   Invert   |   Pin value
    //   1    |     0      |       1
    //   0    |     0      |       0
    //   1    |     1      |       0
    //   0    |     1      |       1
    //
    // Eyballing that truth table, this is equivalent to P = L XOR I
    //
    if (locked ^ invert) {
      set_pins();
    } else {
      clear_pins();
    }
    mqtt_publish("status/lock", lockness, true);
  }

  void mqtt_connect()
  {
    Leaf::mqtt_connect();
    setLock(lockState);
  }

  void mqtt_disconnect()
  {
    setLock(failState);
  }

  void status_pub()
  {
      LEAF_INFO("Refreshing lock status");
      setLock(lockState);
  }

  virtual bool valueChangeHandler(String topic, Value *val)
  {
    bool handled=false;

    WHEN("lock", setLock(VALUE_AS(bool, val)))
    ELSEWHEN("standby", setLock(VALUE_AS(bool, val)?false:lockState));
    else handled = Leaf::valueChangeHandler(topic, v);

    return handled;
  }

  virtual bool commandHandler(String type, String name, String topic, String payload) {
    bool handled=false;

    WHEN("unlock",{
	int duration = payload.toInt();
	if (standby) {
	  LEAF_INFO("Ignore unlock command in standby mode");
	} else if (duration > 0) {
	  LEAF_INFO("unlock via command");
	  setLock(false);
	  timedUnlock = true;
	  timedUnlockEnd = millis() + (1000*duration);
	}
      })
    else handled = Leaf::commandHandler(type,name,topic,payload);

    return handled;
  }

  virtual bool parseBool(String value, bool default_value=false, bool *valid_r=NULL)
  {
    bool lock = false;
    if ((payload=="unlocked") || (payload=="open")) {
      payload = false;
      if (valid_r) *valid_r = true;
    }
    else if ((payload=="locked") || (payload=="closed")) {
      payload = false;
      if (valid_r) *valid_r = true;
    }
    else {
      lock = Leaf::parseBool(value, default_value, valid_r);
    }
    return lock;
  }

  virtual bool mqtt_receive(String type, String name, String topic, String payload, direct) {
    LEAF_ENTER(L_INFO);
    bool handled = false;

    WHEN("status/lock",{
      // Ignore this except when receiving retained state at first startup
      bool newLockState = (payload.toInt() == 1);
      if (newLockState != lockState) {
	LEAF_INFO("Updating lock via retained status");
	setLock(newLockState);
      }
      })
    else Leaf::mqtt_receive(type, name, topic, payload, direct);

    LEAF_LEAVE;
    return handled;
  }
};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
