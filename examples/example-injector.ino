// Configuration defaults for Stacx
//
// Preferences can be set either by editing here, using the persistent
// storage mechanism, or via the captive-portal setup process.
//

#define BUILD_NUMBER 47

#define DEBUG_LEVEL 1
#define MAX_DEBUG_LEVEL 4
#define DEBUG_FLUSH true
#define DEBUG_LINES true

#define DEVICE_ID "oozor"

#ifdef ESP8266
#define helloPin 5
#else
#define helloPin 22
#endif

#define HELLO_ON 1
#define HELLO_OFF 0

//#define APP_TOPIC "oozor"
#define DEVICE_ID_APPEND_MAC 1
//#define DEVICE_ID_PREFERENCE_GROUP "control"
//#define DEVICE_ID_PREFERENCE_KEY "sensor"
//#define USE_FLAT_TOPIC true
#define USE_WILDCARD_TOPIC true
//#define NETWORK_RECONNECT_SECONDS 30

//#define MQTT_HOST "mqtt.lan"
//#define MQTT_PORT "1883"
//#define MQTT_USER ""
//#define MQTT_PASS ""

#define OTA_PASSWORD "changeme"
#define UPDATE_URL "http://firmware.accelerando.io/stacx/stacx.bin"

#define USE_OLED 1
//#define OLED_GEOMETRY GEOMETRY_64_48
//#define USE_NTP 1
//#define USE_BT_CONSOLE 0
//#define SHELL_DELAY 1000
//#define SHELL_DELAY_COLD 2000

#define SYSLOG_IP 255,255,255,255
//#define SYSLOG_IP 10,1,1,2
#define SYSLOG_flag wifiConnected

//#define SETTINGS_CLEAR_PIN 12
#include "leaf_preferences.h"
#include "leaf_ip_esp.h"
#include "leaf_oled.h"
#include "leaf_pubsub_mqtt_esp.h"
#include "leaf_shell.h"
#include "leaf_wire.h"
#include "leaf_max6675.h"
#include "leaf_pcf8574.h"
//#include "leaf_analog.h"
//#include "leaf_analog_ac.h"

#include "leaf_button.h"

#include "leaf_light.h"

#include "app_injector.h"


Leaf *leaves[] = {
	new PreferencesLeaf("prefs"),
	new IpEspLeaf("esp"),
	new PubsubEspAsyncMQTTLeaf("espmqtt"),
	new ShellLeaf("shell"),
	new WireBusLeaf("wire", /*sda=*/14, /*scl=*/13),
	//new OledLeaf("screen"),
	//new LightLeaf(      "heater1",   "", LEAF_PIN(19), LightLeaf::PERSIST_OFF, Leaf::PIN_INVERT), // D5 OUT
	//new LightLeaf(      "heater2",   "", LEAF_PIN(18), LightLeaf::PERSIST_OFF, Leaf::PIN_INVERT), // D6 OUT
	//new LightLeaf(      "heater3",   "", LEAF_PIN( 5), LightLeaf::PERSIST_OFF, Leaf::PIN_INVERT), // D7 OUT
	//new LightLeaf(      "heater4",   "", LEAF_PIN(17),
	//LightLeaf::PERSIST_OFF, Leaf::PIN_INVERT), // D8 OUT
	//new Max6675Leaf(    "thermocouple", 26),
	new PCF8574Leaf(    "heaters", 0x41),
	//new AnalogInputLeaf("ntc"    ,       LEAF_PIN(36), 0, 4095, 0, 100),
	//new AnalogACLeaf(   "amps"   ,       LEAF_PIN(39)),
	//new InjectorAppLeaf("injector", "thermocouple,heaters"),
	//new InjectorAppLeaf("injector", "ntc,amps,light@heater1,light@heater2,light@heater3,light@heater4"),

	NULL
};
