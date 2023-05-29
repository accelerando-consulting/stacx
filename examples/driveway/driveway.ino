#include "stacx/stacx.h"

#include "stacx/leaf_fs_preferences.h"
#include "stacx/leaf_ip_esp.h"
#include "stacx/leaf_pubsub_mqtt_esp.h"
#include "stacx/leaf_shell.h"

#include "stacx/leaf_lock.h"
#include "stacx/leaf_contact.h"
#include "stacx/leaf_button.h"

#include "stacx/leaf_light.h"
#include "stacx/leaf_motion.h"
#include "stacx/leaf_level.h"
#include "stacx/abstract_app.h"

class DrivewayAppLeaf : public AbstractAppLeaf
{
protected: // configuration preferences
  uint32_t motion_interval_sec=120;

protected: // ephemeral state
  bool state = false;
  bool dark = false;
  uint32_t auto_off_time = 0;

public:
  DrivewayAppLeaf(String name, String target)
    : AbstractAppLeaf(name,target) {
    LEAF_ENTER(L_INFO);
    this->target=target;
    // default variables or constructor argument processing goes here
    LEAF_LEAVE;
  }

  virtual void setup(void) {
    AbstractAppLeaf::setup();
    LEAF_ENTER(L_INFO);
    registerIntValue("motion_interval_sec", &motion_interval_sec, "Duration of actuation upon motion trigger")
    LEAF_LEAVE;
  }

  virtual void loop(void)
  {
    AbstractAppLeaf::loop();

    if (state && auto_off_time && (millis() >= auto_off_time)) {
      LEAF_DEBUG("Turning off light via timer");
      publish("set/light", false);
      auto_off_time = 0;
    }
  }

  virtual bool mqtt_receive(String type, String name, String topic, String payload, bool direct=false)
  {
    LEAF_ENTER(L_DEBUG);
    bool handled = false;

    LEAF_INFO("RECV %s %s %s %s", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());


    WHEN("status/light", {
      handled=true;
      state = (payload=="lit");
      LEAF_NOTICE("Noting status of light: %s (%d)", payload, (int)state);
    })
      ELSEWHEN("status/level", {
	  dark=(payload != "1");
	  mqtt_publish("status/daylight", dark?"dark":"light");
    })
      ELSEWHEN("event/motion", {
	if (dark) {
	  if (!state) {
	    LEAF_NOTICE("Turning on light via motion");
	    publish("set/light", true);
	  }
	  if (motion_interval_sec) {
	    uint32_t offat = millis()+(motion_interval_sec*1000);
	    if (offat > auto_off_time) {
	      auto_off_time = offat;
	    }
	  }
	}
    })
    else {
      handled = AbstractAppLeaf::mqtt_receive(type, name, topic, payload, direct);
    }

    LEAF_LEAVE;
    RETURN(handled);
  }

};

Leaf *leaves[] = {
	new FSPreferencesLeaf("prefs"),
	new IpEspLeaf("esp","prefs"),
	new PubsubEspAsyncMQTTLeaf("espmqtt","prefs"),
	new ShellLeaf("shell"),

	new MotionLeaf("driveway", LEAF_PIN(16 /* D0  IN */)),
	new LightLeaf("driveway",   "", LEAF_PIN(14 /* D5 OUT */)),
	new LevelLeaf("daylight",  LEAF_PIN(15 /* D8  IN */)), // light level

	new DrivewayAppLeaf("driveway", "light@driveway,level@daylight,motion@driveway"),

	NULL
};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:


