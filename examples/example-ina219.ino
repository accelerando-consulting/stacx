#define BUILD_NUMBER 1
#define USE_TFT 1
#include "stacx.h"

#include "leaf_preferences.h"
#include "leaf_ip_esp.h"
#include "leaf_pubsub_mqtt_esp.h"
#include "leaf_wire.h"
#include "leaf_ina219.h"
#include "leaf_vl53l1x.h"
#include "leaf_shell.h"


Leaf *leaves[] = {
	new PreferencesLeaf("prefs"),
	new IpEspLeaf("esp","prefs"),
	new PubsubEspAsyncMQTTLeaf("espmqtt","prefs"),
	new ShellLeaf("shell"),
	new WireBusLeaf("wire"),
	//new Vl53l1xLeaf("lidar"),

	new INA219Leaf("battery"),
	NULL
};
