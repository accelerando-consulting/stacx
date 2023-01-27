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
  bool button_momentary=true;
  static constexpr const char *COLOR_OFF="orange";
  static constexpr const char *COLOR_ON="green";
  static constexpr const char *COLOR_TEMP="blue";
  

protected: // ephemeral state
  bool state = false;
  uint32_t auto_off_time = 0;

public:
  LightswitchAppLeaf(String name, String target)
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
    registerBoolValue("button_momentary", &button_momentary, "Light switch is a momentary action (press on/press off)");
    registerIntValue("button_interval", &button_interval, "Duration of light actuation upon button press").toInt();
    registerIntValue("motion_interval", &motion_interval, "Duration of light actuation upon motino trigger").toInt();

    LEAF_LEAVE;
  }

  virtual void start(void) 
  {
    AbstractAppLeaf::start();
    if (hasActiveTap("pixel")) {
      message("pixel", "set/color", state?COLOR_ON:COLOR_OFF);
    }
  }

  virtual void loop(void)
  {
    Leaf::loop();

    if (state && auto_off_time && (millis() >= auto_off_time)) {
      LEAF_NOTICE("Turning off light via timer");
      publish("set/light", false);
      if ( hasActiveTap("pixel")) {
	message("pixel", "set/color", COLOR_OFF);
      }
      auto_off_time = 0;
    }
  }

  virtual void status_pub() 
  {
    mqtt_publish("status/light", state?"lit":"unlit");
  }

  virtual bool mqtt_receive(String type, String name, String topic, String payload, bool direct=false)
  {
    LEAF_ENTER(L_DEBUG);
    bool handled = false;

    LEAF_NOTICE("RECV %s %s %s %s", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());


    WHEN("status/light", {
	state = (payload=="lit");
	LEAF_NOTICE("Noting status of light: %s (%d)", payload, (int)state);
	status_pub();
      })
    WHEN("event/press", {
	if (button_momentary && state) {
	  LEAF_NOTICE("Turning off light via button");
	  publish("set/light", false);
	  if (hasActiveTap("pixel")) {
	    message("pixel", "set/color", COLOR_OFF);
	  }
	  auto_off_time = 0;
	}
	else {
	  LEAF_NOTICE("Turning on light via button");
	  publish("set/light", true);
	  if (hasActiveTap("pixel")) {
	    message("pixel", "set/color", COLOR_ON);
	  }
	  if (button_interval) {
	    LEAF_INFO("Light will auto-off after %dms", button_interval);
	    uint32_t offat = millis()+(button_interval*1000);
	    if (offat > auto_off_time) {
	      auto_off_time = offat;
	    }
	  }
	}
    })
    WHEN("event/release", {
	if (!button_momentary) {
	  LEAF_NOTICE("Turning off light via switch");
	  publish("set/light", false);
	  if (hasActiveTap("pixel")) {
	    message("pixel", "set/color", COLOR_OFF);
	  }
	}
    })
    ELSEWHEN("event/motion", {
	if (!state) {
	  LEAF_NOTICE("Turning on light via motion");
	  publish("set/light", true);
	  if (hasActiveTap("pixel")) {
	    message("pixel", "set/color", COLOR_TEMP);
	  }
	}
	if (motion_interval) {
	  uint32_t offat = millis()+(motion_interval*1000);
	  if (offat > auto_off_time) {
	    auto_off_time = offat;
	  }
	}
    })
    else {
      handled = Leaf::mqtt_receive(type, name, topic, payload, direct);
    }

    LEAF_LEAVE;
    RETURN(handled);
  }

};



// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
