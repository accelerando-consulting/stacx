#if defined(setup) && defined(loop)
// we are in a unit test, we have a mechanism to change the names of the setup/loop functions
#undef setup
#define CUSTOM_SETUP
#undef loop
#define CUSTOM_LOOP
#endif

#include "post.h"

#ifndef USE_NTP
#define USE_NTP 0
#endif

#ifndef BUILD_NUMBER
#define BUILD_NUMBER 0
#endif

#ifdef ESP8266
#include <FS.h> // must be first
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#define HEAP_CHECK 1
#define SETUP_HEAP_CHECK 1
#ifdef HEAP_CHECK
#include <umm_malloc/umm_malloc.h>
#include <umm_malloc/umm_heap_select.h>
#endif
#if USE_NTP
#include <TZ.h>
#include <time.h>
#include <sys/time.h>
#include <sntp.h>
#endif
#elif defined(ESP32)

#define HEAP_CHECK 1
#define SETUP_HEAP_CHECK 1
#define CONFIG_ASYNC_TCP_RUNNING_CORE -1
#define CONFIG_ASYNC_TCP_USE_WDT 0
#ifdef ESP32
#include <soc/rtc_cntl_reg.h>
#endif

#include <WiFi.h>          //https://github.com/esp8266/Arduino
#include <WebServer.h>
#include <ESPmDNS.h>
#include <esp_partition.h>
#include <esp_ota_ops.h>
#include <esp_task_wdt.h>
#include "freertos/portable.h"

#if defined(DEVICE_ID_PREFERENCE_GROUP) && defined(DEVICE_ID_PREFERENCE_KEY)
#include <Preferences.h>
Preferences global_preferences;
#endif // global preferences

// Esp32cam uses pin33 inverted
//#define HELLO_PIN 33
//#define HELLO_ON 0
//#define HELLO_OFF 1

#endif // ESP32

#ifndef HELLO_ON
#ifdef ARDUINO_ESP8266_WEMOS_D1MINIPRO
#define HELLO_ON 0
#else
#define HELLO_ON 1
#endif
#endif //ndef HELLO_ON

#ifndef HELLO_OFF
#ifdef ARDUINO_ESP8266_WEMOS_D1MINIPRO
#define HELLO_OFF 1
#else
#define HELLO_OFF 0
#endif
#endif //ndef HELLO_OFF

#include <Arduino.h>
#include <ArduinoJson.h>
#include <SimpleMap.h>
#ifdef ESP32
#include <Ticker.h>
#endif
#include <time.h>

//@************************** Default preferences ****************************
// you can override these by defining them before including stacx.h
// either in your .ino file or in a config.h included before stacx.h

#ifndef DEVICE_ID
#define DEVICE_ID "stacx"
#endif

#ifndef STACX_LOG_FILE
#define STACX_LOG_FILE "app.log"
#endif

#ifndef STACX_LEAF_VECTOR
#define STACX_LEAF_VECTOR 0
#endif

#ifndef HARDWARE_VERSION
#define HARDWARE_VERSION -1
#endif

#ifndef USE_PREFS
#define USE_PREFS 1
#endif

#ifndef USE_OLED
#define USE_OLED 0
#endif

#ifndef USE_TFT
#define USE_TFT 0
#endif

#ifndef USE_WDT
#define USE_WDT false
#endif

#ifndef STACX_WDT_PANIC
#define STACX_WDT_PANIC true
#endif

#ifndef STACX_WDT_INTERVAL
#define STACX_WDT_INTERVAL 10
#endif

#ifndef USE_BT_CONSOLE
#define USE_BT_CONSOLE 0
#endif

#ifndef USE_STATUS
#define USE_STATUS true
#endif

#ifndef USE_EVENT
#define USE_EVENT true
#endif

#ifndef USE_SET
#define USE_SET true
#endif

#ifndef USE_GET
#define USE_GET true
#endif

#ifndef USE_CMD
#define USE_CMD true
#endif

#ifndef FORCE_SHELL
#define FORCE_SHELL false
#endif

#ifndef USE_FLAT_TOPIC
#define USE_FLAT_TOPIC false
#endif

#ifndef USE_WILDCARD_TOPIC
#define USE_WILDCARD_TOPIC true
#endif

#ifndef SHELL_DELAY
#define SHELL_DELAY 0
#endif

#ifndef SHELL_DELAY_COLD
#define SHELL_DELAY_COLD 2000
#endif

#ifndef LEAF_SETUP_DELAY
#define LEAF_SETUP_DELAY 0
#endif

#ifndef HEARTBEAT_INTERVAL_SECONDS
#define HEARTBEAT_INTERVAL_SECONDS (600)
#endif

#ifndef NETWORK_RECONNECT_SECONDS
#define NETWORK_RECONNECT_SECONDS 20
#endif

#ifndef TIMEZONE_HOURS
#define TIMEZONE_HOURS 10
#endif

#ifndef TIMEZONE_MINUTES
#define TIMEZONE_MINUTES 0
#endif

#ifndef OTA_PASSWORD
#define OTA_PASSWORD "changeme"
#endif

#ifndef HELLO_PIN
#define HELLO_PIN -1
#undef USE_HELLO_PIN
#else
#define USE_HELLO_PIN
#endif

#ifndef HELLO_PIXEL
#define HELLO_PIXEL -1
#undef USE_HELLO_PIXEL
#else
#define USE_HELLO_PIXEL
#endif

#ifndef PIXEL_BLINK
#define PIXEL_BLINK true
#endif

#ifndef HELLO_TRACE_LEVEL
#define HELLO_TRACE_LEVEL 5
#endif

#ifndef DEVICE_ID_MAX
#define DEVICE_ID_MAX 20
#endif

#ifndef EARLY_SERIAL
#define EARLY_SERIAL 0
#endif

#ifndef BOOT_ANIMATION
#define BOOT_ANIMATION 1
#endif

#ifndef BOOT_ANIMATION_DELAY
#define BOOT_ANIMATION_DELAY 50
#endif

#ifndef PIXEL_CODE_DELAY
#define PIXEL_CODE_DELAY 0
#endif

int hello_trace_level = HELLO_TRACE_LEVEL;

#ifdef USE_HELLO_PIXEL
#include <Adafruit_NeoPixel.h>
Adafruit_NeoPixel *hello_pixel_string=NULL;
SemaphoreHandle_t hello_pixel_sem = NULL;
extern Adafruit_NeoPixel *helloPixelSetup();
uint32_t hello_color = Adafruit_NeoPixel::Color(150,0,0);


