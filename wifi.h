#ifdef ESP32
#include "esp_system.h"
#include <HTTPClient.h>
#endif

//@******************************* variables *********************************

#ifdef ESP8266
#define CONFIG_FILE "config.json"
#else
#define CONFIG_FILE "/config.json"
#endif

//
// Network resources
//
bool wifiConnected = false;

WiFiClient espClient;

bool _shouldSaveConfig = false;
#ifdef ESP8266
WiFiEventHandler _gotIpEventHandler, _disconnectedEventHandler;
#endif
Ticker wifiReconnectTimer;
extern Ticker mqttReconnectTimer;

#ifdef USE_NTP
boolean syncEventTriggered = false; // True if a time even has been triggered
NTPSyncEvent_t ntpEvent; // Last triggered event
#endif

int blink_rate = 100;
int blink_duty = 50;
char ip_addr_str[20] = "unset";
char reformat[8] = "no";
char mac_short[7];
char mac[19];
char ap_ssid[32]="";

IPAddress ap_ip = IPAddress(192,168,  4,  1);
IPAddress ap_gw = IPAddress(192,168,  4,  1);
IPAddress ap_sn = IPAddress(255,255,255,  0);
String ap_ip_str;


//
//@******************************* functions *********************************

void wifi_setup();
bool _readConfig();
void _writeConfig(bool force_format = false);
void _mqtt_connect();
void _wifiMgr_setup(bool reset = false);

void getMacAddress() {
  uint8_t baseMac[6];
  // Get MAC address for WiFi station
#ifdef ESP8266
  WiFi.macAddress(baseMac);
#else
  esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
#endif
  char baseMacChr[18] = {0};
  snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X", baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);
  snprintf(mac_short, sizeof(mac_short), "%02X%02X%02X", baseMac[3], baseMac[4], baseMac[5]);
}

void _saveConfigCallback()
{
  ALERT("Will save config");
  _shouldSaveConfig = true;
}

void _wifiAPCallback(WiFiManager *wifiManager)
{
  INFO("AP will use static ip %s", ap_ip_str.c_str());
  const char *ssid = ap_ssid;
  const char *ip = ap_ip_str.c_str();
  //const char *ip = WiFi.softAPIP().toString().c_str();
  //  const char *ssid = WiFi.SSID().c_str();
  //IPAddress softIP = WiFi.softAPIP();
  //String softIPStr = String(softIP[0]) + "." + softIP[1] + "." + softIP[2] + "." + softIP[3];
  NOTICE("Created wifi AP: %s %s", ssid, ip);
  //NOTICE("SoftAP ip = %s", softIPStr.c_str());
#if USE_OLED
  oled_text(0,10, String("WiFi Access Point:"));
  oled_text(0,20, ssid);
  oled_text(0,30, String("WiFi IP: ")+ip);
  oled_text(0,40, String("setup at http://")+ip+"/");
#endif
}

bool _readConfig()
{
  if (!SPIFFS.begin()) {
    ALERT("NO SPIFFS.  Formatting");
    _writeConfig(true);
    ALERT("Rebooting after format");
    delay(3000);
#ifdef ESP8266
    ESP.reset();
#else
    ESP.restart();
#endif
  }

  NOTICE("mounted file system");
  if (!SPIFFS.exists(CONFIG_FILE)) {
    ALERT("No configuration file found");
    _writeConfig(true);
  }

  //file exists, reading and loading
  NOTICE("reading config file");
  File configFile = SPIFFS.open(CONFIG_FILE, "r");
  if (!configFile) {
    ALERT("Cannot read config file");
    return false;
  }

  size_t size = configFile.size();
  NOTICE("Parsing config file, size=%d", size);

  DynamicJsonDocument doc(size*2);
  DeserializationError error = deserializeJson(doc, configFile);
  if (error) {
    ALERT("Failed to parse config file: %s", error.c_str());
    configFile.close();
    return false;
  }

  JsonObject root = doc.as<JsonObject>();
  {
    char buf[256];
    serializeJson(root, buf, sizeof(buf));
    NOTICE("Read config: %s", buf);
  }

  strlcpy(mqtt_server, root["mqtt_server"]|"mqtt.lan", sizeof(mqtt_server));
  strlcpy(mqtt_port, root["mqtt_port"]|"1883", sizeof(mqtt_server));
  strlcpy(device_id, root["device_id"]|"scratch", sizeof(mqtt_server));
  strlcpy(ota_password, root["ota_password"]|"changeme", sizeof(mqtt_server));

  configFile.close();
  return true;
}

