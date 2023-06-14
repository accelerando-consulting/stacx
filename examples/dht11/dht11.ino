#pragma STACX_BOARD esp8266:esp8266:d1_mini_pro
#include "config.h"
#define USE_PREFS 0
#define HELLO_PIN D4
#define HELLO_ON LOW
#define HELLO_OFF HIGH
#define IDLE_PATTERN_ONLINE 5000,0

#include "stacx.h"
#include "leaf_dht11.h"
#include "leaf_ground.h"
#include "leaf_ip_esp.h"
#include "leaf_pubsub_mqtt_esp.h"
#include "leaf_shell.h"

#include "../common/app_tempdisplay.h"

Leaf *leaves[] = {
  // Dirty trick to allow DHT 11 module to be plugged straight into D3,D4,GND using D4 as +3v
  new ShellLeaf("shell"),
  (new IpEspLeaf("wifi")),
  (new PubsubEspAsyncMQTTLeaf("wifimqtt","wifi")),

  (new Dht11Leaf("dht11", LEAF_PIN(D6)))->setMute()->setTrace(L_NOTICE),
  new GroundLeaf("3v", LEAF_PIN(D7), HIGH), 
  new GroundLeaf("gnd", LEAF_PIN(D8)), 
  (new TempDisplayAppLeaf("app", "dht11,wifi,wifimqtt"))->setTrace(L_NOTICE),
  NULL
};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:


