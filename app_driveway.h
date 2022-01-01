//
// This class implements the control logic for an outdoor lighting
// installation that triggers from motion only during hours of darkness
//
#pragma once

#include "abstract_app.h"

//RTC_DATA_ATTR int saved_something = 0;

class DrivewayAppLeaf : public AbstractAppLeaf
{
protected: // configuration preferences
  uint32_t motion_interval_sec;

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
    motion_interval_sec = getPref("motion_interval_sec", "120").toInt();
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

  bool mqtt_receive(String type, String name, String topic, String payload)
  {
    LEAF_ENTER(L_DEBUG);
    bool handled = AbstractAppLeaf::mqtt_receive(type, name, topic, payload);

    LEAF_NOTICE("RECV %s %s %s %s", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());


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
