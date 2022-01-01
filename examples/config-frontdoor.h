// Configuration defaults for Stacx
//
// You do not actually need to edit these defaults, because you will do
// so either via non-volatile preferences, or via the captive-portal setup process.
extern bool wifiConnected;
extern bool mqttConnected;

#define BUILD_NUMBER 4

#define USE_OLED 0
//#define USE_BT_CONSOLE 1
#define SHELL_DELAY 0
#define SHELL_DELAY_COLD 2000
#define DEBUG_LEVEL 2

//#define SYSLOG_IP 10,1,1,2
//#define SYSLOG_flag wifiConnected

//#define APP_TOPIC "your/prefix/here"

char device_id[16] = "frontdoor";
//#define DEVICE_ID_APPEND_MAC 1
//#define DEVICE_ID_PREFERENCE_GROUP "control"
//#define DEVICE_ID_PREFERENCE_KEY "sensor"
//#define USE_FLAT_TOPIC true
//#define USE_WILDCARD_TOPIC true

char ota_password[20] = "negativeion";
int8_t timeZone = 10;
int8_t minutesTimeZone = 0;
#define NETWORK_RECONNECT_SECONDS 30

//#define CLEAR_PIN 12

// Uncomment to use local network
//char mqtt_host[40] = "192.168.65.70";
//char mqtt_port[16] = "1883";

#define DEFAULT_UPDATE_URL "http://firmware.accelerando.io/stacx/stacx.bin"

char mqtt_host[40] = "tweety.lan";
char mqtt_port[16] = "1883";
char mqtt_user[40] = "";

char mqtt_pass[40] = "";

// Uncomment to use AWS
//char mqtt_host[40] = "iotlab.accelerando.io";
//char mqtt_port[16] = "8883";
//char mqtt_port[16] = "1883";
//char mqtt_user[40] = "";
//char mqtt_pass[40] = "";

enum post_error {
	POST_ERROR_BATTLOW=2,
	POST_ERROR_MODEM,//3
	POST_ERROR_LTE,//4
	POST_ERROR_PUBSUB,//5
	POST_ERROR_CAMERA,//6
	// codes past here are not blinked, because who can count that?
	POST_ERROR_PSRAM,//7
	POST_ERROR_LTE_LOST,//8
	POST_ERROR_LTE_NOSERV,//9
	POST_ERROR_LTE_NOSIG,//10
	POST_ERROR_LTE_NOCONN,//11
	POST_ERROR_LTE_NOSIM,//12
	POST_ERROR_PUBSUB_LOST,//13
	POST_ERROR_MAX
};

enum post_device {
	POST_DEV_ESP,
	POST_DEV_MAX
};

static const char *post_error_names[] = {
	NULL,
	NULL,
	"BATTLOW",
	"MODEM",
	"LTE",
	"PUBSUB",
	"CAMERA",
	"PSRAM",
	"LTE_LOST",
	"LTE_NOSERV",
	"LTE_NOSIG",
	"LTE_NOCONN"
	"LTE_NOSIM"
};

#define POST_ERROR_HISTORY_PERDEV 5
#define POST_ERROR_HISTORY_LEN POST_ERROR_HISTORY_PERDEV*POST_DEV_MAX
