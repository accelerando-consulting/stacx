#include "config.h"
#include "stacx.h"


//
// Example stack: A smart light switch which operates autonomously, but also
//                interacts with a building controller via MQTT.
//
// You should define your stack here (or you can incorporate stacx as a git 
// submodule in your project by including stacx/stacx.h
//

#include "leaf_fs_preferences.h"
#include "leaf_ip_esp.h"
#include "leaf_pubsub_mqtt_esp.h"
#include "leaf_shell.h"

#include "leaf_button.h"
#include "leaf_light.h"
#include "leaf_motion.h"

#include "app_lightswitch.h"

Leaf *leaves[] = {
	new FSPreferencesLeaf("prefs"),
	new IpEspLeaf("wifi","prefs"),
	new PubsubEspAsyncMQTTLeaf("wifimqtt","prefs"),
	new ShellLeaf("shell"),

	new LightLeaf("light",  "app", LEAF_PIN( 5 /* D1 OUT */), true),
	new ButtonLeaf("button", LEAF_PIN( 4 /* D2  IN */), HIGH),
	new MotionLeaf("motion", LEAF_PIN( 0 /* D3  IN */)),

	new LightswitchAppLeaf("app", "light,button,motion"),
	NULL
};
