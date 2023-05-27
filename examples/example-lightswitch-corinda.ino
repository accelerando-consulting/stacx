#include "config.h"

#undef HELLO_PIXEL

#include "stacx.h"

#include "leaf_fs_preferences.h"
#include "leaf_ip_null.h"
#include "leaf_pubsub_null.h"
#include "leaf_ip_esp.h"
#include "leaf_pubsub_mqtt_esp.h"
#include "leaf_shell.h"

#include "leaf_button.h"
#include "leaf_light.h"
#include "leaf_motion.h"

#include "app_lightswitch.h"
#include "leaf_pixel.h"

#ifdef HELLO_PIXEL
Adafruit_NeoPixel pixels(1, 9, NEO_GRB + NEO_KHZ800); // 1 light on IO9
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
  //new FSLeaf("fs", FS_DEFAULT, FS_ALLOW_FORMAT),
  new FSPreferencesLeaf("prefs"),
  new ShellLeaf("shell"),
  
  new IpEspLeaf("wifi","fs,prefs"),
  new PubsubEspAsyncMQTTLeaf("wifimqtt","fs,prefs,wifi",PUBSUB_SSL_DISABLE, PUBSUB_DEVICE_TOPIC_DISABLE),
  //new IpNullLeaf("nullip", "fs"),
  //new PubsubNullLeaf("nullmqtt", "nullip"),


  new LightLeaf("light",   "app", LEAF_PIN( 4 /* D4 OUT */)),
  new ButtonLeaf("button", LEAF_PIN(3 /* D3  IN */)),
  new MotionLeaf("motion",  LEAF_PIN(8 /* D8  IN */)),
#ifdef HELLO_PIXEL
  new PixelLeaf("pixel",        LEAF_PIN( 9) , 1, 0, &pixels),
#endif
  
  new LightswitchAppLeaf("app", "light,button,motion,pixel"),
  NULL
};
