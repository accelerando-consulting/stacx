//
// This class implements the control logic for YOUR PROBLEM HERE
//
#pragma once

#include "abstract_app.h"

//RTC_DATA_ATTR int saved_something = 0;

class MyAppLeaf : public AbstractAppLeaf
{
protected:

public:
  MyAppLeaf(String name, String target)
    : AbstractAppLeaf(name,target) {
    LEAF_ENTER(L_INFO);
    // default variables or constructor argument processing goes here
    LEAF_LEAVE;
  }

  virtual void setup(void) {
    AbstractAppLeaf::setup();
    LEAF_ENTER(L_INFO);

    //String value;
    //value = getPref("some_setting", "0");
    //if (value) some_setting = value.toInt();

    LEAF_LEAVE;
  }

  virtual void start()
  {
    AbstractAppLeaf::start();

  }

  virtual void mqtt_do_subscribe() {
    LEAF_ENTER(L_NOTICE);
    AbstractAppLeaf::mqtt_do_subscribe();

    //mqtt_subscribe("get/something");
    //mqtt_subscribe("set/something");

    LEAF_LEAVE;
  }

  void save_sensors()
  {
    LEAF_NOTICE("Saving sensor values to RTC memory");
    //saved_something = something;
    LEAF_LEAVE;
  }

  void load_sensors()
  {
    LEAF_NOTICE("Loading sensor values from RTC memory");
    //something = saved_something;
    LEAF_LEAVE;
  }

  virtual void pre_reboot()
  {
    LEAF_ENTER(L_NOTICE);
    // your pre-reboot logic here
    AbstractAppLeaf::pre_reboot();
    LEAF_LEAVE;
  }

  virtual void pre_sleep(int duration=0)
  {
    LEAF_ENTER(L_NOTICE);
    // your pre-sleep logic here
    AbstractAppLeaf::pre_sleep();
    LEAF_LEAVE;
  }

  virtual void post_sleep()
  {
    LEAF_ENTER(L_NOTICE);
    AbstractAppLeaf::post_sleep();
    // your post-sleep logic here
    LEAF_LEAVE;
  }

  virtual void heartbeat(unsigned long uptime)
  {
    LEAF_ENTER(L_INFO);
    AbstractAppLeaf::heartbeat(uptime);

    //static const int msg_max = 512;
    //static char msg[msg_max];
    //static const char *msg_format = "formatgoesbrrr";

    //snprintf(msg, msg_max, msg_format, /*argsgohere*/);
    //mqtt_publish(heartbeat_topic, msg, qos, false);

    LEAF_LEAVE;
  }

  virtual void loop(void)
  {
    Leaf::loop();

    //unsigned long now = millis();
  }
  

  bool mqtt_receive(String type, String name, String topic, String payload)
  {
    LEAF_ENTER(L_DEBUG);
    bool handled = Leaf::mqtt_receive(type, name, topic, payload);

    LEAF_INFO("RECV %s %s %s %s", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());


    WHEN("status/adc/1", {
	//int value = payload.toInt();
	//float battery_new = 6.6 * value / 65536;
	//handle_battery_change(battery_new);
      })
    ELSEWHEN("_ip_init", {
	// IP module reports that it's ready
    })
    ELSEWHEN("_ip_nophys", {
	// IP module reports that its physical layer has lost connection
    })
    ELSEWHEN("_ip_connect", {
    })
    ELSEWHEN("_ip_disconnect", {
    })
    ELSEWHEN("_pubsub_connect", {
    })
    ELSEWHEN("_pubsub_disconnect", {
    })
    ELSEWHEN("event/press", {
    })
    ELSEWHEN("status/temp", {
	//handle_temp_change(payload.toFloat());
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
