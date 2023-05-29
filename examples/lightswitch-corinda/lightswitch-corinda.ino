#include "config.h"
#include "stacx/stacx.h"

#include "stacx/leaf_fs_preferences.h"
#include "stacx/leaf_ip_null.h"
#include "stacx/leaf_pubsub_null.h"
#include "stacx/leaf_ip_esp.h"
#include "stacx/leaf_pubsub_mqtt_esp.h"
#include "stacx/leaf_shell.h"

#include "stacx/leaf_button.h"
#include "stacx/leaf_light.h"
#include "stacx/leaf_motion.h"
#include "stacx/leaf_pixel.h"
#include "../common/app_lightswitch.h"


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
  new PixelLeaf("pixel",        LEAF_PIN( 9) , 1, 0, &pixels),
  
  new LightswitchAppLeaf("app", "light,button,motion,pixel"),
  NULL
};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
