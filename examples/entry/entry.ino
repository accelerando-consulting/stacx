#pragma STACX_BOARD espressif:esp32:ttgo-t7-v13-mini32
#include "variant_pins.h"
#include "config.h"
#define USE_OTA 1
#define USE_FTP 0
#include "stacx.h"

#if USE_PREFS
#include "leaf_fs_preferences.h"
#endif
#include "leaf_ip_null.h"
#include "leaf_pubsub_null.h"
#include "leaf_ip_esp.h"
#include "leaf_pubsub_mqtt_esp.h"
#include "leaf_shell.h"

#include "leaf_lock.h"
#include "leaf_contact.h"
#include "leaf_button.h"

#include "leaf_tone.h"
#include "leaf_light.h"
#include "leaf_motion.h"
#include "abstract_app.h"

class EntryAppLeaf : public AbstractAppLeaf
{
protected: // configuration preferences
  uint32_t button_interval_sec=0;
  uint32_t motion_interval_sec=120;
  uint32_t unlock_interval_msec=2000;

protected: // ephemeral state
  bool state = false;
  uint32_t auto_off_time = 0;

public:
  EntryAppLeaf(String name, String target)
    : AbstractAppLeaf(name,target)
    , Debuggable(name)
  {
    LEAF_ENTER(L_INFO);
    // default variables or constructor argument processing goes here
    LEAF_LEAVE;
  }

  virtual void setup(void) {
    AbstractAppLeaf::setup();
    LEAF_ENTER(L_INFO);
    registerIntValue("button_interval_sec", &button_interval_sec  , "Duration of light actuation upon button press");
    registerIntValue("motion_interval_sec", &motion_interval_sec  , "Duration of light actuation upon motino trigger");
    registerIntValue("unlock_interval_msec", &unlock_interval_msec, "Duration of lock actuation (milliseconds)");
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
	state = (payload=="lit");
	LEAF_NOTICE("Noting status of light: %s (%d)", payload, (int)state);
      })
    WHEN("event/press", {
	if (state) {
	  LEAF_NOTICE("Turning off light via button");
	  publish("set/light", false);
	  auto_off_time = 0;
	}
	else {
	  LEAF_NOTICE("Turning on light via button");
	  publish("set/light", true);
	  if (button_interval_sec) {
	    uint32_t offat = millis()+(button_interval_sec*1000);
	    if (offat > auto_off_time) {
	      auto_off_time = offat;
	    }
	  }
	}
    })
    ELSEWHEN("event/motion", {
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
    })
    else {
      handled = AbstractAppLeaf::mqtt_receive(type, name, topic, payload, direct);
    }

    LEAF_LEAVE;
    RETURN(handled);
  }

};

Leaf *leaves[] = {
  //new FSLeaf("fs", FS_DEFAULT, FS_ALLOW_FORMAT),
#if USE_PREFS
  new FSPreferencesLeaf("prefs"),
#endif
  new ShellLeaf("shell"),
  
  new IpEspLeaf("wifi","fs,prefs"),
  (new PubsubEspAsyncMQTTLeaf("wifimqtt","fs,prefs,wifi"))->setTrace(L_NOTICE),
  //new IpNullLeaf("nullip", "fs"),
  //new PubsubNullLeaf("nullmqtt", "nullip"),


  new LockLeaf("entry" ,      LEAF_PIN( D1 /*OUT*/), true, true), 
  new ContactLeaf("entry",    LEAF_PIN( D2 /* IN*/)),
  new ButtonLeaf("egress",    LEAF_PIN( D3 /* IN*/)),
  
  new ToneLeaf("spkr",        LEAF_PIN( D5 /*OUT*/)),
  (new LightLeaf("entry", "", LEAF_PIN( D6 /*OUT*/)))->setTrace(L_NOTICE),
  new MotionLeaf("porch",     LEAF_PIN( D7 /* IN*/)),
  new MotionLeaf("entry",     LEAF_PIN( D8 /* IN*/)),
  
  (new EntryAppLeaf("entry", "light@entry,button@egress,motion@porch,motion@entry"))->setTrace(L_NOTICE),
  
  NULL
};


// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