void _writeConfig(bool force_format)
{
  ALERT("saving config to flash");

  if (force_format) {
    ALERT("Reformatting SPIFFS");
    if (SPIFFS.format()) {
      NOTICE("Reformatted OK");
    }
    else {
      ALERT("SPIFFS Format failed");
      return;
    }
  }

  if (!SPIFFS.begin()) {
    ALERT("failed to mount FS");
    return;
  }

#if 0
  if (!SPIFFS.rename(CONFIG_FILE,CONFIG_FILE".bak")) {
    ALERT("Unable to create backup config file");
    return;
  }
#endif

  File configFile = SPIFFS.open(CONFIG_FILE, "w");
  if (!configFile) {
    ALERT("Unable to create new config file");
    return;
  }

  DynamicJsonDocument doc(1024);
  JsonObject root = doc.to<JsonObject>();

  root["mqtt_server"] = mqtt_server;
  root["mqtt_port"] = mqtt_port;
  root["device_id"] = device_id;
  root["ota_password"] = ota_password;

  if (serializeJson(doc, configFile) == 0) {
    ALERT("Failed to serialise configuration");
  }
  {
    char buf[256];
    serializeJson(root, buf, sizeof(buf));
    NOTICE("Written config: %s", buf);
  }
  configFile.close();
}

#ifdef ESP8266
void  _wifi_connect_callback(const WiFiEventStationModeGotIP& event)
#else
void  _wifi_connect_callback(WiFiEvent_t event, WiFiEventInfo_t info)
#endif
{
#ifdef ESP8266
  strlcpy(ip_addr_str, event.ip.toString().c_str(), sizeof(ip_addr_str));
#else
  strlcpy(ip_addr_str, IPAddress(info.got_ip.ip_info.ip.addr).toString().c_str(), sizeof(ip_addr_str));
#endif
  NOTICE("WiFi connected, IP: %s OTA: %s", ip_addr_str, ota_password);
  wifiConnected = true;
  blink_rate = 500;
  blink_duty = 50;

  // Cancel any future connect attempt, as we are now connected
  wifiReconnectTimer.detach();

  // Get the time from NTP server
#ifdef ESP8266
#ifdef USE_NTP
  NOTICE("Starting NTP");
  NTP.begin(DEFAULT_NTP_SERVER, timeZone);
#endif
#else // ESP32
  configTime(0, 0, "pool.ntp.org");

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    ALERT("Failed to obtain time");
  }
#endif
  
  // Start trying to connect to MQTT
  // (Connection won't proceed until setup is complete)
  _mqtt_connect();
}

#ifdef ESP8266
void _wifi_disconnect_callback(const WiFiEventStationModeDisconnected& event)
#else
void _wifi_disconnect_callback(WiFiEvent_t event, WiFiEventInfo_t info)
#endif
{
#ifdef ESP8266
  ALERT("WiFi disconnected (reason %d)", (int)(event.reason));
#else
  ALERT("WiFi disconnected (reason %d)", (int)(info.disconnected.reason));
#endif
  wifiConnected = false;
  blink_rate = 100;
  blink_duty = 50;

#ifdef ESP8266
  if (event.reason == WIFI_DISCONNECT_REASON_AUTH_FAIL) {
    NOTICE("Auth failed, reverting to config portal");
    delay(2000);
    _wifiMgr_setup(true);
    return;
  }
// TODO: handle esp32 case here
#endif

  wifiReconnectTimer.once(NETWORK_RECONNECT_SECONDS, wifi_setup);
}

#ifdef USE_NTP
void processSyncEvent (NTPSyncEvent_t ntpEvent) {
  if (ntpEvent < 0) {
    NOTICE ("Time Sync error: %d", ntpEvent);
    if (ntpEvent == noResponse) {
      NOTICE("NTP server not reachable");
    }
    else if (ntpEvent == invalidAddress) {
      NOTICE("Invalid NTP server address");
    }
    else if (ntpEvent == errorSending) {
      NOTICE("Error sending request");
    }
    else if (ntpEvent == responseError) {
      NOTICE("NTP response error");
    }
  } else {
    if (ntpEvent == timeSyncd) {
      NOTICE("Got NTP time: %s", NTP.getTimeDateString(NTP.getLastNTPSync ()).c_str());
    }
  }
}
#endif