#define PC_RED Adafruit_NeoPixel::Color(150,0,0)
#define PC_BROWN Adafruit_NeoPixel::Color(191,121,39)
#define PC_GREEN Adafruit_NeoPixel::Color(0,150,0)
#define PC_BLUE Adafruit_NeoPixel::Color(0,0,150)
#define PC_CYAN Adafruit_NeoPixel::Color(0,120,120)
#define PC_YELLOW Adafruit_NeoPixel::Color(120,120,0)
#define PC_GREENYELLOW Adafruit_NeoPixel::Color(80,120,0)
#define PC_ORANGE Adafruit_NeoPixel::Color(120,60,0)
#define PC_MAGENTA Adafruit_NeoPixel::Color(120,0,120)
#define PC_PURPLE Adafruit_NeoPixel::Color(60,0,120)
#define PC_PINK Adafruit_NeoPixel::Color(153,102,255)
#define PC_WHITE Adafruit_NeoPixel::Color(100,100,100)
#define PC_BLACK Adafruit_NeoPixel::Color(0,0,0)
#else
#define PC_BROWN 0xc6007927
#define PC_RED 0x80000000
#define PC_GREEN 0x00008000
#define PC_GREENYELLOW 0x00005078
#define PC_BLUE 0x00000080
#define PC_CYAN 0x00008080
#define PC_YELLOW 0x80008000
#define PC_ORANGE 0x80004000
#define PC_MAGENTA 0x80000080
#define PC_PURPLE 0x40000080
#define PC_PINK 0x990066ff
#define PC_WHITE 0x80008080
#define PC_BLACK 0x00000000
#endif

const char *get_color_name(uint32_t c)
{
  static char name_buf[16];
  if (c==PC_RED)
    return "red";
  else if (c== PC_BROWN)
    return "brown";
  else if (c== PC_GREEN)
    return "green";
  else if (c== PC_BLUE)
    return "blue";
  else if (c== PC_CYAN)
    return "cyan";
  else if (c== PC_YELLOW)
    return "yellow";
  else if (c== PC_GREENYELLOW)
    return "greenyellow";
  else if (c== PC_ORANGE)
    return "orange";
  else if (c== PC_MAGENTA)
    return "magenta";
  else if (c== PC_PURPLE)
    return "purple";
  else if (c== PC_PINK)
    return "pink";
  else if (c== PC_WHITE)
    return "white";
  else if (c== PC_BLACK)
    return "black";
  else {
    snprintf(name_buf, sizeof(name_buf), "0x%08x", c);
    return name_buf;
  }
}

#ifdef ESP32
RTC_DATA_ATTR int saved_reset_reason = -1;
RTC_DATA_ATTR int saved_wakeup_reason = -1;
RTC_DATA_ATTR int saved_uptime_sec = -1;
#endif

enum comms_state {
  UNDEFINED = 0,
  OFFLINE,
  REVERT,
  WAIT_MODEM,
  TRY_MODEM,
  WAIT_IP,
  TRY_IP,
  TRY_GPS,
  WAIT_PUBSUB,
  TRY_PUBSUB,
  ONLINE,
  TRANSACTION,
  COMMS_STATE_COUNT
};

enum comms_state stacx_comms_state=UNDEFINED;
enum comms_state stacx_comms_state_prev=UNDEFINED;

const char *comms_state_name[COMMS_STATE_COUNT]={
  "UNDEFINED",
  "OFFLINE",
  "(revert)",
  "WAIT_MODEM",
  "TRY_MODEM",
  "WAIT_IP",
  "TRY_IP",
  "TRY_GPS",
  "WAIT_PUBSUB",
  "TRY_PUBSUB",
  "ONLINE",
  "TRANSACTION"
};


#ifndef IDLE_PATTERN_OFFLINE
#define IDLE_PATTERN_OFFLINE 50,50
#endif

#ifndef IDLE_PATTERN_WAIT_MODEM
#define IDLE_PATTERN_WAIT_MODEM 100,1
#endif

#ifndef IDLE_PATTERN_TRY_MODEM
#define IDLE_PATTERN_TRY_MODEM 100,50
#endif

#ifndef IDLE_PATTERN_WAIT_IP
#define IDLE_PATTERN_WAIT_IP 250,1
#endif

#ifndef IDLE_PATTERN_TRY_IP
#define IDLE_PATTERN_TRY_IP 250,50
#endif

#ifndef IDLE_PATTERN_TRY_IP
#define IDLE_PATTERN_TRY_IP 250,25
#endif

#ifndef IDLE_PATTERN_WAIT_PUBSUB
#define IDLE_PATTERN_WAIT_PUBSUB 500,1
#endif

#ifndef IDLE_PATTERN_TRY_PUBSUB
#define IDLE_PATTERN_TRY_PUBSUB 500,50
#endif

#ifndef IDLE_PATTERN_ONLINE
#define IDLE_PATTERN_ONLINE 5000,1
#endif

#ifndef IDLE_PATTERN_TRANSACTION
#define IDLE_PATTERN_TRANSACTION 1000,50
#endif


#ifndef IDLE_COLOR_OFFLINE
#define IDLE_COLOR_OFFLINE PC_RED
#endif

#ifndef IDLE_COLOR_WAIT_MODEM
#define IDLE_COLOR_WAIT_MODEM PC_BROWN
#endif

#ifndef IDLE_COLOR_TRY_MODEM
#define IDLE_COLOR_TRY_MODEM PC_MAGENTA
#endif

#ifndef IDLE_COLOR_WAIT_IP
#define IDLE_COLOR_WAIT_IP PC_ORANGE
#endif

#ifndef IDLE_COLOR_TRY_IP
#define IDLE_COLOR_TRY_IP PC_YELLOW
#endif

#ifndef IDLE_COLOR_TRY_GPS
#define IDLE_COLOR_TRY_GPS PC_ORANGE
#endif

#ifndef IDLE_COLOR_WAIT_PUBSUB
#define IDLE_COLOR_WAIT_PUBSUB PC_GREENYELLOW
#endif

#ifndef IDLE_COLOR_TRY_PUBSUB
#define IDLE_COLOR_TRY_PUBSUB PC_CYAN
#endif

#ifndef IDLE_COLOR_ONLINE
#define IDLE_COLOR_ONLINE PC_GREEN
#endif

