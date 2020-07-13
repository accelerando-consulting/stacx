#if defined(ESP8266)
#include <FS.h> // must be first
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#define USE_NTP
#ifdef USE_NTP
#include <TimeLib.h>
#include <NtpClientLib.h>
#endif
#define helloPin 4
#else // ESP32

#define CONFIG_ASYNC_TCP_RUNNING_CORE -1
#define CONFIG_ASYNC_TCP_USE_WDT 0

#include <SPIFFS.h>
#include <WiFi.h>          //https://github.com/esp8266/Arduino
#include <WebServer.h>
#include <ESPmDNS.h>
#include <esp_partition.h>
#include <esp_ota_ops.h>
#define helloPin 2
#endif

#include <Arduino.h>
#include <DNSServer.h>
#include <WiFiManager.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <AsyncMqttClient.h>
#include <SimpleMap.h>
#include <Ticker.h>
#include <time.h>

#include "config.h"

#ifndef USE_BT_CONSOLE
  #define USE_BT_CONSOLE 0
#endif

#ifndef USE_OLED
  #define USE_OLED 0
#endif

#if USE_BT_CONSOLE
  #error BT_CONSOLE is busted
  #include "BluetoothSerial.h"
  BluetoothSerial *SerialBT = NULL;
  String bt_handle="stacx";
#endif

#include "accelerando_trace.h"

//@******************************* constants *********************************
// you can override these by defining them in config.h

#ifndef HEARTBEAT_INTERVAL_SECONDS
#define HEARTBEAT_INTERVAL_SECONDS (60)
#endif

#ifndef NETWORK_RECONNECT_SECONDS
#define NETWORK_RECONNECT_SECONDS 2
#endif

#ifndef MQTT_RECONNECT_SECONDS
#define MQTT_RECONNECT_SECONDS 2
#endif

//@******************************* variables *********************************

const char *wake_reason = NULL; // will be filled in during startup
String _ROOT_TOPIC="";

//@********************************* leaves **********************************

#if USE_OLED
#include "oled.h"
#endif

#include "wifi.h"
#include "leaf.h"
#include "mqtt.h"
#include "leaves.h"

//
//@********************************* setup ***********************************

void setup(void)
{
#ifdef helloPin
  pinMode(helloPin, OUTPUT);
  digitalWrite(helloPin, 1);
  delay(500);
  digitalWrite(helloPin, 0);
  delay(500);
  digitalWrite(helloPin, 1);
#endif

  //
  // Set up the serial port for diagnostic trace
  //
  Serial.begin(115200);
  Serial.println("\n\n\n");
  Serial.println("Accelerando.io Multipurpose IoT Backplane");
#ifdef helloPin
  delay(1000);
  digitalWrite(helloPin, 0);
#endif

#if USE_BT_CONSOLE
//  snprintf(bt_handle, sizeof(bt_handle),
//	   "%s_%08x", device_id, (uint32_t)ESP.getEfuseMac());
  SerialBT = new BluetoothSerial();
  SerialBT->begin("ESP32test"); //Bluetooth device name
#endif

  // It is now safe to use accelerando_trace ALERT NOTICE INFO DEBUG macros

#ifdef ESP8266
  wake_reason = ESP.getResetReason().c_str();
#else
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  switch(wakeup_reason)
  {
  case ESP_SLEEP_WAKEUP_EXT0 : wake_reason="RTC IO"; break;
  case ESP_SLEEP_WAKEUP_EXT1 : wake_reason ="RTC CNTL"; break;
  case ESP_SLEEP_WAKEUP_TIMER : wake_reason = "timer"; break;
  case ESP_SLEEP_WAKEUP_TOUCHPAD : wake_reason = "touchpad"; break;
  case ESP_SLEEP_WAKEUP_ULP : wake_reason="ULP"; break;
  case ESP_SLEEP_WAKEUP_GPIO: wake_reason="GPIO"; break;
  case ESP_SLEEP_WAKEUP_UART: wake_reason="UART"; break;
  default: wake_reason="other"; break;
  }
#endif
  NOTICE("ESP Wakeup reason: %s", wake_reason);

#if USE_OLED
  oled_setup();
#endif

  //
  // Set up the WiFi connection and MQTT client
  //
  wifi_setup();
#ifdef APP_TOPIC
  // define APP_TOPIC this to use an application prefix on all topics
  //
  // without APP_TOPIC set topics will resemble
  //		eg. devices/TYPE/INSTANCE/VALUE

  // with APP_TOPIC set topics will resemble
  //		eg. APP_TOPIC/MACADDR/devices/TYPE/INSTANCE/VALUE
  //
  _ROOT_TOPIC = String(APP_TOPIC)+"/"+mac_short+"/";
#endif

  mqtt_setup();

  //
  // Set up the IO leaves
  //

  for (int i=0; leaves[i]; i++) {
    Leaf *leaf = leaves[i];

    NOTICE("LEAF %d SETUP: %s", i+1, leaf->get_name().c_str());
    leaf->setup();
    leaf->describe_taps();
  }
  for (int i=0; leaves[i]; i++) {
    leaves[i]->describe_output_taps();
  }

  mqttConfigured = true;
  ALERT("Setup complete");
}

//
//@********************************** loop ***********************************

void loop(void)
{
  unsigned long now = millis();

#ifdef helloPin
  static int hello = 0;

  int pos = now % blink_rate;
  int flip = blink_rate * blink_duty / 100;
  int led = (pos > flip);
  //DEBUG("now = %lu pos=%d flip=%d led=%d hello=%d", now, pos, flip, led, hello);
  if (led != hello) {
    //INFO("writing helloPin <= %d", led);
    digitalWrite(helloPin, hello=led);
  }
#endif

  //
  // Handle network Events
  //
  wifi_loop();
  mqtt_loop();

  //
  // Handle Leaf events
  //
  for (int i=0; leaves[i]; i++) {
    leaves[i]->loop();
  }
}

// Local Variables:
// mode: C++
// c-basic-offset: 2
// End:
