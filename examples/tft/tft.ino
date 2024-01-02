#pragma STACX_BOARD espressif:esp32:esp32
#include "config.h"
#include "stacx.h"

#include "leaf_fs.h"
#include "leaf_fs_preferences.h"
#include "leaf_shell.h"
#include "leaf_ip_null.h"
#include "leaf_pubsub_null.h"
#include "leaf_tft.h"

Leaf *leaves[] = {
  new FSLeaf("fs", FS_DEFAULT, FS_ALLOW_FORMAT),
  new FSPreferencesLeaf("prefs"),
  new ShellLeaf("shell", "Stacx CLI"),

  new IpNullLeaf("nullip", "fs"),
  new PubsubNullLeaf("nullmqtt", "ip"),
  
  new TFTLeaf("screen", 3),
  NULL
};
