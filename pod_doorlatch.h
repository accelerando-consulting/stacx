
class DoorLatchPod : public Pod
{
public:
  bool standby;
  bool timedUnlock;
  bool lockState;
  bool failState;
  unsigned long timedUnlockEnd;

  DoorLatchPod(String name, unsigned long long pins, bool defaultState = false) : Pod("door-latch", name, pins){
    standby = false;
    timedUnlock = false;
    lockState = defaultState;
    failState = defaultState;
  }

  void setup(void) {
    Pod::setup();
    enable_pins_for_output();
  }

  void loop(void) {
    Pod::loop();
    
    if ( !standby && timedUnlock && (millis() >= timedUnlockEnd) ) {
      NOTICE("Deactivating timed unlock");
      setLock(true);
      timedUnlock = false;
    }
  }

  void mqtt_subscribe() {
    ENTER(L_INFO);
    _mqtt_subscribe(base_topic+"/set/lock");
    _mqtt_subscribe(base_topic+"/cmd/unlock");
    _mqtt_subscribe(base_topic+"/cmd/status");
    _mqtt_subscribe(base_topic+"/set/standby");
    LEAVE;
  }
  
  void setLock(bool locked) {
    const char *lockness = locked?"locked":"unlocked";
    NOTICE("Set lock relay to %s", lockness);

    // The relay is wired with the solenoid to normally closed, so we
    // ENERGISE the relay to unlock
    //
    lockState = locked;
    if (locked) {
      clear_pins();
    } else {
      set_pins();
    }
    mqtt_publish("status/lock", lockness, true);
  }

  void mqtt_connect() 
  {
    setLock(lockState);
  }
  
  void mqtt_disconnect() 
  {
    setLock(failState);
  }
  

  bool mqtt_receive(String type, String name, String topic, String payload) {
    if (!Pod::mqtt_receive(type, name, topic, payload)) return false;
    ENTER(L_INFO);
    bool handled = false;
    
    WHEN("set/lock",{
      INFO("Updating lock via set operation");
      setLock((payload.toInt() == 1));
      handled = true;
      })
    ELSEWHEN("status/lock",{
      // Ignore this except when receiving retained state at first startup 
      bool newLockState = (payload.toInt() == 1);
      if (newLockState != lockState) {
	INFO("Updating lock via retained status");
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
    ELSEWHEN("cmd/status",{
      INFO("Refreshing lock status");
      setLock(lockState);
    })
    ELSEWHEN("cmd/unlock",{
      int duration = payload.toInt();
      if (standby) {
	INFO("Ignore unlock command in standby mode");
      } else if (duration > 0) {
	INFO("unlock via command");
	setLock(false);
	timedUnlock = true;
	timedUnlockEnd = millis() + (1000*duration);
      }
      else {
	ALERT("Invalid unlock duration: [%s]", payload.c_str());
      }
    })
    LEAVE;
    return handled;
  }
};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
