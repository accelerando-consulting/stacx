// Configuration defaults for Stacx
//
// You do not actually need to edit these defaults, because you will do
// so via the captive-portal setup process.

//#define USE_OLED 1
#define DEBUG_LEVEL L_DEBUG
// #define APP_TOPIC "catsquirter"
#define SYSLOG_IP 255,255,255,255
extern bool wifiConnected;
#define SYSLOG_flag wifiConnected

char mqtt_server[40] = "mqtt.lan";
char mqtt_port[16] = "1883";
char device_id[16] = "striplight3";
char ota_password[20] = "changeme";
int8_t timeZone = 10;
int8_t minutesTimeZone = 0;