#ifndef IDLE_COLOR_TRANSACTION
#define IDLE_COLOR_TRANSACTION PC_BLUE
#endif

#ifndef IDENTIFY_INTERVAL
#define IDENTIFY_INTERVAL 1000
#endif

#ifndef IDENTIFY_COUNT
#define IDENTIFY_COUNT 4
#endif




#ifdef ESP32
esp_reset_reason_t reset_reason = esp_reset_reason();
esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
#endif

int8_t timeZone = TIMEZONE_HOURS;
int8_t minutesTimeZone = TIMEZONE_MINUTES;

bool use_status = USE_STATUS;
bool use_event = USE_EVENT;
bool use_set = USE_SET;
bool use_get = USE_GET;
bool use_cmd = USE_CMD;
bool use_wildcard_topic = USE_WILDCARD_TOPIC;
bool use_flat_topic = USE_FLAT_TOPIC;
bool force_shell = FORCE_SHELL;
int heartbeat_interval_seconds = HEARTBEAT_INTERVAL_SECONDS;

char ota_password[20] = OTA_PASSWORD;
unsigned long identify_backing[IDENTIFY_COUNT];

int leaf_setup_delay = LEAF_SETUP_DELAY;

//@***************************** state globals *******************************

char device_id[DEVICE_ID_MAX] = DEVICE_ID;
int hello_pin = HELLO_PIN;
int hello_pixel = HELLO_PIXEL;
int blink_rate = 5000;
int blink_duty = 1;
bool identify = false;
int pixel_fault_code = 0;
unsigned long last_external_input = 0;

#if HEAP_CHECK
int heap_check_interval = 3600000;
static unsigned long last_heap_check = 0;
#endif

#ifdef ESP32
RTC_DATA_ATTR int boot_count = 0;
#else
int boot_count = 0;
#endif

String wake_reason=""; // will be filled in during startup
String _ROOT_TOPIC="";

char mac_short[7] = "unset";
char mac[19];

char post_error_history[POST_ERROR_HISTORY_LEN];
int post_error_reps = 0;
int post_error_code = 0;

enum post_fsm_state {
  POST_IDLE = 0,
  POST_STARTING,
  POST_INITIAL_OFF,
  POST_BLINK_ON,
  POST_BLINK_OFF,
  POST_INTER_REP,
} post_error_state;

#if USE_BT_CONSOLE
  #include "BluetoothSerial.h"
  BluetoothSerial *SerialBT = NULL;
#endif

bool _stacx_ready = false;

#include "accelerando_trace.h"

//@************************** forward declarations ***************************
class Leaf;

void comms_state(enum comms_state s, codepoint_t where=undisclosed_location, Leaf *l=NULL);
void idle_color(uint32_t color, codepoint_t where=undisclosed_location);
void idle_pattern(int cycle, int duty, codepoint_t where=undisclosed_location);
void pixel_code(codepoint_t where, uint32_t code=0, uint32_t color=PC_WHITE);
void post_error(enum post_error, int count);
void post_error_history_update(enum post_device dev, uint8_t err);
void post_error_history_reset();
uint8_t post_error_history_entry(enum post_device dev, int pos);
void disable_bod();
void enable_bod();
void helloUpdate();

void hello_off();
void hello_on();


//@********************************* leaves **********************************

#if USE_OLED
#include "oled.h"
#endif

class Leaf;

#if STACX_LEAF_VECTOR
#include <vector>
typedef std::vector<Leaf*> leaf_table_t;
leaf_table_t leaves;
#define leaf_table_ref_t leaf_table_t &
extern void leaf_allocate();
#else
typedef Leaf ** leaf_table_t;
#define leaf_table_ref_t leaf_table_t
extern Leaf *leaves[];
#endif

#include "leaf.h"


//
//@********************************* setup ***********************************


#ifdef USE_HELLO_PIXEL
void stacx_pixel_check(Adafruit_NeoPixel *pixels, int rounds=4, int step_delay=BOOT_ANIMATION_DELAY,bool log=false)
{
  int px_count = pixels->numPixels();
  if (log) {
    WARN("Pixel check count=%d rounds=%d step=%d", px_count, rounds, step_delay);
  }
  if (hello_pixel_sem && (xSemaphoreTake(hello_pixel_sem, (TickType_t)100) != pdTRUE)) {
    ALERT("Pixel semaphore blocked");
    return;
  }
  for (int cycle=0; cycle<rounds; cycle++) {
    for (int pixel=0; pixel < px_count; pixel++) {
      pixels->clear();
      switch (cycle%4) {
      case 0:
	pixels->setPixelColor(pixel, pixels->Color(255,0,0));
	break;
      case 1:
	pixels->setPixelColor(pixel, pixels->Color(0,255,0));
	break;
      case 2:
	pixels->setPixelColor(pixel, pixels->Color(0,0,255));
	break;
      case 3:
	pixels->setPixelColor(pixel, pixels->Color(255,255,255));
	break;
      }
      pixels->show();
      delay(step_delay);
    }
    pixels->clear();
    pixels->show();
    delay(step_delay);
  }
  if (hello_pixel_sem) {
    xSemaphoreGive(hello_pixel_sem);
  }
}
#endif // USE_HELLO_PIXEL

#if USE_WDT
unsigned long wdt_count=0;
void esp_task_wdt_isr_user_handler(void)
{
  ++wdt_count;
  Serial.println("\n\n**WDT ALERT**\n\n");
}
#endif


void stacx_heap_check(codepoint_t where=undisclosed_location, int level=L_NOTICE)
{
#ifdef HEAP_CHECK
  //size_t heap_size = xPortGetFreeHeapSize();
  //size_t heap_lowater = xPortGetMinimumEverFreeHeapSize();
  static size_t heap_free_prev = 0;
  static size_t stack_hiwm_prev = 0;
#ifdef ESP8266
  uint32_t heap_free;
  uint32_t heap_largest;
  uint8_t frag;
  ESP.getHeapStats(&heap_free, &heap_largest, &frag);
  int change = heap_free_prev?((int)heap_free-(int)heap_free_prev):0;
  if (change <= -2048) level=L_WARN;
  if (heap_free_prev==0) level = L_WARN;

  __DEBUG_AT__(CODEPOINT(where), level, "      heap: RAMfree/largest=%d/%d frag=%d change=%d", (int)heap_free, (int)heap_largest, (int)frag, change);
    heap_free_prev = heap_free;
#elif defined(ESP32)
  size_t heap_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  size_t heap_largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
  int change = heap_free_prev?((int)heap_free-(int)heap_free_prev):0;
  if (change <= -2048) level=L_WARN;
  if (psramFound())  {
    size_t spiram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t spiram_largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
    __DEBUG_AT__(CODEPOINT(where), level, "      heap: RAMfree/largest=%d/%d change=%d SPIfree/largest=%d/%d", (int)heap_free, (int)heap_largest, (int)change, (int)spiram_free, (int)spiram_largest);
  }
  else {
    __DEBUG_AT__(CODEPOINT(where), level, "      heap: RAMfree/largest=%d/%d change=%d", (int)heap_free, (int)heap_largest, change);
  }
  heap_free_prev = heap_free;
  size_t stack_hiwm = uxTaskGetStackHighWaterMark(NULL);
  size_t stack_size = getArduinoLoopTaskStackSize();
  if (stack_hiwm != stack_hiwm_prev) {
    WARN_AT(CODEPOINT(where),   "      stack: size %d hiwm %d",(int)stack_size, (int)stack_hiwm);
    stack_hiwm_prev=stack_hiwm;
  }
#endif // ESP32
#endif // HEAP_CHECK
}

