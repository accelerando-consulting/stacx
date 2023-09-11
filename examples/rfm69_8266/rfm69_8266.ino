#pragma STACX_BOARD esp8266:esp8266:d1_mini_pro

#include "variant_pins.h"

#undef HELLO_PIXEL
#define HELLO_PIN 22
#include "defaults.h"
#include "config.h"
#include "stacx.h"

//
// Example stack: A modbus satellite (aka sl*v*) device
//
//

#include "leaf_fs.h"
#include "leaf_fs_preferences.h"
#include "leaf_ip_esp.h"
#include "leaf_pubsub_mqtt_esp.h"
#include "leaf_shell.h"
#include "leaf_ip_null.h"
#include "leaf_pubsub_null.h"

#include "leaf_rfm69.h"
#include "abstract_app.h"


Leaf *leaves[] = {
  new FSLeaf("fs", FS_DEFAULT, FS_ALLOW_FORMAT),
  new FSPreferencesLeaf("prefs"),
  new ShellLeaf("shell", "Stacx CLI"),

  new IpNullLeaf("nullip", "fs"),
  new PubsubNullLeaf("nullmqtt", "nullip,fs"),

  new Rfm69Leaf("radio", LEAF_PIN(D8), D4),
  NULL
};
