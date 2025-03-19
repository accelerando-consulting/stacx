#include "config.h"
#include "stacx.h"

#include "abstract_app.h"
#include "leaf_fs.h"
#include "leaf_fs_preferences.h"
#include "leaf_ip_null.h"
#include "leaf_pubsub_null.h"
#include "leaf_ip_esp.h"
#include "leaf_pubsub_mqtt_esp.h"
#include "leaf_wire.h"
#include "leaf_ina219.h"
#include "leaf_shell.h"
#include "leaf_oled.h"

class MeterAppLeaf : public AbstractAppLeaf
{
protected: // configuration preferences

protected: // ephemeral state
  float volts=NAN;
  float amps=NAN;
  float vmax=NAN;
  float vmin=NAN;
  float imax=NAN;
  float imin=NAN;
  Leaf *screen=NULL;
  
public:
  MeterAppLeaf(String name, String target)
    : AbstractAppLeaf(name,target)
    , Debuggable(name)
  {
    // default variables or constructor argument processing goes here
  }

  virtual void setup(void) {
    AbstractAppLeaf::setup();
    LEAF_ENTER(L_INFO);
    screen = get_tap("screen");
    LEAF_LEAVE;
  }

  virtual void start(void) {
    AbstractAppLeaf::start();
    LEAF_ENTER(L_INFO);
    LEAF_LEAVE;
  }

  virtual void loop(void)
  {
    AbstractAppLeaf::loop();
  }

  virtual void display()
  {
    char cmd[128];
    char when[16];
    time_t now = time(NULL);
    LEAF_INFO("update display");

    message(screen, "cmd/clear", "");
    strftime(when, sizeof(when), "%a %l:%M", localtime(&now));

    snprintf(cmd, sizeof(cmd), "{\"font\":10,\"align\":\"center\",\"row\": 0,\"column\": 32,\"text\":\"%s\"}", when);
    message(screen, "cmd/draw", cmd);

    snprintf(cmd, sizeof(cmd), "{\"font\":16,\"align\":\"left\",\"row\": 16,\"column\": 0,\"text\":\"%.3f V\"}", volts);
    message(screen, "cmd/draw", cmd);
    snprintf(cmd, sizeof(cmd), "{\"font\":10,\"align\":\"left\",\"row\": 16,\"column\": 90,\"text\":\"%.3f\"}", vmin);
    message(screen, "cmd/draw", cmd);
    snprintf(cmd, sizeof(cmd), "{\"font\":10,\"align\":\"left\",\"row\": 26,\"column\": 90,\"text\":\"%.3f\"}", vmax);
    message(screen, "cmd/draw", cmd);

    snprintf(cmd, sizeof(cmd), "{\"font\":16,\"align\":\"left\",\"row\": 42,\"column\": 0,\"text\":\"%.0f mA\"}", amps/1000);
    message(screen, "cmd/draw", cmd);
    snprintf(cmd, sizeof(cmd), "{\"font\":10,\"align\":\"left\",\"row\": 42,\"column\": 90,\"text\":\"%.0f\"}", imin/1000);
    message(screen, "cmd/draw", cmd);
    snprintf(cmd, sizeof(cmd), "{\"font\":10,\"align\":\"left\",\"row\": 52,\"column\": 90,\"text\":\"%.0f\"}", imax/1000);
    message(screen, "cmd/draw", cmd);

  }



  virtual void status_pub() {
    AbstractAppLeaf::status_pub();

  }

  virtual bool mqtt_receive(String type, String name, String topic, String payload, bool direct=false)
  {
    LEAF_ENTER(L_DEBUG);
    bool handled = false;
    

    LEAF_INFO("RECV %s/%s %s <= [%s]", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());

    WHEN("status/milliamps",{
	amps = payload.toFloat()*1000;
	if (isnan(imin) || (amps < imin)) imin = amps;
	if (isnan(imax) || (amps > imax)) imax = amps;
	display();
    })
    ELSEWHEN("status/millivolts",{
	volts = payload.toFloat()/1000;
	if (isnan(vmin) || (volts < vmin)) vmin = volts;
	if (isnan(vmax) || (volts > vmax)) vmax = volts;
	display();
    })
    else {
      handled = Leaf::mqtt_receive(type, name, topic, payload);
      if (!handled) {
	LEAF_DEBUG("app did not consume type=%s name=%s topic=%s payload=%s", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());
      }
    }

    LEAF_LEAVE;
    RETURN(handled);
  }

};

Leaf *leaves[] = {

  new FSLeaf("fs", FS_DEFAULT, FS_ALLOW_FORMAT),
  new FSPreferencesLeaf("prefs"),
  new ShellLeaf("shell"),

  new IpNullLeaf("nullip", "fs"),
  new PubsubNullLeaf("nullmqtt", "ip"),
  //new IpEspLeaf("esp","prefs"),
  //new PubsubEspAsyncMQTTLeaf("espmqtt","prefs"),

  new WireBusLeaf("wire"),
  new INA219Leaf("meter"),
  new OledLeaf("screen"),
  new MeterAppLeaf("app", "meter,screen"),

  NULL
};


// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