#ifdef CUSTOM_SETUP
void stacx_setup(void)
#else
void setup(void)
#endif
{
#if EARLY_SERIAL
  Serial.begin(115200);
#ifdef TARGET_RP2040
  for (i=0; i<10;i++) delay(1000);
#endif
  Serial.printf("\n# %d %s b#%d %s\n", (int)millis(), DEVICE_ID, BUILD_NUMBER, __DATE__);
#endif

#ifdef USE_HELLO_PIN
  pinMode(hello_pin, OUTPUT);
#endif
#ifdef USE_HELLO_PIXEL
  hello_pixel_string = helloPixelSetup();
#endif

#if BOOT_ANIMATION
  bool do_boot_animation =
#ifdef ESP32
    (esp_reset_reason()!=ESP_RST_DEEPSLEEP)
#else
    true
#endif
    ;

  if (do_boot_animation) {
#if defined(USE_HELLO_PIXEL)
    stacx_pixel_check(hello_pixel_string);
#elif defined(USE_HELLO_PIN)
    for (int i=0; i<3;i++) {
      hello_on();
      delay(4*BOOT_ANIMATION_DELAY);
      hello_off();
      delay(4*BOOT_ANIMATION_DELAY);
    }
    delay(4*BOOT_ANIMATION_DELAY);
#endif //USE_HELLO_PIXEL
  } // endif (do_boot_animation)
  helloUpdate();

#endif // BOOT_ANIMATION
  pixel_code(HERE, 1);

  post_error_history_reset();
  //idle_pattern(50,50,HERE);

  //
  // Set up the serial port for diagnostic trace
  //
#if !EARLY_SERIAL
  Serial.begin(115200);
  Serial.printf("# %d %s b#%d %s\n", (int)millis(), DEVICE_ID, BUILD_NUMBER, __DATE__);
#endif

  if (Serial) {
    //Serial.printf("boot_latency %lums",millis());
    //Serial.printf(" system clock %llu",(unsigned long long)time(NULL));
    Serial.println("\n\n\n");
    Serial.print("# Stacx --- Accelerando.io Multipurpose IoT Backplane");
    if (HARDWARE_VERSION >= 0) {
      Serial.print(", HW version ");
      Serial.print(HARDWARE_VERSION);
#ifdef HARDWARE_VERSION_STRING
      Serial.print(" (");
      Serial.print(HARDWARE_VERSION_STRING);
      Serial.print(")");
#endif
    }
#ifdef BUILD_NUMBER
    Serial.print(", SW build "); Serial.print(BUILD_NUMBER);
#endif
#ifdef ESP8266
    Serial.printf(" for %s", ARDUINO_BOARD);
#elif defined(ESP32)
    Serial.printf(" for %s/%s", ARDUINO_BOARD, ARDUINO_VARIANT);
#else
    Serial.printf(" for %s", BOARD_NAME);
#endif
    Serial.println();
  }

  uint8_t baseMac[6];
  // Get MAC address for WiFi station
#ifdef ESP8266
  WiFi.macAddress(baseMac);
#elif defined(ESP32)
  esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
#endif
  char baseMacChr[18] = {0};
  snprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x", baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);
  snprintf(mac_short, sizeof(mac_short), "%02x%02x%02x", baseMac[3], baseMac[4], baseMac[5]);
#ifdef DEVICE_ID_APPEND_MAC
  strlcat(device_id, "-", sizeof(device_id));
  strlcat(device_id, mac_short, sizeof(device_id));
#endif

  if (Serial) {
    Serial.printf("# MAC address is %s which makes default device ID %s\n",
		  mac, device_id);
  }
#if defined(DEVICE_ID_PREFERENCE_GROUP) && defined(DEVICE_ID_PREFERENCE_KEY)
  #error nope
    global_preferences.begin(DEVICE_ID_PREFERENCE_GROUP, true);
    global_preferences.getString(DEVICE_ID_PREFERENCE_KEY, device_id, sizeof(device_id));
    Serial.printf("Load configured device ID from preferences: %s\n", device_id);
    String s = global_preferences.getString("heartbeat_interval");
    if (s.length()>0) heartbeat_interval_seconds = s.toInt();
    s = global_preferences.getString("debug_level");
    if (s.length()>0) debug_level = s.toInt();
    global_preferences.end();
#endif

  pixel_code(HERE, 2);

  __DEBUG_INIT__();
#if USE_BT_CONSOLE
  SerialBT = new BluetoothSerial();
  SerialBT->begin(device_id); //Bluetooth device name
#endif
  // It is now safe to use accelerando_trace ALERT NOTICE INFO DEBUG macros

#ifdef APP_TOPIC
  // define APP_TOPIC this to use an application prefix address on all topics
  //
  // without APP_TOPIC set topics will resemble
  //		eg. devices/TYPE/INSTANCE/VALUE

  // with APP_TOPIC set topics will resemble
  //		eg. APP_TOPIC/MACADDR/devices/TYPE/INSTANCE/VALUE
  if (strlen(APP_TOPIC)) {
    _ROOT_TOPIC = String(APP_TOPIC)+"/";
  }
  else {
    _ROOT_TOPIC = String("");
  }
#elif defined(APP_TOPIC_BASE)
  // define APP_TOPIC_BASE this to use an application prefix plus mac address on all topics
  //
  _ROOT_TOPIC = String(APP_TOPIC_BASE)+"/"+mac_short+"/";
#endif

  pixel_code(HERE, 3);

#if defined(ESP32) || defined(ESP8266)
  WiFi.mode(WIFI_OFF);
  //disable_bod();
#endif

#if USE_OLED
  oled_setup();
#endif

  pixel_code(HERE, 4);
#ifdef ESP8266
  wake_reason = ESP.getResetReason();
  system_rtc_mem_read(64, &boot_count, sizeof(boot_count));
  ++boot_count;
  if (boot_count < 0) boot_count=0;
  system_rtc_mem_write(64, &boot_count, sizeof(boot_count));
  NOTICE("ESP Wakeup #%d reason: %s", boot_count, wake_reason.c_str());
  ACTION("STACX boot %d %s", boot_count, wake_reason.c_str());
#elif defined(ESP32)
  reset_reason = esp_reset_reason();
  wakeup_reason = esp_sleep_get_wakeup_cause();
  if (saved_reset_reason != -1) {
    ALERT("Overriding reset reason (was %d, faking %d)", reset_reason, saved_reset_reason);
    reset_reason = (esp_reset_reason_t)saved_reset_reason;
    saved_reset_reason = -1;
  }
  if ((reset_reason == ESP_RST_DEEPSLEEP) && (saved_wakeup_reason != -1)) {
    ALERT("Overriding wakeup reason (was %d, faking %d)", wakeup_reason, saved_wakeup_reason);
    wakeup_reason = (esp_sleep_wakeup_cause_t) saved_wakeup_reason;
    saved_wakeup_reason = -1;
  }
  switch (reset_reason) {
  case ESP_RST_UNKNOWN: wake_reason="other"; break;
  case ESP_RST_POWERON: wake_reason="poweron"; break;
  case ESP_RST_EXT: wake_reason="external"; break;
  case ESP_RST_SW: wake_reason="software"; break;
  case ESP_RST_PANIC: wake_reason="panic"; break;
  case ESP_RST_WDT: wake_reason="watchdog"; break;
  case ESP_RST_DEEPSLEEP:
    switch(wakeup_reason)
    {
    case ESP_SLEEP_WAKEUP_EXT0 : wake_reason="deepsleep/rtc io"; break;
    case ESP_SLEEP_WAKEUP_EXT1 : wake_reason ="deepsleep/rtc cntl"; break;
    case ESP_SLEEP_WAKEUP_TIMER : wake_reason = "deepsleep/timer"; break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : wake_reason = "deepsleep/touchpad"; break;
    case ESP_SLEEP_WAKEUP_ULP : wake_reason="deepsleep/ulp"; break;
    case ESP_SLEEP_WAKEUP_GPIO: wake_reason="deepsleep/gpio"; break;
    case ESP_SLEEP_WAKEUP_UART: wake_reason="deepsleep/uart"; break;
    default: wake_reason="deepsleep/other"; break;
    }
    break;
  case ESP_RST_BROWNOUT: wake_reason="brownout"; break;
  case ESP_RST_SDIO: wake_reason="sdio reset"; break;
  default:
    wake_reason="other"; break;
  }
  ++boot_count;
  NOTICE("ESP Wakeup #%d reason: %s", boot_count, wake_reason.c_str());
  ACTION("STACX boot %d %s", boot_count, wake_reason.c_str());

  if (saved_uptime_sec != -1) {
    WARN("Last known uptime was %dsec", saved_uptime_sec);
    saved_uptime_sec=-1;
  }
#endif
#ifdef HEAP_CHECK
  stacx_heap_check(HERE);
#endif

#ifdef ESP32
#if USE_WDT
  WARN("Enable Watchdog Timeout %ds%s",STACX_WDT_INTERVAL, (STACX_WDT_PANIC?"":" (warn only)"));
  ACTION("WDT %ds%s",STACX_WDT_INTERVAL, (STACX_WDT_PANIC?"":" warn"));
  esp_err_t err = esp_task_wdt_init(STACX_WDT_INTERVAL, STACX_WDT_PANIC);
  if (err != ESP_OK) {
    ALERT("WDT init error 0x%x", (int)err);
  }
  err = esp_task_wdt_add(NULL);
  if (err != ESP_OK) {
    ALERT("WDT add error 0x%x", (int)err);
  }
#else
  ACTION("Disable WDT");
  esp_task_wdt_deinit();
#endif
#endif

#if STACX_LEAF_VECTOR
  WARN("Allocating Stacx leaves");
#ifdef SETUP_HEAP_CHECK
  stacx_heap_check(HERE);
#endif
  leaf_allocate();
  leaves.push_back(NULL);
#endif

// If FORCE_SHELL is set, the shell module will do its own pause-for-commands
// later in the startup.
//
// If FORCE_SHELL is not set, (but the shell module is present) then
// offer the chance to drop into the shell by sending a character on console
//
  pixel_code(HERE, 5);
#if !FORCE_SHELL
  Leaf *shell_leaf = Leaf::get_leaf_by_name(leaves, "shell");
  if (shell_leaf && shell_leaf->canRun()) {
    unsigned long wait=SHELL_DELAY;

    if (!wake_reason.startsWith("deepsleep")) {
      wait = SHELL_DELAY_COLD;
    }
    int deciseconds = wait/100;
    WARN("Press any key for shell (you have %lu deciseconds to comply)", deciseconds);
    Serial.flush();
    unsigned long wait_until = millis() + wait;
    do {
      if (Serial.available()) {
	WARN("Activating rescue shell");
	force_shell = true;
	shell_leaf->setup(); // temporary command shell
	force_shell = false;
	break;
      }
      delay(10);
    } while (millis() <= wait_until);
  }
#endif

  pixel_code(HERE, 6);
  //
  // Do any post-sleep hooks if waking from sleep
  //
  if (wake_reason.startsWith("deepsleep/")) {
    ALERT("Woke from deep sleep (%s), trigger post-sleep hooks", wake_reason.c_str());
    for (int i=0; leaves[i]; i++) {
      Leaf *leaf = leaves[i];
      if (leaf->canRun()) {
	leaf->post_sleep();
      }
    }
  }

  pixel_code(HERE, 7);
  //
  // Set up the IO leaves
  //
  // TODO: pass a 'was asleep' flag
  //
  // disable_bod();
  WARN("Initialising Stacx leaves");
  for (int i=0; leaves[i]; i++) {
    Leaf *leaf = leaves[i];
    if (leaf->canRun()) {
      //WARN("%s can run", leaf->getNameStr());
      Leaf::wdtReset(HERE);
#ifdef SETUP_HEAP_CHECK
      stacx_heap_check(HERE);
#endif
      leaf->setup();
      if (leaf_setup_delay) delay(leaf_setup_delay);
    }
    else {
      /*
      WARN("%s won't run", leaf->getNameStr());
      if (leaf->getName() == "lotus") {
	ALERT("DOS is done.");
      }
      */
      ACTION("INHIBIT %s", leaf->getNameStr());
    }
  }
  //enable_bod();

  // summarise the connections between leaves
  for (int i=0; leaves[i]; i++) {
    Leaf *leaf = leaves[i];
    if (leaf->canRun()) {
      leaf->describe_taps();
      leaf->describe_output_taps();
    }
  }
  pixel_code(HERE);

  // call the start method on active leaves
  // (this can be used to do one-off actions after all leaves and taps are configured)
  for (int i=0; leaves[i]; i++) {
    Leaf *leaf = leaves[i];
    if (leaf->canStart()) {
      Leaf::wdtReset(HERE);
#ifdef SETUP_HEAP_CHECK
      stacx_heap_check(HERE);
#endif
      leaf->start();
    }
  }

#ifdef HEAP_CHECK
  stacx_heap_check(HERE, L_WARN);
#endif
  ACTION("STACX ready");
  _stacx_ready = true;
}

