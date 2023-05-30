#include "config.h"
#include "stacx.h"

#include "leaf_fs_preferences.h"
#include "leaf_ip_esp.h"
#include "leaf_pubsub_mqtt_esp.h"
#include "leaf_shell.h"

#include "leaf_button.h"
#include "leaf_light.h"
#include "leaf_pixel.h"

#include "../common/app_lightswitch.h"


Adafruit_NeoPixel pixels(1, 0, NEO_GRB + NEO_KHZ800);

#ifdef helloPixel

#error nope

Adafruit_NeoPixel *helloPixelSetup() 
{
  pixels.begin();
  pixels.setBrightness(255);
  pixels.clear();
  pixels.show();
  return &pixels;
}
#endif

Leaf *leaves[] = {
	new FSPreferencesLeaf("prefs"),
	new IpEspLeaf("wifi","prefs"),
	new PubsubEspAsyncMQTTLeaf("wifimqtt","prefs,wifi"),
	new ShellLeaf("shell"),

	(new LightLeaf("light",  "app", LEAF_PIN( 5 /* D1 OUT */)))->setMute(),
	(new ButtonLeaf("button", LEAF_PIN( 4 /* D2  IN */), HIGH))->setMute(),
	new PixelLeaf("pixel", LEAF_PIN( 0 /* D3 out */), 1,Adafruit_NeoPixel::Color(64,0,0), &pixels),

	new LightswitchAppLeaf("app", "light,button,pixel"),
	NULL
};
