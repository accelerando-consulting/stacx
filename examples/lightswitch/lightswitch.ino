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
	new IpEspLeaf("wifi","prefs"),
	new PubsubEspAsyncMQTTLeaf("wifimqtt","prefs,wifi"),
	new ShellLeaf("shell"),

	new LightLeaf("light",  "app", LEAF_PIN( 5 /* D1 OUT */)),
	new ButtonLeaf("button", LEAF_PIN( 0 /* D3  IN */), HIGH),
	new MotionLeaf("motion", LEAF_PIN( 2 /* D4  IN */)),

	new LightswitchAppLeaf("app", "light,button,motion"),
	NULL
};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:


