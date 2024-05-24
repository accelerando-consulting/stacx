#include "defaults.h"
#include "config.h"
#include "stacx.h"

#include "leaf_fs_preferences.h"
#include "leaf_ip_esp.h"
#include "leaf_pubsub_mqtt_esp.h"
#include "leaf_shell.h"

#ifdef USE_HELLO_PIXEL
Adafruit_NeoPixel pixels(1, HELLO_PIXEL, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel *helloPixelSetup() { pixels.begin(); return &pixels; }
#endif

Leaf *leaves[] = {
	new FSPreferencesLeaf("prefs"),
	new IpEspLeaf("esp","prefs"),
	new PubsubEspAsyncMQTTLeaf("espmqtt","prefs"),
	new ShellLeaf("shell"),
	NULL
};