void disable_bod()
{
#ifdef ESP32
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
#endif
}

bool check_bod()
{
#ifdef ESP32
  return READ_PERI_REG(RTC_CNTL_BROWN_OUT_REG); //get status of brownout detector
#else
  return false;
#endif
}

void enable_bod()
{
#ifdef ESP32
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 1); //reenable brownout detector
#endif
}

void stacxSetComms(AbstractIpLeaf *ip, AbstractPubsubLeaf *pubsub)
{
  for (int i=0; leaves[i]; i++) {
    leaves[i]->setComms(ip, pubsub);
  }

  if (ip) {
    if (!ip->canRun()) {
      ip->permitRun();
      ip->setup();
      stacx_heap_check(HERE);
    }
  }
  if (pubsub) {
    if (!pubsub->canRun()) {
      pubsub->permitRun();
      pubsub->setup();
      stacx_heap_check(HERE);
    }
  }
}

void stacxSetServiceComms(AbstractIpLeaf *ip, AbstractPubsubLeaf *pubsub)
{
  stacx_heap_check(HERE);
  stacx_heap_check(HERE);
  if (ip) {
    if (!ip->canRun()) {
      WARN("Enabling %s leaf as secondary comms, for service operations only", ip->getNameStr());
      ip->permitRun();
      ip->usePriority("service");
      ip->setup();
      ip->setComms(ip, pubsub);
      stacx_heap_check(HERE);
    }
  }
  if (pubsub) {
    if (!pubsub->canRun()) {
      WARN("Enabling %s pubsub for service operations only", pubsub->getNameStr());
      pubsub->usePriority("service");
      pubsub->setComms(ip, pubsub);
      pubsub->permitRun();
      pubsub->setup();
      stacx_heap_check(HERE);
    }
  }

  for (int i=0; leaves[i]; i++) {
    leaves[i]->setServiceComms(ip, pubsub);
  }

}

void post_error_history_reset()
{
  memset(post_error_history, 0, POST_ERROR_HISTORY_LEN);
}

uint8_t post_error_history_entry(enum post_device dev, int pos)
{
	if (pos >= POST_ERROR_HISTORY_PERDEV) {
		pos = POST_ERROR_HISTORY_PERDEV-1;
	}
	return post_error_history[(POST_ERROR_HISTORY_PERDEV*dev)+pos]-'0';
}

void post_error_history_update(enum post_device dev, uint8_t err)
{
	// rotate the buffer of POST code history
	// (each device has 5 codes recorded in a fifo)
	int devpos = (dev*POST_ERROR_HISTORY_PERDEV);
	for (int i=POST_ERROR_HISTORY_PERDEV-1; i>0;i--) {
		post_error_history[devpos+i] = post_error_history[devpos+i-1];
	}
	post_error_history[devpos] = '0'+err;
}

void post_error(enum post_error err, int count)
{
  ACTION("ERR %s", post_error_names[err]);
#if defined(USE_HELLO_PIN)||defined(USE_HELLO_PIXEL)
  post_error_history_update(POST_DEV_ESP, (uint8_t)err);

  if (count == 0) return;

  post_error_reps = count;
  post_error_code = err;
  post_error_state = POST_STARTING;
#ifdef USE_HELLO_PIXEL
  hello_color = PC_RED;
#endif
  helloUpdate();
#endif // def hello_pin
  return;
}

#if defined(USE_HELLO_PIN) || defined(USE_HELLO_PIXEL)
Ticker led_on_timer;
Ticker led_off_timer;

void hello_on()
{
#ifdef USE_HELLO_PIN
  digitalWrite(hello_pin, HELLO_ON);
#endif

#ifdef USE_HELLO_PIXEL
  __DEBUG__(hello_trace_level, "hello_on identify=%d", ABILITY(identify));
  if (hello_pixel_string) {
    if (identify) {
      int count = hello_pixel_string->numPixels();
      for (int i=0; (i<IDENTIFY_COUNT) && ((hello_pixel+i)<count); i++) {
	identify_backing[i] = hello_pixel_string->getPixelColor(hello_pixel+i);
	hello_pixel_string->setPixelColor(hello_pixel+i, hello_color);
      }
    }
    else {
      hello_pixel_string->setPixelColor(hello_pixel, hello_color);
    }
    if (hello_pixel_sem && (xSemaphoreTake(hello_pixel_sem, (TickType_t)100) != pdTRUE)) {
      ALERT("Pixel semaphore blocked");
    }
    else {
      hello_pixel_string->show();
      if (hello_pixel_sem) {
	xSemaphoreGive(hello_pixel_sem);
      }
    }
  }
#endif
}

void hello_on_blinking()
{
  __DEBUG__(hello_trace_level, "hello_on_blinking post_error_state=%d identify=%d", (int)post_error_state, ABILITY(identify));
  if (post_error_state != POST_IDLE) {
    return;
  }
  if (identify || blink_duty) {
    hello_on();
  }

  int flip = identify?(IDENTIFY_INTERVAL/2):(blink_rate * blink_duty / 100);
  __DEBUG__(hello_trace_level, "hello_on_blinking flip=%d", flip);
  led_off_timer.once_ms(flip, &hello_off);
}

void hello_off()
{
#ifdef USE_HELLO_PIN
  digitalWrite(hello_pin, HELLO_OFF);
#endif
#ifdef USE_HELLO_PIXEL
  __DEBUG__(hello_trace_level, "hello_off post_error_state=%d identify=%d", (int)post_error_state, ABILITY(identify));

  if (hello_pixel_string) {
    if (identify) {
      int count = hello_pixel_string->numPixels();
      for (int i=0; (i<IDENTIFY_COUNT) && ((hello_pixel+i) < count); i++) {
	hello_pixel_string->setPixelColor(hello_pixel+i, identify_backing[i]);
      }
    }
    else {
      hello_pixel_string->setPixelColor(hello_pixel, 0);
    }
    if (hello_pixel_sem && (xSemaphoreTake(hello_pixel_sem, (TickType_t)100) != pdTRUE)) {
      ALERT("Pixel semaphore blocked");
    }
    else {
      hello_pixel_string->show();
      if (hello_pixel_sem) {
	xSemaphoreGive(hello_pixel_sem);
      }
    }
  }
#endif
}
#endif

void helloUpdate()
{
#if defined(USE_HELLO_PIN) || defined(USE_HELLO_PIXEL)
  unsigned long now = millis();
  int interval = identify?IDENTIFY_INTERVAL:blink_rate;
  static int post_rep;
  static int post_blink;
  __DEBUG__(hello_trace_level, "helloUpdate post_error_state=%d identify=%d", (int)post_error_state, ABILITY(identify));

  if (post_error_state==POST_IDLE) {

    if (pixel_fault_code != 0) {
      // LEDs are being used to show a (parallel) fault code, do not blink
      return;
    }

    // cancel any previous blinking behaviour
    led_on_timer.detach();
    led_off_timer.detach();

    bool do_blink = false;
    if (identify) do_blink=true;
#ifdef USE_HELLO_PIN
    do_blink=true;
#elif defined(USE_HELLO_PIXEL) && PIXEL_BLINK
    do_blink=true;
#endif

    if (do_blink) {
      // simple periodic blink
      led_on_timer.attach_ms(interval, hello_on_blinking);
    }
    else {
      // no blinking
      hello_on();
    }
    return;
  }

  // special POST (power on self test) serial-error-code blinking, implemented as an FSM
  //
  // We start out with post_error_rep and post_error_blink=-1
  //
  // Note: trace disabled because using trace from a timer interrupt is BAD BAD BAD
  switch (post_error_state) {
  case POST_IDLE:
    // can't happen
    //ALERT("POST_IDLE state can't happen here");
    break;
  case POST_STARTING:
    // begin an initial one second pause
    //Serial.println("=> POST_STARTING");
    hello_off();
    led_on_timer.detach();
    led_off_timer.detach();
    post_error_state = POST_INITIAL_OFF;
    //Serial.println("=> POST_INITIAL_OFF");
    led_on_timer.once_ms(1000, helloUpdate);
    break;
  case POST_INITIAL_OFF:
    // begin a blink cycle
    hello_on();
    post_rep=1;
    post_blink=1;
    post_error_state = POST_BLINK_ON;
    //Serial.println("=> POST_BLINK_ON");

    led_on_timer.once_ms(200, helloUpdate);
    break;
  case POST_BLINK_ON:
    // completed the first half of a blink (the on half), now do the off half
    hello_off();
    post_error_state = POST_BLINK_OFF;
    //Serial.printf("=> POST_BLINK_OFF rep=%d/%d blink=%d/%d\n", post_rep, post_error_reps, post_blink, post_error_code);
    led_on_timer.once_ms(200, helloUpdate);
    break;
  case POST_BLINK_OFF:
    // completed a full blink.   Either finish this repeititon, or do another blink
    post_blink ++;
    if (post_blink > post_error_code) {
      // end of one rep, leave the led off for 1s, then resume
      post_error_state = POST_INTER_REP;
      //Serial.printf("=> POST_INTER_REP of rep %d/%d\n", post_rep, post_error_reps);
      led_on_timer.once_ms(1000, helloUpdate);
    }
    else {
      // do another blink
      hello_on();
      post_error_state = POST_BLINK_ON;
      //Serial.printf("=> POST_BLINK_ON rep=%d/%d blink=%d/%d\n", post_rep, post_error_reps, post_blink, post_error_code);
      led_on_timer.once_ms(200, helloUpdate);
    }
    break;
  case POST_INTER_REP:
    // finished an inter-repetition pause (1 second off)
    ++post_rep;
    if (post_rep > post_error_reps) {
      // finished the cycle
      post_error_state = POST_IDLE;
      //Serial.println("=> POST_IDLE");

      // resume normal blinking service
      led_on_timer.attach_ms(interval, hello_on_blinking);
    }
    else {
      // begin a a new repetition
      post_blink = 1;
      hello_on();
      post_error_state = POST_BLINK_ON;
      //Serial.printf("=> POST_BLINK_ON repeat #%d of POST code %d\n", post_rep, post_error_code);
      led_on_timer.once_ms(200, helloUpdate);
    }
    break;
    default:
    //ERROR("Unhandled POST FSM state %d", post_error_state);
      break;
  }
#else
  //ALERT("the helloUpdates, they do nothing!");
#endif
}

void set_identify(bool identify_new=true)
{
  ACTION("IDENT %s", ABILITY(identify_new));
  identify = identify_new;
#ifdef USE_HELLO_PIXEL
  if (identify) {
    hello_color = PC_WHITE;
  }
  else {
    comms_state(stacx_comms_state);
  }
#endif

  helloUpdate();
}

void idle_pattern(int cycle, int duty, codepoint_t where)
{
  blink_rate = cycle;
  blink_duty = duty;
  helloUpdate();
}

