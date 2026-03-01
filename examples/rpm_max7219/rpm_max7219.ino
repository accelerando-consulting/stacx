#pragma STACX_BOARD espressif:esp32:d1_mini32
#include "defaults.h"
#include "config.h"

#include "stacx.h"
#include "leaf_fs.h"
#include "leaf_fs_preferences.h"
#include "leaf_rpm.h"
#include "leaf_ledseg.h"
#include "leaf_ip_null.h"
#include "leaf_pubsub_null.h"
#include "leaf_shell.h"

#include "abstract_app.h"

class RpmLEDAppLeaf : public AbstractAppLeaf
{
protected: // configuration preferences
  int ppr = 1;

protected: // ephemeral state
  float rpm;
  Leaf *screen=NULL;
  String string_cmd;
  String clear_cmd;
  String num_cmd;
  
public:
  RpmLEDAppLeaf(String name, String target)
    : AbstractAppLeaf(name,target)
    , Debuggable(name, L_INFO)
 {
 }

  virtual void setup(void) {
    AbstractAppLeaf::setup();
    LEAF_ENTER(L_INFO);
    screen = get_tap("ledseg");
    if (!screen) {
      LEAF_WARN("Did not obtain screen handle");
    }
    else {
      string_cmd = screen->getName()+"_string";
      num_cmd = screen->getName()+"_num";
      clear_cmd = screen->getName()+"_clear";
    }
    LEAF_LEAVE;
  }

  virtual void start(void) {
    AbstractAppLeaf::start();
    cmdf(screen, string_cmd.c_str(), "%s", "dead");
  }

  virtual void loop(void)
  {
    AbstractAppLeaf::loop();
  }

  virtual void status_pub() 
  {
    if (!isnan(rpm)) {
      mqtt_publish("status/rpm", String(rpm, 2));
    }
  }

  void update()
  {
    LEAF_NOTICE("update");
    //cmdf(screen, string_cmd.c_str(), "%.1fC", temperature);
    cmdf(screen, clear_cmd.c_str(), "1");
    cmdf(screen, num_cmd.c_str(), "%d", (int)rpm);
  }

  virtual bool mqtt_receive(String type, String name, String topic, String payload, bool direct=false)
  {
    LEAF_ENTER(L_DEBUG);
    bool handled = false;

    LEAF_INFO("RECV %s %s %s %s", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());

    WHEN("status/rpm", {
	rpm=payload.toFloat()/ppr;
	if (screen) update();
	mqtt_publish("status/rpm", payload);
      })
    else {
      handled = AbstractAppLeaf::mqtt_receive(type, name, topic, payload, direct);
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
  
  new RPMInputLeaf("rpm", LEAF_PIN(36)),
  new LedSegLeaf("ledseg", "", 4, 19, 5, 23),
  (new RpmLEDAppLeaf("app", "rpm,ledseg"))->setTrace(L_NOTICE),
  NULL
};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:


