// Configuration defaults for Stacx
//
// Preferences can be set either by editing here, using the persistent
// storage mechanism, or via the captive-portal setup process.
//

#define BUILD_NUMBER 45

#define DEBUG_LEVEL 1
#define MAX_DEBUG_LEVEL 3

#define DEVICE_ID "yournamehere"

#ifdef ESP8266
#define helloPin 5
#else
#define helloPin 22
#endif

#define HELLO_ON 1
#define HELLO_OFF 0

//#define APP_TOPIC "myapp"
//#define DEVICE_ID_APPEND_MAC 1
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

//#define USE_OLED 1
//#define USE_NTP 1
//#define USE_BT_CONSOLE 0
//#define SHELL_DELAY 1000
//#define SHELL_DELAY_COLD 2000

#define SYSLOG_IP 255,255,255,255
//#define SYSLOG_IP 10,1,1,2
#define SYSLOG_flag wifiConnected

//#define SETTINGS_CLEAR_PIN 12
