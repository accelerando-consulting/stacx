#include "config.h"
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



Leaf *leaves[] = {
  //new FSLeaf("fs", FS_DEFAULT, FS_ALLOW_FORMAT),
  new FSPreferencesLeaf("prefs"),
  new ShellLeaf("shell"),
  
  new IpEspLeaf("wifi","fs,prefs"),
  new PubsubEspAsyncMQTTLeaf("wifimqtt","fs,prefs,wifi",PUBSUB_SSL_DISABLE, PUBSUB_DEVICE_TOPIC_DISABLE),
  //new IpNullLeaf("nullip", "fs"),
  //new PubsubNullLeaf("nullmqtt", "nullip"),


  new LightLeaf("light",   "", LEAF_PIN( 16 /* D4 OUT */)),
  new ButtonLeaf("button", LEAF_PIN(19 /* D6  IN */)),
  new MotionLeaf("motion",  LEAF_PIN(5 /* D8  IN */)),
  
  new LightswitchAppLeaf("app", "light,button,motion"),
  NULL
};
