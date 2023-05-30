#pragma STACX_BOARD esp8266:esp8266:d1_mini_pro
#include "config.h"
#undef HELLO_PIXEL
#define USE_PREFS 0
#define USE_OTA 0
#define USE_FTP 0

#include "stacx.h"

//#include "leaf_fs.h"
#include "leaf_fs_preferences.h"
//#include "leaf_ip_null.h"
//#include "leaf_pubsub_null.h"
#include "leaf_ip_esp.h"
#include "leaf_pubsub_mqtt_esp.h"
#include "leaf_shell.h"

#include "leaf_button.h"
#include "leaf_light.h"
#include "leaf_actuator.h"
#include "leaf_motion.h"

#include "app_lightswitch.h"

Leaf *leaves[] = {
  //new FSLeaf("fs", FS_DEFAULT, FS_ALLOW_FORMAT),
#if USE_PREFS
  new FSPreferencesLeaf("prefs"),
#endif // USE_PREFS
  new ShellLeaf("shell"),
  
  (new IpEspLeaf("wifi"))->setTrace(L_NOTICE),
  (new PubsubEspAsyncMQTTLeaf("wifimqtt","wifi"))->setTrace(L_NOTICE),
  //new IpNullLeaf("nullip", NO_TAPS),
  //new PubsubNullLeaf("nullmqtt", "nullip"),


  (new LightLeaf("light",   NO_TAPS, LEAF_PIN( 0 /* D3 OUT */)))->setMute()->setTrace(L_NOTICE),
  (new ActuatorLeaf("indicator",   NO_TAPS, LEAF_PIN( 2 /* D4 OUT */),false,true))->setMute(),
  (new ButtonLeaf("button", LEAF_PIN(12 /* D6  IN */)))->setMute()->setTrace(L_NOTICE),
  new MotionLeaf("motion",  LEAF_PIN(15 /* D8  IN */)),
  
  (new LightswitchAppLeaf("app", "light,button,motion,indicator"))->setTrace(L_NOTICE),
  
  NULL
};
