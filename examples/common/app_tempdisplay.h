#pragma once

#include "abstract_app.h"

class TempDisplayAppLeaf : public AbstractAppLeaf
{
protected: // configuration preferences

protected: // ephemeral state
  int battery_mv = -1;
  float temperature=NAN;
  float humidity=NAN;
  Leaf *screen=NULL;

public:
  TempDisplayAppLeaf(String name, String target)
    : AbstractAppLeaf(name,target)
    , Debuggable(name)
 {
 }

  virtual void setup(void) {
    AbstractAppLeaf::setup();
    LEAF_ENTER(L_INFO);
    screen = get_tap("screen");
    LEAF_LEAVE;
  }

  virtual void loop(void)
  {
    AbstractAppLeaf::loop();
  }

  virtual void status_pub() 
  {
    if (!isnan(temperature)) {
      mqtt_publish("status/temperature", String(temperature, 1));
    }
    if (!isnan(humidity)) {
      mqtt_publish("status/humidity", String(humidity, 1));
    }
    if (battery_mv != -1) {
      mqtt_publish("status/battery", String(battery_mv));
    }
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

    snprintf(cmd, sizeof(cmd), "{\"font\":10,\"align\":\"center\",\"row\": 36,\"column\": 32,\"text\":\"%.1f %%\"}", humidity);
    message(screen, "cmd/draw", cmd);

  }

  virtual bool mqtt_receive(String type, String name, String topic, String payload, bool direct=false)
  {
    LEAF_ENTER(L_DEBUG);
    bool handled = false;

    LEAF_INFO("RECV %s %s %s %s", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());

    WHEN("status/temperature", {
	temperature=payload.toFloat();
	if (screen) update();
	mqtt_publish("status/temperature", payload);
      })
    ELSEWHEN("status/humidity", {
	humidity=payload.toFloat();
	if (screen) update();
	mqtt_publish("status/humidity", payload);
      })
    ELSEWHEN("status/battery", {
	int mv_new=payload.toInt();
	if (screen) update();
	mqtt_publish("status/battery", payload);
      })
    else {
      handled = AbstractAppLeaf::mqtt_receive(type, name, topic, payload, direct);
    }

    LEAF_LEAVE;
    RETURN(handled);
  }

};
// local Variables:
// mode: C++
// c-basic-offset: 2
// End:


