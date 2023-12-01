#pragma STACX_BOARD esp8266:esp8266:d1_mini_pro

#include "defaults.h"
#include "config.h"
#define USE_PREFS 0
#define HELLO_PIN D4
#define HELLO_ON LOW
#define HELLO_OFF HIGH
#define IDLE_PATTERN_ONLINE 1000,50

#include "stacx.h"
#include "leaf_ds1820.h"
#include "leaf_ip_null.h"
#include "leaf_pubsub_null.h"
#include "leaf_shell.h"


Leaf *leaves[] = {
  // Dirty trick to allow DHT 11 module to be plugged straight into D3,D4,GND using D4 as +3v
  //new ShellLeaf("shell"),
  new IpNullLeaf("nullip",""),
  new PubsubNullLeaf("nullmqtt","nullip"),

  new Ds1820Leaf("ds1820", LEAF_PIN(0)),
  NULL
};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:


