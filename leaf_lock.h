
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
    mqtt_subscribe("/cmd/unlock");
    mqtt_subscribe("/set/lock");
    mqtt_subscribe("/set/standby");
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

  bool mqtt_receive(String type, String name, String topic, String payload) {
    LEAF_ENTER(L_INFO);
    bool handled = Leaf::mqtt_receive(type, name, topic, payload);
    
    WHEN("set/lock",{
      LEAF_INFO("Updating lock via set operation");
      bool lock;
      if (payload == "1") lock=true;
      else if (payload == "0") lock=false;
      else if (payload == "true") lock=true;
      else if (payload == "false") lock=false;
      else if (payload == "on") lock=true;
      else if (payload == "off") lock=false;
      else if (payload == "unlocked") lock=false;
      else if (payload == "locked") lock=true;
      else if (payload == "open") lock=false;
      else if (payload == "closed") lock=true;
      else lock = true;
      setLock(lock);
      })
    ELSEWHEN("status/lock",{
      // Ignore this except when receiving retained state at first startup 
      bool newLockState = (payload.toInt() == 1);
      if (newLockState != lockState) {
	LEAF_INFO("Updating lock via retained status");
	setLock(newLockState);
      }
      })
    ELSEWHEN("set/standby",{
      standby = (payload.toInt() == 1);
      if (standby) {
	setLock(false);
      } else {
	setLock(lockState);
      }
      mqtt_publish("status/standby", standby?"standby":"normal", true);
    })
    ELSEWHEN("cmd/unlock",{
      int duration = payload.toInt();
      if (standby) {
	LEAF_INFO("Ignore unlock command in standby mode");
      } else if (duration > 0) {
	LEAF_INFO("unlock via command");
	setLock(false);
	timedUnlock = true;
	timedUnlockEnd = millis() + (1000*duration);
      }
      else {
	LEAF_ALERT("Invalid unlock duration: [%s]", payload.c_str());
      }
    })
    LEAF_LEAVE;
    return handled;
  }
};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
