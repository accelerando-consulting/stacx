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

#include <DNSServer.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <time.h>
#include <ArduinoOTA.h>

#include <PubSubClient.h>
#include <Bounce2.h>

#include "config.h"
#include <Arduino.h>

#include "config.h"
#include "credentials.h"
#include "accelerando_trace.h"
#include "wifi.h"

//@******************************* constants *********************************

#ifndef HEARTBEAT_INTERVAL
#define HEARTBEAT_INTERVAL (5*60*1000)
#endif

//@******************************* variables *********************************


//@********************************* pods **********************************

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
