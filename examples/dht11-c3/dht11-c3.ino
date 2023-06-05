#pragma STACX_BOARD espressif:esp32:esp32c3
#include "config.h"

#include "stacx.h"
#include "leaf_fs.h"
#include "leaf_fs_preferences.h"
#include "leaf_battery_level.h"
#include "leaf_dht11.h"
#include "leaf_ground.h"
#include "leaf_ip_esp.h"
#include "leaf_pubsub_mqtt_esp.h"
#include "leaf_shell.h"

#include "../common/app_tempdisplay.h"

Leaf *leaves[] = {
  new FSLeaf("fs", FS_DEFAULT, FS_ALLOW_FORMAT),
  new FSPreferencesLeaf("prefs"),
  new ShellLeaf("shell"),
  (new IpEspLeaf("wifi"))->setTrace(L_NOTICE),
  (new PubsubEspAsyncMQTTLeaf("wifimqtt","wifi"))->setTrace(L_NOTICE),

  (new BatteryLevelLeaf("battery", LEAF_PIN(2),100000,100000))->setMute(),

  // Dirty trick to allow DHT 11 module to be plugged into D6,D7,D8 using IO pins as power
  (new Dht11Leaf("dht11", LEAF_PIN(6)))->setMute()->setTrace(L_NOTICE),
  new GroundLeaf("3v", LEAF_PIN(7), HIGH), 
  new GroundLeaf("gnd", LEAF_PIN(10)), 

  (new TempDisplayAppLeaf("app", "dht11,wifi,wifimqtt,battery"))->setTrace(L_NOTICE),
  NULL
};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:


