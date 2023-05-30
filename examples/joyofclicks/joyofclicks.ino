#include "config.h"
#include "stacx.h"

#include "leaf_fs.h"
#include "leaf_fs_preferences.h"
#include "leaf_ip_null.h"
#include "leaf_pubsub_null.h"
//#include "leaf_ip_esp.h"
//#include "leaf_pubsub_mqtt_esp.h"
#include "leaf_shell.h"

#include "abstract_app.h"
#include "leaf_joyofclicks.h"
#include "leaf_pixel.h"
#include "leaf_power.h"
#include "leaf_oled.h"
#include "leaf_wire.h"

#ifdef USE_HELLO_PIXEL
#define PIXEL_COUNT 1
#define PIXEL_PIN 9
Adafruit_NeoPixel pixels(PIXEL_COUNT, PIXEL_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel *helloPixelSetup() {
  pixels.begin();
  return &pixels;
}
#endif


class JoyfulAppLeaf : public AbstractAppLeaf 
{
public:
  JoyfulAppLeaf(String name, String targets)
    : AbstractAppLeaf(name, targets)
    , Debuggable(name, L_INFO)
  {};

};
  

Leaf *leaves[] = {
        new FSLeaf("fs", FS_DEFAULT, FS_ALLOW_FORMAT),
	new FSPreferencesLeaf("prefs"),
	new PowerLeaf("power"),
	new ShellLeaf("shell"),

	//new IpEspLeaf("wifi","fs,prefs"),
	//new PubsubEspAsyncMQTTLeaf("wifimqtt","fs,prefs,wifi",PUBSUB_SSL_DISABLE, PUBSUB_DEVICE_TOPIC_DISABLE),
	new IpNullLeaf("nullip", "fs"),
	new PubsubNullLeaf("nullmqtt", "nullip"),


#ifdef USE_HELLO_PIXEL
	new PixelLeaf("pixel", LEAF_PIN(PIXEL_PIN), PIXEL_COUNT,  0, &pixels),
#endif
	new WireBusLeaf("wire", /*sda=*/SDA, /*scl=*/SCL),
	new JoyOfClicksLeaf("joc", 0x20),
	new OledLeaf("screen", 0x3c),

	new JoyfulAppLeaf("app", "fs,prefs,joc,screen"),
	
	NULL
};