void _wifiMgr_setup(bool reset)
{
  ENTER(L_INFO);
  ALERT("Wifi manager setup commencing");
  if (!_readConfig()) {
    ALERT("Could not read configuration file, forcing config portal");
    reset= true;
  }
  snprintf(ap_ssid, sizeof(ap_ssid), "%s_%s", device_id, mac_short);
  INFO("Using AP SSID %s", ap_ssid);

  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_mqtt_server("mqtt_server", "mqtt server", mqtt_server, sizeof(mqtt_server));
  WiFiManagerParameter custom_mqtt_port("mqtt_port", "mqtt port", mqtt_port, sizeof(mqtt_port));
  WiFiManagerParameter custom_device_id("device_id", "Device ID", device_id, sizeof(device_id));
  WiFiManagerParameter custom_ota_password("ota_password", "Update Password", ota_password, sizeof(ota_password));
  WiFiManagerParameter custom_reformat("reformat", "Force format", reformat, sizeof(reformat));


  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(_saveConfigCallback);
  wifiManager.setAPCallback(_wifiAPCallback);

  //set static ip
  ap_ip_str = ap_ip.toString();
  INFO("AP will use static ip %s", ap_ip_str.c_str());
  wifiManager.setAPStaticIPConfig(ap_ip, ap_gw, ap_sn);
  //wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));

  //add all your parameters here
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_device_id);
  wifiManager.addParameter(&custom_ota_password);
  wifiManager.addParameter(&custom_reformat);

  //reset settings - for testing
  pinMode(5, INPUT_PULLUP);
  delay(50);
  if (digitalRead(5) == LOW) {
    ALERT("Settings reset via pin 5 low");
    reset=true;
  }

  if (reset) {
    ALERT("Starting wifi config portal %s", ap_ssid);
#if USE_OLED
    oled_text(0,10, String("AP: ")+ap_ssid);
#endif
    wifiManager.startConfigPortal(ap_ssid);
  }

  //set minimum quality of signal so it ignores AP's under that quality
  //defaults to 8%
  wifiManager.setMinimumSignalQuality();

  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  wifiManager.setTimeout(300);
  wifiManager.setDebugOutput(true);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //and goes into a blocking loop awaiting configuration
#if USE_OLED
  oled_text(0,10, "Joining wifi...");
#endif
  if (!wifiManager.autoConnect(ap_ssid)) {
    ALERT("Failed to connect to WiFi after timeout");
#if USE_OLED
    oled_text(0,20, "WiFi timeout");
#endif
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
#ifdef ESP8266
    ESP.reset();
#else
    ESP.restart();
#endif
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  ALERT("Connected to WiFi");
  wifiConnected = true;

  //read updated parameters
  strlcpy(mqtt_server, custom_mqtt_server.getValue(),sizeof(mqtt_server));
  strlcpy(mqtt_port, custom_mqtt_port.getValue(), sizeof(mqtt_port));
  strlcpy(device_id, custom_device_id.getValue(), sizeof(device_id));
  strlcpy(ota_password, custom_ota_password.getValue(), sizeof(ota_password));
  strlcpy(reformat, custom_reformat.getValue(), sizeof(reformat));

  bool force_format = false;
  if (strcasecmp(reformat, "yes") == 0) force_format = true;
  //save the custom parameters to FS
  if (_shouldSaveConfig) {
    _writeConfig(force_format);
  }

  MDNS.begin(device_id);

  ALERT("My IP Address: %s", WiFi.localIP().toString().c_str());
#if USE_OLED
  oled_text(0,20, "WiFi IP: "+WiFi.localIP().toString());
#endif

}

