#ifdef ESP8266
#include "leaf_fs_preferences.h"
#else
#include "leaf_preferences.h"
#endif
#include "leaf_ip_esp.h"
#include "leaf_oled.h"
#include "leaf_pubsub_mqtt_esp.h"
#include "leaf_shell.h"
#include "leaf_analog.h"

#include "leaf_button.h"

#include "leaf_light.h"

#include "app_injector.h"


Leaf *leaves[] = {
#ifdef ESP8266
	new FSPreferencesLeaf("prefs"),
#else
	new PreferencesLeaf("prefs"),
#endif
	
	new IpEspLeaf("esp"),
	new PubsubEspAsyncMQTTLeaf("espmqtt"),
	new ShellLeaf("shell"),
#ifdef USE_OLED
	new OledLeaf("screen"),
#endif
	new LightLeaf("heater1",   "", LEAF_PIN(14)), // D5 OUT
	new LightLeaf("heater2",   "", LEAF_PIN(12)), // D6 OUT
	new LightLeaf("heater3",   "", LEAF_PIN(13)), // D7 OUT
	new LightLeaf("heater4",   "", LEAF_PIN(15)), // D8 OUT
#ifdef ESP8266
	new AnalogInputLeaf("ntc"    , NO_PINS), // implicitly A0 on esp8266
#else
	new AnalogInputLeaf("ntc"    , LEAF_PIN(36)),
#endif

	new InjectorAppLeaf("injector", "ntc,screen,light@heater1,light@heater2,light@heater3,light@heater4"),

	NULL
};
