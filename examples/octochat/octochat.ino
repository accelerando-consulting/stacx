#include "config.h"
#include "stacx/stacx.h"

//
// Example stack: An octochat footboard drives soldering tools (eg vacuum pen, power solder feed, extractor)
//
#include "stacx/leaf_fs_preferences.h"
#include "stacx/leaf_shell.h"
#include "stacx/leaf_ip_null.h"
#include "stacx/leaf_pubsub_null.h"

#include "stacx/leaf_wire.h"
#include "stacx/leaf_actuator.h"
#include "stacx/leaf_pinextender_pcf8574.h"
#include "stacx/abstract_app.h"

class OctoAppLeaf : public AbstractAppLeaf
{
protected: // configuration preferences

protected: // ephemeral state
  unsigned long last_press=0;
  bool extractor=false;

public:
  OctoAppLeaf(String name, String targets)
    : AbstractAppLeaf(name, targets)
    , Debuggable(name, L_INFO)
    {
    }

  virtual void start(void) {
    AbstractAppLeaf::start();
    LEAF_ENTER(L_INFO);
    message("keys", "set/publish_bits", "off");
    LEAF_LEAVE;
  }

  virtual bool mqtt_receive(String type, String name, String topic, String payload)
  {
    LEAF_ENTER(L_DEBUG);
    bool handled = false;

    LEAF_INFO("App RECV %s/%s %s <= [%s]", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());

    WHENFROMSUB("keys", "status/~KEY", {
      LEAF_NOTICE("Keypress %s", topic.c_str());
      int key = topic.substring(topic.length()-1).toInt();
      int value = payload.toInt();
      message("keys", "set/pin/~LED"+String(key), payload);
      switch (key) {
      case 0:
	// vacuum pickup is momentary
	message("vacuum", "set/actuator", ABILITY(value));
	break;
      case 1:
	// solder feed is momentary 
	message("solder", "set/actuator", ABILITY(value));
	break;
      case 2:
	// extractor fan is push on push off
	if (value==1) {
	  extractor = !extractor;
	  message("extractor", "set/actuator", ABILITY(extractor));
	}
	break;
      }
    })
    else {
      handled = AbstractAppLeaf::mqtt_receive(type, name, topic, payload);
    }

    if (!handled) {
      LEAF_INFO("app did not consume type=%s name=%s topic=%s payload=%s", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());
    }

    LEAF_BOOL_RETURN(handled);
  }

};

const char *key_names = "~KEY0,~LED0,~KEY1,~LED1,~KEY2,~LED2,~KEY3,~LED3";


Leaf *leaves[] = {
  new FSPreferencesLeaf("prefs"),
  new ShellLeaf("shell", "Stacx CLI"),

  new IpNullLeaf("nullip", "fs"),
  new PubsubNullLeaf("nullmqtt", "ip"),
  
  new WireBusLeaf("wire",       /*sda=*/12, /*scl=*/13),

  // One can have a number of octochat button groups, each group has up to four keys
  new PinExtenderPCF8574Leaf("keys", 0x38, key_names, 0xAA),

  new ActuatorLeaf("vacuum", NO_TAPS, LEAF_PIN(5)),
  //new ActuatorLeaf("solder", NO_TAPS, LEAF_PIN(4)),
  //new ActuatorLeaf("extractor", NO_TAPS, LEAF_PIN(0)),
  new OctoAppLeaf("app", "keys,wire,vacuum"),

  NULL
};
