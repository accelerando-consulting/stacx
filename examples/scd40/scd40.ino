#include "config.h"
#define HELLO_PIXEL 0
#include "stacx/stacx.h"



//
// Example stack: A setup for the EasyThree IO board
//
// You should define your stack here (or you can incorporate stacx as a git 
// submodule in your project by including stacx/stacx.h
//

#include "stacx/leaf_fs.h"
#include "stacx/leaf_fs_preferences.h"
#include "stacx/leaf_ip_esp.h"
#include "stacx/leaf_pubsub_mqtt_esp.h"
#include "stacx/leaf_shell.h"
#include "stacx/leaf_ip_null.h"
#include "stacx/leaf_pubsub_null.h"

#include "stacx/leaf_wire.h"
#include "stacx/leaf_button.h"
#include "stacx/leaf_tone.h"
#include "stacx/leaf_actuator.h"
#include "stacx/leaf_pixel.h"
#include "stacx/abstract_app.h"
#include "stacx/leaf_oled.h"
#include "stacx/leaf_scd40.h"

#ifdef HELLO_PIXEL
Adafruit_NeoPixel pixels(4, 5, NEO_RGB + NEO_KHZ800); // 4 lights on IO5

Adafruit_NeoPixel *helloPixelSetup() 
{
#if EARLY_SERIAL
  Serial.printf("%d Start boot animation\n", (int)millis());
#endif  
  pixels.begin();

  pixels.setBrightness(255);
  pixels.clear();
  pixels.show();

  // rotate the pixels through RGBW 8 times, shifting the pattern each time
  int count = pixels.numPixels();
  for (int cycle=0; cycle<8; cycle++) {
    for (int pixel=0; pixel<count; pixel++) {
      pixels.clear();
      switch (cycle%4) {
      case 0:
	pixels.setPixelColor(pixel, pixels.Color(255,0,0));
	break;
      case 1:
	pixels.setPixelColor(pixel, pixels.Color(0,255,0));
	break;
      case 2:
	pixels.setPixelColor(pixel, pixels.Color(0,0,255));
	break;
      case 3:
	pixels.setPixelColor(pixel, pixels.Color(255,255,255));
	break;
      }
      pixels.show();
      delay(50);
    }
    pixels.clear();
    pixels.show();
  }
#if EARLY_SERIAL
  Serial.printf("%d Finish boot animation\n", (int)millis());
#endif  
  return &pixels;
}
#endif

class EasyAppLeaf : public AbstractAppLeaf
{
protected: // configuration preferences

protected: // ephemeral state
  bool state = false;
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
      publish("state", ABILITY(state));
    }

  bool mqtt_receive(String type, String name, String topic, String payload)
  {
    LEAF_ENTER(L_DEBUG);
    bool handled = Leaf::mqtt_receive(type, name, topic, payload);

    LEAF_INFO("RECV %s/%s %s <= [%s]", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());

    WHEN("set/state", {
      state = parseBool(payload));
    }
    ELSEWHEN("event/press", {
      LEAF_NOTICE("press %s", name.c_str());
    })
    ELSEWHEN("event/release",{
      LEAF_NOTICE("release %s", name.c_str());
    })
    else {
      LEAF_DEBUG("app did not consume type=%s name=%s topic=%s payload=%s", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());
    }

    LEAF_LEAVE;
    RETURN(handled);
  }

};

Leaf *leaves[] = {
  new FSLeaf("fs", FS_DEFAULT, FS_ALLOW_FORMAT),
  new FSPreferencesLeaf("prefs"),
  new ShellLeaf("shell", "Stacx CLI"),

  new IpNullLeaf("nullip", "fs"),
  new PubsubNullLeaf("nullmqtt", "ip"),
  //new IpEspLeaf("wifi","prefs"),
  //new PubsubEspAsyncMQTTLeaf("wifimqtt","prefs"),
  
  new WireBusLeaf("wire",       /*sda=*/21, /*scl=*/22),
  new ButtonLeaf("sw1",         LEAF_PIN(26 /* D0  IN */), LOW),
  new ButtonLeaf("sw2",         LEAF_PIN(18 /* D5  IN */), LOW),
  new ToneLeaf("speaker",       LEAF_PIN(19 /* D6 OUT */), 800, 100),
  new ActuatorLeaf("rumble","", LEAF_PIN(23 /* D7 out    */)),
#ifdef HELLO_PIXEL
  new PixelLeaf("pixel",        LEAF_PIN( 5 /* D8 out    */), 4, 0, &pixels),
#endif
  (new Scd40Leaf("scd40"))->setMute()->inhibit(),


#if USE_OLED
  new OledLeaf("screen", 0x3c, SDA, SCL     /* D1,D2 I2C */, GEOMETRY_128_64),
#endif
  new EasyAppLeaf("app", "wire,sw1,sw2,speaker,rumble,pixel,scd40"),

  NULL
};
