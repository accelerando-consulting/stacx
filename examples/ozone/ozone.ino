#include "stacx.h"

#include "leaf_spiffs_preferences.h"
#include "leaf_ip_esp.h"
#include "leaf_pubsub_mqtt_esp.h"
#include "leaf_analog.h"
#include "leaf_shell.h"

Leaf *leaves[] = {
	new SPIFFSPreferencesLeaf("prefs"),
	new IpEspLeaf("esp","prefs"),
	new PubsubEspAsyncMQTTLeaf("espmqtt","prefs"),
	new ShellLeaf("shell"),
	new AnalogInputLeaf("ozone", LEAF_PIN(A0), 0, 1023, 0, 0.33),
  NULL
};
