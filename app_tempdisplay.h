//
// This class implements the display logic for an environmetal sensor
//
#pragma once

#include "abstract_app.h"

//RTC_DATA_ATTR int saved_something = 0;

class TempDisplayAppLeaf : public AbstractAppLeaf
{
protected: // configuration preferences

protected: // ephemeral state
  float temperature;
  float pressure;
  OledLeaf *screen;

public:
  TempDisplayAppLeaf(String name, String target)
    : AbstractAppLeaf(name,target) {
    LEAF_ENTER(L_INFO);
    this->target=target;
    // default variables or constructor argument processing goes here
    LEAF_LEAVE;
  }

  virtual void setup(void) {
    AbstractAppLeaf::setup();
    LEAF_ENTER(L_INFO);
    screen = (OledLeaf *)get_tap("screen");
    LEAF_LEAVE;
  }

  virtual void loop(void)
  {
    Leaf::loop();
  }

  void update() 
  {
    char cmd[128];
    char when[16];
    time_t now = time(NULL);
    LEAF_NOTICE("update");

    message(screen, "cmd/clear", "");
    strftime(when, sizeof(when), "%a %l:%M", localtime(&now));
   
    snprintf(cmd, sizeof(cmd), "{\"font\":10,\"align\":\"center\",\"row\": 0,\"column\": 32,\"text\":\"%s\"}", when);
    message(screen, "cmd/draw", cmd);

    snprintf(cmd, sizeof(cmd), "{\"font\":24,\"align\":\"center\",\"row\": 10,\"column\": 32,\"text\":\"%.1f\"}", temperature);
    message(screen, "cmd/draw", cmd);

    snprintf(cmd, sizeof(cmd), "{\"font\":10,\"align\":\"center\",\"row\": 36,\"column\": 32,\"text\":\"%.1f hPa\"}", pressure);
    message(screen, "cmd/draw", cmd);

  }

  bool mqtt_receive(String type, String name, String topic, String payload)
  {
    LEAF_ENTER(L_DEBUG);
    bool handled = Leaf::mqtt_receive(type, name, topic, payload);

    LEAF_INFO("RECV %s %s %s %s", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());

    WHEN("status/temperature", {
	temperature=payload.toFloat();
	update();
      })
    WHEN("status/pressure", {
	pressure=payload.toFloat();
	update();
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
