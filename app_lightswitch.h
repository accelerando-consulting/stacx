//
// This class implements the control logic for a light switch
//
#pragma once

#include "abstract_app.h"

//RTC_DATA_ATTR int saved_something = 0;

class LightswitchAppLeaf : public AbstractAppLeaf
{
protected: // configuration preferences
  int button_interval_sec=0;
  int motion_interval_sec=60;
  bool button_momentary=true;
  static constexpr const char *COLOR_OFF="black";
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
    registerIntValue("button_interval", &button_interval_sec, "Duration of light actuation upon button press (seconds)");
    registerIntValue("motion_interval", &motion_interval_sec, "Duration of light actuation upon motino trigger (seconds)");

    LEAF_LEAVE;
  }

  virtual void start(void) 
  {
    AbstractAppLeaf::start();
    if (hasActiveTap("pixel")) message("pixel", state?"cmd/on":"cmd/off");
    if (hasActiveTap("indicator")) message("indicator", state?"cmd/on":"cmd/off");
  }

  virtual void loop(void)
  {
    Leaf::loop();

    if (state && auto_off_time && (millis() >= auto_off_time)) {
      LEAF_NOTICE("Turning off light via timer");
      message("light", "cmd/off");
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
	if (hasActiveTap("pixel")) message("pixel", state?"cmd/on":"cmd/off");
	if (hasActiveTap("indicator")) message("indicator", state?"cmd/on":"cmd/off");
	status_pub();
      })
    WHEN("event/press", {
	if (button_momentary && state) {
	  LEAF_NOTICE("Turning off light via button");
	  message("light", "cmd/off");
	  auto_off_time = 0;
	}
	else {
	  LEAF_NOTICE("Turning on light via button");
	  message("light", "cmd/on");
	  if (button_interval_sec) {
	    LEAF_NOTICE("Light will auto-off after %ds", button_interval_sec);
	    uint32_t offat = millis()+(button_interval_sec*1000);
	    if (offat > auto_off_time) {
	      auto_off_time = offat;
	    }
	  }
	}
    })
    WHEN("event/release", {
	if (!button_momentary) {
	  LEAF_NOTICE("Turning off light via switch");
	  message("light", "cmd/off");
	}
    })
    ELSEWHEN("event/motion", {
	if (!state) {
	  LEAF_NOTICE("Turning on light via motion");
	  message("light", "cmd/on");
	}
	if (motion_interval_sec) {
	  uint32_t offat = millis()+(motion_interval_sec*1000);
	  if (offat > auto_off_time) {
	    LEAF_NOTICE("Light will auto-off after %ds", motion_interval_sec);
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
