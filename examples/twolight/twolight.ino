#include "stacx/stacx.h"

#include "stacx/leaf_fs_preferences.h"
#include "stacx/leaf_ip_esp.h"
#include "stacx/leaf_pubsub_mqtt_esp.h"
#include "stacx/leaf_shell.h"

#include "stacx/leaf_button.h"
#include "stacx/leaf_light.h"
#include "stacx/leaf_motion.h"

#include "../common/app_lightswitch.h"

Leaf *leaves[] = {
	new FSPreferencesLeaf("prefs"),
	new IpEspLeaf("esp","prefs"),
	new PubsubEspAsyncMQTTLeaf("espmqtt","prefs"),
	new ShellLeaf("shell"),

	new LightLeaf("light1",  "", LEAF_PIN( 14 /* D5 OUT */)),
	new LightLeaf("light2",  "", LEAF_PIN( 13 /* D7 OUT */)),

	NULL
};
