#include "defaults.h"
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
#include "leaf_tft.h"
#include "leaf_analog.h"
#include "leaf_pixel.h"
#include "leaf_pulsecounter.h"
#include "leaf_rpm.h"

#ifdef PIXEL_GPIO
Adafruit_NeoPixel pixels(PIXEL_COUNT, PIXEL_GPIO, NEO_GRB + NEO_KHZ800);

Adafruit_NeoPixel *helloPixelSetup()
{
#ifdef PIXEL_ENABLE_GPIO
  pinMode(PIXEL_ENABLE_GPIO, OUTPUT);
  digitalWrite(PIXEL_ENABLE_GPIO, HIGH); 
#endif

  // pixel pin is IO8
  pixels.begin();
  pixels.setBrightness(32);
  pixels.clear();
  pixels.show();
  return &pixels;
}
#endif

class MeterAppLeaf : public AbstractAppLeaf
{
protected: // configuration preferences

protected: // ephemeral state
  float volts;
  float amps;
  float vmax;
  float vmin;
  float imax;
  float imin;
  
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

  virtual void status_pub() {
    AbstractAppLeaf::status_pub();

  }

  bool mqtt_receive(String type, String name, String topic, String payload)
  {
    LEAF_ENTER(L_DEBUG);
    bool handled = false;
    

    LEAF_INFO("RECV %s/%s %s <= [%s]", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());

    WHEN("set/state", {
	//state = parseBool(payload));
    })
    ELSEWHEN("event/press", {
      LEAF_NOTICE("press %s", name.c_str());
    })
    ELSEWHEN("event/release",{
      LEAF_NOTICE("release %s", name.c_str());
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

  new TFTLeaf("screen",0),
  new WireBusLeaf("wire"),
  new INA219Leaf("meter"),

  // you might think we only need rise or fall here but those interrupt handlers
  // don't handle switches that bounce on disengage
#ifdef PULSE_GPIO  
  new PulseCounterLeaf("pulse", LEAF_PIN(PULSE_GPIO), CHANGE, true),
  //new RPMInputLeaf("pulse", LEAF_PIN(PULSE_GPIO), true),
#endif
#ifdef PULSE2_GPIO  
  new PulseCounterLeaf("pulse2", LEAF_PIN(PULSE2_GPIO), CHANGE, true),
#endif
#ifdef ANALOG_GPIO  
  new AnalogInputLeaf("analog", LEAF_PIN(ANALOG_GPIO)),
#endif
  
  new MeterAppLeaf("app", "screen,meter,wire"
#ifdef ANALOG_GPIO
		   ",analog"
#endif
#ifdef PULSE_GPIO		   
		   ",pulse"
#endif
#ifdef PULSE2_GPIO		   
		   ",pulse2"
#endif
),

  NULL
};


// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
