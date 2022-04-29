#include "stacx.h"

// Barometer and thermometer using Bosch BMP180 sensor

#include "leaf_fs_preferences.h"
#include "leaf_ip_esp.h"
#include "leaf_pubsub_mqtt_esp.h"
#include "leaf_bmp180.h"
#include "leaf_shell.h"
#include "leaf_oled.h"

#include "app_tempdisplay.h"

Leaf *leaves[] = {
	new FSPreferencesLeaf("prefs"),
	new IpEspLeaf("esp","prefs"),
	new PubsubEspAsyncMQTTLeaf("espmqtt","prefs"),
	new ShellLeaf("shell"),
	new Bmp180Leaf("sensor", 0),
	new OledLeaf("screen", 0x3c, D1, D2, GEOMETRY_64_48),
	new TempDisplayAppLeaf("app", "sensor,screen"),
	NULL
};
