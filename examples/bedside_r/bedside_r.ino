#include "config.h"
#undef HELLO_PIXEL
#define USE_PREFS 0
#define USE_OTA 0
#define USE_FTP 0

#include "stacx/stacx.h"

//#include "stacx/leaf_fs.h"
#include "stacx/leaf_fs_preferences.h"
//#include "stacx/leaf_ip_null.h"
//#include "stacx/leaf_pubsub_null.h"
#include "stacx/leaf_ip_esp.h"
#include "stacx/leaf_pubsub_mqtt_esp.h"
#include "stacx/leaf_shell.h"

#include "stacx/leaf_button.h"
#include "stacx/leaf_light.h"
#include "stacx/leaf_actuator.h"
#include "stacx/leaf_motion.h"

#include "../common/app_lightswitch.h"

Leaf *leaves[] = {
  //new FSLeaf("fs", FS_DEFAULT, FS_ALLOW_FORMAT),
#if USE_PREFS
  new FSPreferencesLeaf("prefs"),
#endif // USE_PREFS
  new ShellLeaf("shell"),
  
  (new IpEspLeaf("wifi"))->setTrace(L_NOTICE),
  new PubsubEspAsyncMQTTLeaf("wifimqtt","wifi"),
  //new IpNullLeaf("nullip", NO_TAPS),
  //new PubsubNullLeaf("nullmqtt", "nullip"),


  (new LightLeaf("light",   NO_TAPS, LEAF_PIN( 0 /* D3 OUT */)))->setMute()->setTrace(L_INFO),
  (new ActuatorLeaf("indicator",   NO_TAPS, LEAF_PIN( 2 /* D4 OUT */),false,true))->setMute(),
  (new ButtonLeaf("button", LEAF_PIN(12 /* D6  IN */), HIGH, false))->setMute()->setTrace(L_NOTICE),
  //new MotionLeaf("motion",  LEAF_PIN(15 /* D8  IN */)),
  
  (new LightswitchAppLeaf("app", "light,button,motion,indicator"))->setTrace(L_NOTICE),
  
  NULL
};
