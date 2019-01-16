

//@******************************* variables *********************************

// 
// Network resources
//
bool wifiConnected = false;

WiFiClient espClient;

bool _shouldSaveConfig = false;
WiFiEventHandler _gotIpEventHandler, _disconnectedEventHandler;
Ticker wifiReconnectTimer;
extern Ticker mqttReconnectTimer;

int blink_rate = 100;
int blink_duty = 50;
char ip_addr_str[20] = "unset";
char reformat[8] = "yes";


//
//@******************************* functions *********************************

void wifi_setup();
void _readConfig();
void _writeConfig(bool force_format = false);
void _mqtt_connect();
void _wifiMgr_setup(bool reset = false);


void _saveConfigCallback() 
{
  ALERT("Will save config");
  _shouldSaveConfig = true;
}

void _readConfig() 
{
  if (!SPIFFS.begin()) {
    ALERT("NO SPIFFS.  Formatting");
    _writeConfig(true);
    ALERT("Rebooting after format");
    delay(3000);
    ESP.reset();
  }

  NOTICE("mounted file system");
  if (!SPIFFS.exists("config.json")) {
    ALERT("No configuration file found");
    _writeConfig(true);
  }

  //file exists, reading and loading
  NOTICE("reading config file");
  File configFile = SPIFFS.open("config.json", "r");
  if (!configFile) {
    ALERT("Cannot read config file");
    return;
  }

  size_t size = configFile.size();
  NOTICE("Parsing config file, size=%d", size);

  DynamicJsonDocument doc;
  DeserializationError error = deserializeJson(doc, configFile);
  if (error) {
    ALERT("Failed to parse config file: %s", error.c_str());
    configFile.close();
    return;
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
  if (!SPIFFS.rename("config.json","config.bak")) {
    ALERT("Unable to create backup config file");
    return;
  }
#endif

  File configFile = SPIFFS.open("config.json", "w");
  if (!configFile) {
    ALERT("Unable to create new config file");
    return;
  }

  DynamicJsonDocument doc;
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

void _wifi_connect_callback(const WiFiEventStationModeGotIP& event) 
{
  ALERT("WiFi connected, IP: %s", event.ip.toString().c_str());
  strlcpy(ip_addr_str, event.ip.toString().c_str(), sizeof(ip_addr_str));
  wifiConnected = true;
  blink_rate = 500;
  blink_duty = 50;

  // Cancel any future connect attempt, as we are now connected
  wifiReconnectTimer.detach(); 

  // Start trying to connect to MQTT
  _mqtt_connect();
}

void _wifi_disconnect_callback(const WiFiEventStationModeDisconnected& event)
{
  ALERT("WiFi disconnected (reason %d)", (int)(event.reason));
  wifiConnected = false;
  blink_rate = 100;
  blink_duty = 50;

  // Cancel any existing timers, and set a new timer to retry wifi in 2 seconds
  mqttReconnectTimer.detach();
  if (event.reason == WIFI_DISCONNECT_REASON_AUTH_FAIL) {
    NOTICE("Auth failed, reverting to config portal");
    delay(2000);
    _wifiMgr_setup(true);
    return;
  }

  wifiReconnectTimer.once(NETWORK_RECONNECT_SECONDS, wifi_setup);
}



void _wifiMgr_setup(bool reset) 
{
  _readConfig();

  ALERT("Wifi manager setup");
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

  //set static ip
  //wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
  
  //add all your parameters here
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_device_id);
  wifiManager.addParameter(&custom_ota_password);
  wifiManager.addParameter(&custom_reformat);

  //reset settings - for testing

  if (reset /*LATER lightButton.read() == LOW*/) {
    ALERT("Factory Reset");
    wifiManager.resetSettings();
  }
  
  //set minimum quality of signal so it ignores AP's under that quality
  //defaults to 8%
  //wifiManager.setMinimumSignalQuality();
  
  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  //wifiManager.setTimeout(120);
  //wifiManager.setDebugOutput(true);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect()
    ) {
    ALERT("Failed to connect to WiFi after tiemout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
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
}

void _OTAUpdate_setup() {
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

void wifi_setup()
{
  _gotIpEventHandler = WiFi.onStationModeGotIP(_wifi_connect_callback);
  _wifiMgr_setup();
  _OTAUpdate_setup();

  _disconnectedEventHandler = WiFi.onStationModeDisconnected(_wifi_disconnect_callback);
}

void wifi_loop() 
{
  ArduinoOTA.handle();
}

// Local Variables:
// mode: C++
// c-basic-offset: 2
// End:
