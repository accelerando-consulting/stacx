#include "leaf_fs_preferences.h"
#include "leaf_ip_esp.h"
#include "leaf_pubsub_mqtt_esp.h"
#include "leaf_shell.h"

#include "leaf_lock.h"
#include "leaf_button.h"

#include "leaf_light.h"
#include "leaf_level.h"

//#include "app_injector.h"


Leaf *leaves[] = {
	new FSPreferencesLeaf("prefs"),
	new IpEspLeaf("esp","prefs"),
	new PubsubEspAsyncMQTTLeaf("espmqtt","prefs"),
	new ShellLeaf("shell"),

	new LightLeaf("heater1",   "", LEAF_PIN(14 /* D5 OUT */)),
	new LightLeaf("heater2",   "", LEAF_PIN(12 /* D6 OUT */)),
	new LightLeaf("heater3",   "", LEAF_PIN(13 /* D7 OUT */)),
	new LightLeaf("heater4",   "", LEAF_PIN(15 /* D8 OUT */)),

	//new InjectorAppLeaf("injector", "light@heater1,light@heater2,light@heater3,light@heater4"),

	NULL
};