void _OTAUpdate_setup() {
  ENTER(L_INFO);

  ArduinoOTA.setHostname(device_id);
  ArduinoOTA.setPassword(ota_password);

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    ALERT("Start OTA update (%s)", type.c_str());
  });
  ArduinoOTA.onEnd([]() {
    ALERT("OTA Complete");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    NOTICE("OTA in progress: %3u%% (%u/%u)", (progress / (total / 100)), progress, total);
  });
  ArduinoOTA.onError([](ota_error_t error) {
    ALERT("OTA Error [%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      ALERT("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      ALERT("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      ALERT("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      ALERT("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      ALERT("End Failed");
    }
  });
  ArduinoOTA.begin();
}


#ifdef _OTA_OPS_H

void update_progress(size_t done, size_t size)
{
  NOTICE("Update progress %lu / %lu", done, size);
  //delay(500); // let other tasks run to avoid WDT reset
}

void rollback_update(String notused)
{
  if (Update.canRollBack()) {
    ALERT("Rolling back to previous version");
    if (Update.rollBack()) {
      NOTICE("Rollback succeeded.  Rebooting.");
      delay(1000);
      ESP.restart();
    }
    else {
      ALERT("Rollback failed");
    }
  }
  else {
    ALERT("Rollback is not possible");
  }
}


void pull_update(String url)
{
  HTTPClient http;
  http.begin(url);
  static const char *want_headers[] = {
    "Content-Length",
    "Content-Type",
    "Last-Modified",
    "Server"
  };
  http.collectHeaders(want_headers, sizeof(want_headers)/sizeof(char*));

  int httpCode = http.GET();
  if(httpCode > 0) {
    // HTTP header has been send and Server response header has been handled
    INFO("HTTP OTA request returned code %d", httpCode);

    // file found at server
    if(httpCode == HTTP_CODE_OK) {
      int header_count = http.headers();
      for (int i = 0; i < header_count; i++) {
	INFO("response header %s: %s",
	     http.headerName(i).c_str(), http.header(i).c_str());
      }
      NOTICE("HTTP Content-Length %s", http.header("Content-Length").c_str());
      NOTICE("HTTP Content-Type   %s", http.header("Content-Type").c_str());
      int contentLength = http.header("Content-Length").toInt();
      bool canBegin = Update.begin(contentLength, U_FLASH, 2, HIGH);
      if (canBegin) {
	NOTICE("Begin OTA.  May take several minutes.");
	// No activity would appear on the Serial monitor
	// So be patient. This may take 2 - 5mins to complete
	Update.onProgress(update_progress);
	size_t written = Update.writeStream(http.getStream());

	if (written == contentLength) {
	  INFO("Wrote %d successfully", written);
	} else {
	  ALERT("Wrote only %d / %d. ", written, contentLength );
	}

	if (Update.end()) {
	  NOTICE("OTA done!");
	  if (Update.isFinished()) {
	    NOTICE("Update successfully completed. Rebooting.");
	    ESP.restart();
	  } else {
	    ALERT("Update not finished? Something went wrong!");
	  }
	} else {
	  ALERT("Update error code %d", (int)Update.getError());
	}
      } else {
	// not enough space to begin OTA
	// Understand the partitions and
	// space availability
	ALERT("Not enough space to begin OTA");
      }
    }
  } else {
    ALERT("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();

}
#endif // _OTA_OPS_H

void wifi_setup()
{
  ENTER(L_NOTICE);

  getMacAddress();

#ifdef ESP8266
  _gotIpEventHandler = WiFi.onStationModeGotIP(_wifi_connect_callback);
#else // ESP32
  WiFi.onEvent(_wifi_connect_callback, WiFiEvent_t::SYSTEM_EVENT_STA_GOT_IP);
#endif

#ifdef USE_NTP
  NTP.onNTPSyncEvent ([](NTPSyncEvent_t event) {
			ntpEvent = event;
			syncEventTriggered = true;
		      });
#endif
  _wifiMgr_setup();
  _OTAUpdate_setup();

#ifdef ESP8266
  _disconnectedEventHandler = WiFi.onStationModeDisconnected(_wifi_disconnect_callback);
#else // ESP32
  WiFi.onEvent(_wifi_disconnect_callback, WiFiEvent_t::SYSTEM_EVENT_STA_DISCONNECTED);
#endif
  LEAVE;
}

void wifi_loop()
{
  ArduinoOTA.handle();

#ifdef USE_NTP
  if (syncEventTriggered) {
    processSyncEvent (ntpEvent);
    syncEventTriggered = false;
  }
#endif

}

// Local Variables:
// mode: C++
// c-basic-offset: 2
// End:
