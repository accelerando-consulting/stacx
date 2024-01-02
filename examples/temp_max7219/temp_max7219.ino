#pragma STACX_BOARD esp8266:esp8266:d1_mini_pro
#include "defaults.h"
#include "config.h"
#define USE_PREFS 0
#define HELLO_PIN D4
#define HELLO_ON LOW
#define HELLO_OFF HIGH
#define IDLE_PATTERN_ONLINE 5000,0

#include "stacx.h"
#include "leaf_ds1820.h"
#include "leaf_ledseg.h"
#include "leaf_ip_esp.h"
#include "leaf_pubsub_mqtt_esp.h"
#include "leaf_shell.h"

#include "abstract_app.h"

class TempLEDAppLeaf : public AbstractAppLeaf
{
protected: // configuration preferences

protected: // ephemeral state
  float temperature=NAN;
  float humidity=NAN;
  Leaf *screen=NULL;
  String string_cmd;
  String clear_cmd;
  String num_cmd;
  

public:
  TempLEDAppLeaf(String name, String target)
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
  }

  void update()
  {
    LEAF_NOTICE("update");
    //cmdf(screen, string_cmd.c_str(), "%.1fC", temperature);
    cmdf(screen, clear_cmd.c_str(), "1");
    cmdf(screen, num_cmd.c_str(), "%d", (int)temperature);
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
    else {
      handled = AbstractAppLeaf::mqtt_receive(type, name, topic, payload, direct);
    }

    LEAF_LEAVE;
    RETURN(handled);
  }

};

Leaf *leaves[] = {
  // Dirty trick to allow DHT 11 module to be plugged straight into D3,D4,GND using D4 as +3v
  new ShellLeaf("shell"),
  new IpEspLeaf("wifi"),
  new PubsubEspAsyncMQTTLeaf("wifimqtt","wifi"),

  
  new Ds1820Leaf("ds1820", LEAF_PIN(D3)),
  new LedSegLeaf("ledseg", ""),
  (new TempLEDAppLeaf("app", "ds1820,ledseg,wifi,wifimqtt"))->setTrace(L_NOTICE),
  NULL
};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:


