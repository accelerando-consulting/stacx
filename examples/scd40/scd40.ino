#pragma STACX_BOARD espressif:esp32:ttgo-t7-v13-mini32
#include "config.h"
#define USE_OLED 1
#define OLED_GEOMETRY GEOMETRY_64_48
#include "stacx.h"

//
// Example stack: A setup for the EasyThree IO board
//
// You should define your stack here (or you can incorporate stacx as a git 
// submodule in your project by including stacx/stacx.h
//

#include "leaf_fs.h"
#include "leaf_fs_preferences.h"
#include "leaf_ip_esp.h"
#include "leaf_pubsub_mqtt_esp.h"
#include "leaf_shell.h"
#include "leaf_ip_null.h"
#include "leaf_pubsub_null.h"

#include "leaf_wire.h"
#include "leaf_button.h"
#include "leaf_tone.h"
#include "leaf_actuator.h"
#include "leaf_pixel.h"
#include "abstract_app.h"
#include "leaf_oled.h"
#include "leaf_scd40.h"

#ifdef HELLO_PIXEL
Adafruit_NeoPixel pixels(4, 5, NEO_RGB + NEO_KHZ800); // 4 lights on IO5

Adafruit_NeoPixel *helloPixelSetup() 
{
  pixels.begin();
  return &pixels;
}
#endif

class EasyAppLeaf : public AbstractAppLeaf
{
protected: // configuration preferences

protected: // ephemeral state
  unsigned long last_press=0;

public:
  EasyAppLeaf(String name, String target)
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

  virtual void start(void) 
    {
      message("speaker","set/tempo", "240");
      message("speaker","cmd/tune", "G4 A4 F4:2 R F3 C4:2");
    }

  virtual void loop(void)
  {
    Leaf::loop();
  }

  virtual void status_pub() 
    {
    }

  bool mqtt_receive(String type, String name, String topic, String payload, bool direct=false)
  {
    LEAF_ENTER(L_DEBUG);
    bool handled = false;
    char draw[80];

    LEAF_NOTICE("RECV %s/%s %s <= [%s]", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());

    WHEN("event/press", {
      LEAF_NOTICE("press %s", name.c_str());
    })
    ELSEWHEN("event/release",{
      LEAF_NOTICE("release %s", name.c_str());
    })
    ELSEWHEN("status/temperature", {
	snprintf(draw, sizeof(draw),"{\"f\":24,\"r\":0,\"c\":0,\"t\":\"%sC\"}",payload.c_str());
	message("screen","cmd/draw", draw);
	mqtt_publish("status/temperature", payload);
    })
    ELSEWHEN("status/humidity", {
	snprintf(draw, sizeof(draw),"{\"f\":16,\"r\":22,\"c\":0,\"t\":\"%s%%\"}",payload.c_str());
	mqtt_publish("status/humidity", payload);
	message("screen","cmd/draw", draw);
    })
    ELSEWHEN("status/ppmCO2", {
	int ppm = payload.toInt();
	snprintf(draw, sizeof(draw),"{\"f\":10,\"r\":37,\"c\":0,\"t\":\"%dppm\"}",
		 ppm);
	LEAF_NOTICE("PPM=%d draw %s", ppm, draw);
	message("screen","cmd/draw", draw);
	mqtt_publish("status/ppmCO2", payload);
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
  new ShellLeaf("shell", "Stacx CLI"),

  //new IpNullLeaf("nullip", "fs"),
  //new PubsubNullLeaf("nullmqtt", "ip"),
  new IpEspLeaf("wifi","prefs"),
  new PubsubEspAsyncMQTTLeaf("wifimqtt","prefs,wifi"),
  
  new WireBusLeaf("wire",       /*sda=*/21, /*scl=*/22),
  new ButtonLeaf("sw1",         LEAF_PIN(26 /* D0  IN */), LOW),
  new ButtonLeaf("sw2",         LEAF_PIN(18 /* D5  IN */), LOW),
  new ToneLeaf("speaker",       LEAF_PIN(19 /* D6 OUT */), 800, 100),
  new ActuatorLeaf("rumble","", LEAF_PIN(23 /* D7 out    */)),
#ifdef HELLO_PIXEL
  new PixelLeaf("pixel",        LEAF_PIN( 5 /* D8 out    */), 4, 0, &pixels),
#endif
  (new Scd40Leaf("scd40"))->setMute()->setTrace(L_NOTICE),

#if USE_OLED
  new OledLeaf("screen", 0x3c, SDA, SCL     /* D1,D2 I2C */, GEOMETRY_128_64),
#endif
  (new EasyAppLeaf("app", "wire,sw1,sw2,speaker,rumble,pixel,scd40,screen"))->setTrace(L_INFO),

  NULL
};
