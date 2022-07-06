//
// This class implements the control logic for a light switch
//
#pragma once

#include "abstract_app.h"

//RTC_DATA_ATTR int saved_something = 0;

class LightswitchAppLeaf : public AbstractAppLeaf
{
protected: // configuration preferences
  uint32_t button_interval;
  uint32_t motion_interval;

protected: // ephemeral state
  bool state = false;
  uint32_t auto_off_time = 0;

public:
  LightswitchAppLeaf(String name, String target)
    : AbstractAppLeaf(name,target) {
    LEAF_ENTER(L_INFO);
    // default variables or constructor argument processing goes here
    LEAF_LEAVE;
  }

  virtual void setup(void) {
    AbstractAppLeaf::setup();
    LEAF_ENTER(L_INFO);
    button_interval = getPref("button_interval", "0", "Duration of light actuation upon button press").toInt();
    motion_interval = getPref("motion_interval", "120", "Duration of light actuation upon motino trigger").toInt();
    blink_enable = getPref("blink_enable", "1","Duration of lock actuation (milliseconds)").toInt();
    LEAF_LEAVE;
  }

  virtual void loop(void)
  {
    Leaf::loop();

    if (state && auto_off_time && (millis() >= auto_off_time)) {
      LEAF_NOTICE("Turning off light via timer");
      publish("set/light", false);
      auto_off_time = 0;
    }
  }

  bool mqtt_receive(String type, String name, String topic, String payload)
  {
    LEAF_ENTER(L_DEBUG);
    bool handled = Leaf::mqtt_receive(type, name, topic, payload);

    LEAF_NOTICE("RECV %s %s %s %s", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());


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
	  if (button_interval) {
	    LEAF_INFO("Light will auto-off after %dms", button_interval);
	    uint32_t offat = millis()+(button_interval*1000);
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
	if (motion_interval) {
	  uint32_t offat = millis()+(motion_interval*1000);
	  if (offat > auto_off_time) {
	    auto_off_time = offat;
	  }
	}
    })
    else {
      LEAF_DEBUG("app did not consume type=%s name=%s topic=%s payload=%s", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());
    }

    LEAF_LEAVE;
    RETURN(handled);
  }

};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
