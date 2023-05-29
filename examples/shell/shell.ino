#include "stacx/stacx.h"

#include "stacx/leaf_fs_preferences.h"
#include "stacx/leaf_ip_esp.h"
#include "stacx/leaf_pubsub_mqtt_esp.h"
#include "stacx/leaf_shell.h"

Leaf *leaves[] = {
	new FSPreferencesLeaf("prefs"),
	new IpEspLeaf("esp","prefs"),
	new PubsubEspAsyncMQTTLeaf("espmqtt","prefs"),
	new ShellLeaf("shell"),
	NULL
};
