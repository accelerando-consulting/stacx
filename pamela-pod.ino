#if defined(ESP8266)
#include <FS.h> // must be first
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#else // ESP32
#include <SPIFFS.h>
#include <WiFi.h>          //https://github.com/esp8266/Arduino
#include <WebServer.h>
#include <ESPmDNS.h>
#endif

#include <Arduino.h>
#include <DNSServer.h>
#include <WiFiManager.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <AsyncMqttClient.h>
#include <Ticker.h>
#include <time.h>
#include <Bounce2.h>

#include "config.h"
#include "credentials.h"
#include "accelerando_trace.h"

//@******************************* constants *********************************
// you can override these by defining them in config.h

#ifndef HEARTBEAT_INTERVAL_SECONDS
#define HEARTBEAT_INTERVAL_SECONDS (5*60)
#endif

#ifndef NETWORK_RECONNECT_SECONDS 
#define NETWORK_RECONNECT_SECONDS 5
#endif

#ifndef MQTT_RECONNECT_SECONDS
#define MQTT_RECONNECT_SECONDS 10
#endif

//@******************************* variables *********************************


//@********************************* pods **********************************

#include "wifi.h"
#include "pod.h"
#include "mqtt.h"
#include "pods.h"

//
//@********************************* setup ***********************************

void setup()
{
  // 
  // Set up the serial port for diagnostic trace
  // 
  Serial.begin(115200);
  Serial.println("\n\n\n");
  Serial.println("Accelerando.io Multipurpose IoT Backplane");

  // 
  // Set up the WiFi connection and MQTT client
  // 
  wifi_setup();
  mqtt_setup();

  // 
  // Set up the IO pods
  //
#ifdef HELLO_PIN  
  pinMode(helloPin, OUTPUT);
#endif  

  for (int i=0; pods[i]; i++) {
    NOTICE("Pod %d: %s", i+1, pods[i]->describe().c_str());
    pods[i]->setup();
  }
  
  ALERT("Setup complete");
}

//
//@********************************** loop ***********************************

void loop()
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
  // Handle Pod events
  // 
  for (int i=0; pods[i]; i++) {
    pods[i]->loop();
  }
}

// Local Variables:
// mode: C++
// c-basic-offset: 2
// End:
