#define BUILD_NUMBER 1
#include "stacx.h"

#include "leaf_fs.h"
#include "leaf_fs_preferences.h"
#include "leaf_ip_null.h"
#include "leaf_pubsub_null.h"
#include "leaf_wire.h"
#include "leaf_ina219.h"
#include "leaf_shell.h"


Leaf *leaves[] = {
  //new FSLeaf("fs", FS_DEFAULT, FS_ALLOW_FORMAT),
  //new FSPreferencesLeaf("prefs"),
  new ShellLeaf("shell"),
  new IpNullLeaf("nullip", "fs"),
  new PubsubNullLeaf("nullmqtt", "ip"),

  new WireBusLeaf("wire"),
  new INA219Leaf("battery"),
  NULL
};
