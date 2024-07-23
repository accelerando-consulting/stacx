#include "config.h"
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
#include "leaf_joyofclicks.h"

#include "leaf_wire.h"
#include "leaf_button.h"
#include "leaf_tone.h"
#include "leaf_actuator.h"
#include "leaf_pixel.h"
#include "abstract_app.h"
#include "leaf_oled.h"
#include "leaf_scd40.h"

#ifdef HELLO_PIXEL
Adafruit_NeoPixel pixels(1, 9, NEO_RGB + NEO_KHZ800); // 1 light on IO9

Adafruit_NeoPixel *helloPixelSetup() 
{
  pixels.begin();
  return &pixels;
}
#endif

class CobberAppLeaf : public AbstractAppLeaf
{
protected: // configuration preferences

protected: // ephemeral state
  unsigned long last_press=0;

public:
  CobberAppLeaf(String name, String target)
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
      AbstractAppLeaf::start();
    }

  virtual void loop(void)
  {
    AbstractAppLeaf::loop();

    static unsigned long last_update = 0;
    unsigned long now = millis();
    if (now > (last_update+1000)) {
      last_update = now;
      time_t now_sec = time(NULL);
      struct tm tm_now;
      localtime_r(&now_sec, &tm_now);
      char date[24];
      strftime(date, sizeof(date), "%a %d %b %T", &tm_now);
      char draw[80];
      //LEAF_NOTICE("DATE: %s", date);
      snprintf(draw, sizeof(draw),"{\"f\":10,\"r\":38,\"c\":0,\"t\":\"%s\"}",date);
      message("screen","cmd/draw", draw);
    }
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
	snprintf(draw, sizeof(draw),"{\"f\":16,\"r\":22,\"c\":0,\"t\":\"%sC\"}",payload.c_str());
	message("screen","cmd/draw", draw);
	mqtt_publish("status/temperature", payload);
    })
    ELSEWHEN("status/humidity", {
	snprintf(draw, sizeof(draw),"{\"f\":16,\"r\":22,\"c\":64,\"t\":\"%s%%\"}",payload.c_str());
	mqtt_publish("status/humidity", payload);
	message("screen","cmd/draw", draw);
    })
    ELSEWHEN("status/ppmCO2", {
	int ppm = payload.toInt();
	snprintf(draw, sizeof(draw),"{\"f\":24,\"r\":0,\"c\":0,\"t\":\"%d ppm\"}",
		 ppm);
	LEAF_NOTICE("PPM=%d draw %s", ppm, draw);
	message("screen","cmd/draw", draw);
	mqtt_publish("status/ppmCO2", payload);
      })
    else {
      handled = AbstractAppLeaf::mqtt_receive(type, name, topic, payload);
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
  
  new WireBusLeaf("wire", SDA, SCL),
#ifdef HELLO_PIXEL
  new PixelLeaf("pixel",        LEAF_PIN( 9 /* D8 out    */), 1, 0, &pixels),
#endif
  (new Scd40Leaf("scd40"))->setMute(),

#if USE_OLED
  new OledLeaf("screen", 0x3c, SDA, SCL     /* D1,D2 I2C */, GEOMETRY_128_64),
#endif
  (new JoyOfClicksLeaf("joyhat",0x20))->setMute(1),
 
  (new CobberAppLeaf("app", "wire,pixel,joyhat,screen,scd40")),

  NULL
};