void pixel_code(codepoint_t where, uint32_t code, uint32_t color)
{
#ifdef USE_HELLO_PIXEL
  pixel_fault_code = code;
  if (!hello_pixel_string) return;

  int px_count = hello_pixel_string->numPixels();
  for (int i=0; i<px_count;i++) {
    if (code == 0) {
      hello_pixel_string->setPixelColor(i, PC_BLACK);
    }
    else if (code & (1<<i)) {
      hello_pixel_string->setPixelColor(i, color);
    }
    else {
      hello_pixel_string->setPixelColor(i, PC_RED);
    }
  }
  led_on_timer.detach();
  led_off_timer.detach();

  if (hello_pixel_sem && (xSemaphoreTake(hello_pixel_sem, (TickType_t)100) != pdTRUE)) {
    ALERT("Pixel semaphore blocked");
  }
  else {
    hello_pixel_string->show();
    if (hello_pixel_sem) {
      xSemaphoreGive(hello_pixel_sem);
    }
  }
  if (code != 0) {
    delay(PIXEL_CODE_DELAY);
  }
#else
  if (debug_level >= L_WARN) {
    Serial.printf("#           BOOT              %s:%s %d code=%lu\n", where.file, where.func, where.line, code);
  }
#endif
  if (pixel_fault_code == 0) {
    // put the comms state pixel back how it was
    if (stacx_comms_state != UNDEFINED) {
      comms_state(stacx_comms_state, HERE);
      helloUpdate();
    }
  }
}

void idle_color(uint32_t c, codepoint_t where)
{
#ifdef USE_HELLO_PIXEL
  hello_color = c;
  hello_on();
#endif
}

void comms_state(enum comms_state s, codepoint_t where, Leaf *l)
{
  int lvl = L_WARN;
  bool suppress_banner=false;
  static unsigned long transaction_start_time = 0;
  bool is_service = false;
  // evil bastard hack
  if (l==NULL) {
    l=leaves[0];
  }

  if (l->getIpComms() && l->getIpComms()->isPriority("service")) {
    is_service = true;
  }


  if ((s==TRANSACTION) || (s==REVERT)) {
    // log at a lower status for we-are-transmitting and we-are-done-transmitting
    lvl = L_INFO;
    if (s==TRANSACTION) {
      transaction_start_time = millis();
    }
    else {
      __DEBUG_AT_FROM__(where, (l->getNameStr()), lvl, "%sCOMMS %s (transaction duration %lums)", (is_service?"SERVICE ":""), comms_state_name[stacx_comms_state_prev], millis()-transaction_start_time);
      suppress_banner=true;
    }
  }

  if (!suppress_banner) {
    __DEBUG_AT_FROM__(where, l->getNameStr(), lvl, "%sCOMMS %s", is_service?"SERVICE ":"", comms_state_name[s]);
  }

  if (s == REVERT) {
    // go back to whatever the immediate previous state was
    s = stacx_comms_state_prev;
  }

  if (!identify) {
    switch (s) {
    case UNDEFINED:
      break;
    case OFFLINE:
      idle_pattern(IDLE_PATTERN_OFFLINE, where);
      idle_color(IDLE_COLOR_OFFLINE, where);
    case WAIT_MODEM:
      idle_pattern(IDLE_PATTERN_WAIT_MODEM, where);
      idle_color(IDLE_COLOR_WAIT_MODEM, where);
      break;
    case TRY_MODEM:
      idle_pattern(IDLE_PATTERN_TRY_MODEM, where);
      idle_color(IDLE_COLOR_TRY_MODEM, where);
      break;
    case WAIT_IP:
      idle_pattern(IDLE_PATTERN_WAIT_IP, where);
      idle_color(IDLE_COLOR_WAIT_IP, where);
      break;
    case TRY_IP:
      idle_pattern(IDLE_PATTERN_TRY_IP, where);
      idle_color(IDLE_COLOR_TRY_IP, where);
      break;
    case WAIT_PUBSUB:
      idle_pattern(IDLE_PATTERN_WAIT_PUBSUB, where);
      idle_color(IDLE_COLOR_WAIT_PUBSUB, where);
      break;
    case TRY_PUBSUB:
      idle_pattern(IDLE_PATTERN_TRY_PUBSUB, where);
      idle_color(IDLE_COLOR_TRY_PUBSUB, where);
      break;
    case TRY_GPS:
      idle_pattern(IDLE_PATTERN_TRY_IP, where);
      idle_color(IDLE_COLOR_TRY_PUBSUB, where);
      break;
    case ONLINE:
      idle_pattern(IDLE_PATTERN_ONLINE, where);
      idle_color(IDLE_COLOR_ONLINE, where);
      break;
    case TRANSACTION:
      idle_pattern(IDLE_PATTERN_TRANSACTION, where);
      idle_color(IDLE_COLOR_TRANSACTION, where);
      break;
    case REVERT:
      ALERT("Assertion failed: REVERT case cant happen here in comms_state()");
      break;
    default:
      ALERT("Unhandled comms state %d", (int)s);
    }
  }

  if (stacx_comms_state != TRANSACTION) {
    // doesn't make sense to revert 'back' to transaction state
    stacx_comms_state_prev = stacx_comms_state;
  }
  stacx_comms_state = s;

  if (is_service) {
    l->publish("_service_comms_state", String(comms_state_name[s]), L_INFO, CODEPOINT(where));
  }
  else {
    l->publish("_comms_state", String(comms_state_name[s]), L_INFO, CODEPOINT(where));
  }
}


//
//@********************************** loop ***********************************

#ifdef CUSTOM_LOOP
void stacx_loop(void)
#else
void loop(void)
#endif
{
  ENTER(L_TRACE);
#if USE_WDT
  static unsigned long wdt_count_was=0;
  if (wdt_count > wdt_count_was) {
    ALERT("WDT was triggered, count=%lu", wdt_count);
    wdt_count_was=wdt_count;
  }
#endif


  unsigned long now = millis();

  Leaf::wdtReset(HERE);
  //
  // Handle Leaf events
  //
  for (int i=0; leaves[i]; i++) {
    Leaf *leaf = leaves[i];
    if (leaf->canRun()
	&& leaf->isStarted()
#ifdef ESP32
	// if own loop is set, the loop task runs in a separate thread, do not call it here
	&& !leaf->hasOwnLoop()
#endif
      ) {
      leaf->loop();
    }
    Leaf::wdtReset(HERE);
  }

#if HEAP_CHECK
  if (now > (last_heap_check+heap_check_interval)) {
    last_heap_check = now;
    stacx_heap_check(HERE, L_WARN);
  }
#endif

  LEAVE;
}

// Local Variables:
// mode: C++
// c-basic-offset: 2
// End:
