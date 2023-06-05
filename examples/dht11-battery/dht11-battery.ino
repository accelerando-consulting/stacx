#pragma STACX_BOARD esp8266:esp8266:d1_mini_pro
#include "config.h"
#define HELLO_PIN D4
#define HELLO_ON LOW
#define HELLO_OFF HIGH


#include "stacx.h"
#include "leaf_battery_level.h"
#include "leaf_dht11.h"
#include "leaf_ground.h"
#include "leaf_ip_esp.h"
#include "leaf_pubsub_mqtt_esp.h"
#include "leaf_shell.h"

#include "../common/app_tempdisplay.h"

Leaf *leaves[] = {

  new ShellLeaf("shell"),
  (new BatteryLevelLeaf("battery", A0, 350, 100))->setMute()->setTrace(L_INFO),

  (new IpEspLeaf("wifi"))->setTrace(L_NOTICE),
  (new PubsubEspAsyncMQTTLeaf("wifimqtt","wifi"))->setTrace(L_NOTICE),

  // Dirty trick to allow DHT 11 module to be plugged straight into D3,D4,GND using D4 as +3v
  (new Dht11Leaf("dht11", LEAF_PIN(D6), 0.9, 1.5))->setMute()->setTrace(L_INFO),
  new GroundLeaf("3v", LEAF_PIN(D7), HIGH), 
  new GroundLeaf("gnd", LEAF_PIN(D8)), 
  (new TempDisplayAppLeaf("app", "dht11,wifi,wifimqtt,battery"))->setTrace(L_INFO),
  NULL
};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:


