#include "stacx.h"

#include "leaf_fs_preferences.h"
#include "leaf_ip_esp.h"
#include "leaf_pubsub_mqtt_esp.h"
#include "leaf_shell.h"

#include "leaf_button.h"
#include "leaf_light.h"
#include "leaf_motion.h"
#include "../common/app_lightswitch.h"

Leaf *leaves[] = {
#ifdef USE_PREFS
	new FSPreferencesLeaf("prefs"),
#endif
	new IpEspLeaf("wifi","prefs"),
	new PubsubEspAsyncMQTTLeaf("wifimqtt","prefs,wifi"),
	new ShellLeaf("shell"),

	new ActuatorLeaf("actuator",  "app", LEAF_PIN(D1)),
	NULL
};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:


