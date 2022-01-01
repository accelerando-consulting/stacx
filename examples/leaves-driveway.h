#include "leaf_fs_preferences.h"
#include "leaf_ip_esp.h"
#include "leaf_pubsub_mqtt_esp.h"
#include "leaf_shell.h"

#include "leaf_lock.h"
#include "leaf_contact.h"
#include "leaf_button.h"

#include "leaf_light.h"
#include "leaf_motion.h"
#include "leaf_level.h"

#include "app_driveway.h"

Leaf *leaves[] = {
	new FSPreferencesLeaf("prefs"),
	new IpEspLeaf("esp","prefs"),
	new PubsubEspAsyncMQTTLeaf("espmqtt","prefs"),
	new ShellLeaf("shell"),

	new MotionLeaf("driveway", LEAF_PIN(16 /* D0  IN */)),
	new LightLeaf("driveway",   "", LEAF_PIN(14 /* D5 OUT */)),
	new LevelLeaf("daylight",  LEAF_PIN(15 /* D8  IN */)), // light level

	new DrivewayAppLeaf("driveway", "light@driveway,level@daylight,motion@driveway"),

	NULL
};
