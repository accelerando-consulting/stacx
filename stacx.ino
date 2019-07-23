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
#define helloPin D4
#else // ESP32
#include <SPIFFS.h>
#include <WiFi.h>          //https://github.com/esp8266/Arduino
#include <WebServer.h>
#include <ESPmDNS.h>
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

//#define SYSLOG_flag wifiConnected
//#define SYSLOG_host mqtt_server
#define DEBUG_LEVEL L_NOTICE

#include "config.h"
#include "accelerando_trace.h"

//@******************************* constants *********************************
// you can override these by defining them in config.h

#ifndef HEARTBEAT_INTERVAL_SECONDS
#define HEARTBEAT_INTERVAL_SECONDS (60)
#endif

#ifndef NETWORK_RECONNECT_SECONDS
#define NETWORK_RECONNECT_SECONDS 5
#endif

#ifndef MQTT_RECONNECT_SECONDS
#define MQTT_RECONNECT_SECONDS 10
#endif

//@******************************* variables *********************************

const char *wake_reason = NULL; // will be filled in during startup
String _ROOT_TOPIC="";

//@********************************* leaves **********************************

#include "wifi.h"
#include "leaf.h"
#include "mqtt.h"
#include "leaves.h"

//
//@********************************* setup ***********************************

void setup(void)
{
  //
  // Set up the serial port for diagnostic trace
  //
  Serial.begin(115200);
  Serial.println("\n\n\n");
  Serial.println("Accelerando.io Multipurpose IoT Backplane (stacx)");

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


  //
  // Set up the WiFi connection and MQTT client
  //
  wifi_setup();
  // uncomment this to use an application prefix on all topics
  //_ROOT_TOPIC = String("stacx/")+mac_short+"/";
  mqtt_setup();

  //
  // Set up the IO leaves
  //
#ifdef helloPin
  pinMode(helloPin, OUTPUT);
#endif

  for (int i=0; leaves[i]; i++) {
    Leaf *leaf = leaves[i];

    NOTICE("LEAF %d SETUP: %s", i+1, leaf->get_name().c_str());
    leaf->setup();
    leaf->describe_taps();
  }
  for (int i=0; leaves[i]; i++) {
    leaves[i]->describe_output_taps();
  }

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
  if (led != hello) {
    hello = led;
    digitalWrite(helloPin, hello);
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
