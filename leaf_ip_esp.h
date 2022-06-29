#include <WiFiManager.h>
#include <DNSServer.h>
#ifdef ESP32
#include "esp_system.h"
#include <HTTPClient.h>
#endif
#include "abstract_storage.h"

//@***************************** constants *******************************

#ifndef USE_OTA
#define USE_OTA 1
#endif

#ifdef ESP8266
#define CONFIG_FILE "config.json"
#else
#define CONFIG_FILE "/config.json"
#endif

#if USE_OTA
#include <ArduinoOTA.h>
#endif

//
//@**************************** class IpEsp ******************************
//
// This class encapsulates the wifi interface on esp8266 or esp32
//

class IpEspLeaf : public AbstractIpLeaf
{
public:
  IpEspLeaf(String name, String target="", bool run=true) : AbstractIpLeaf(name, target) {
    LEAF_ENTER(L_INFO);
    this->run = run;
    this->impersonate_backplane = true;
    LEAF_LEAVE;
  }
  virtual void setup();
  virtual void loop();
  virtual void ipConfig();
  virtual void start();
  virtual void stop();
  virtual void pullUpdate(String url);
  virtual void rollbackUpdate(String url);
  virtual bool isConnected() { return wifiConnected; }
  int wifi_retry = 3;

private:
  //
  // Network resources
  //
  WiFiClient espClient;

  bool _shouldSaveConfig = false;
#ifdef ESP8266
  WiFiEventHandler _gotIpEventHandler, _disconnectedEventHandler;
#endif
  Ticker wifiReconnectTimer;
  bool wifiConnectNotified = false;

  char reformat[8] = "no";
  char ap_ssid[32]="";

  IPAddress ap_ip = IPAddress(192,168,  4,  1);
  IPAddress ap_gw = IPAddress(192,168,  4,  1);
  IPAddress ap_sn = IPAddress(255,255,255,  0);
  String ap_ip_str;

  void wifi_connect_callback(const char *ip_addr);
  void onDisconnect(void);
  bool readConfig();
  void writeConfig(bool force_format=false);
  void wifiMgr_setup(bool reset);
  void onSetAP();
  void OTAUpdate_setup();
  virtual void startConfig();
};

void IpEspLeaf::setup()
{
  AbstractIpLeaf::setup();
  LEAF_ENTER(L_INFO);

  WiFi.hostname(device_id);

#ifdef ESP8266
  _gotIpEventHandler = WiFi.onStationModeGotIP(
    [](const WiFiEventStationModeGotIP& event){
      IpEspLeaf *that = (IpEspLeaf *)Leaf::get_leaf_by_type(leaves, String("ip"));
      if (that) {
	that->wifi_connect_callback(event.ip.toString().c_str());
      }
    });

  _disconnectedEventHandler = WiFi.onStationModeDisconnected(
    [](const WiFiEventStationModeDisconnected& event) {
      ALERT("WiFi disconnected (reason %d)", (int)(event.reason));
      IpEspLeaf *that = (IpEspLeaf *)Leaf::get_leaf_by_type(leaves, String("ip"));
      if (that) {
	that->onDisconnect();
	if (that->wifi_retry) {
	  --that->wifi_retry;
	  delay(2000);
	  // retry the connection a few times before falling back
	  that->wifiMgr_setup(false);
	}
	else {
	  if (event.reason == WIFI_DISCONNECT_REASON_AUTH_FAIL) {
	    NOTICE("Auth failed, reverting to config portal");
	    delay(2000);
	    that->wifiMgr_setup(true);
	  }
	}
      }
    });
#else // ESP32
  WiFi.onEvent(
    [](arduino_event_id_t event, arduino_event_info_t info) {
      IpEspLeaf *that = (IpEspLeaf *)Leaf::get_leaf_by_type(leaves, String("ip"));
      if (that) {
	that->wifi_connect_callback(IPAddress(info.got_ip.ip_info.ip.addr).toString().c_str());
      }
    }
    ,
    ARDUINO_EVENT_WIFI_STA_GOT_IP
    );

  WiFi.onEvent(
    [](arduino_event_id_t event, arduino_event_info_t info)
    {
      ALERT("WiFi disconnected (reason %d)", (int)(info.wifi_sta_disconnected.reason));
      IpEspLeaf *that = (IpEspLeaf *)Leaf::get_leaf_by_type(leaves, String("ip"));
      if (that) {
	if (that->isConnected()) that->onDisconnect();
      }
    },
    ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
#endif

  LEAVE;
}

void IpEspLeaf::start()
{
  Leaf::start();

  wifiMgr_setup(false);
#if USE_OTA
  OTAUpdate_setup();
#endif
}

void IpEspLeaf::ipConfig() 
{
  wifiMgr_setup(true);
}

void IpEspLeaf::stop()
{
#ifdef ESP32
  WiFi.mode( WIFI_MODE_NULL );
#endif
}

void IpEspLeaf::loop()
{

  if (wifiConnectNotified != wifiConnected) {
    // Tell other interested leaves that IP has changed state
    // (delay this to loop because if done in setup other leaves may not be
    // ready yet).
    // todo: make leaf::start part of core api
    if (wifiConnected) {
      LEAF_INFO("Publishing ip_connect %s", ip_addr_str);
      publish("_ip_connect", String(ip_addr_str));
    }
    else {
     publish("_ip_disconnect", "");
    }
    wifiConnectNotified = wifiConnected;
  }



  AbstractIpLeaf::loop();
#if USE_OTA
  ArduinoOTA.handle();
#endif

}

bool IpEspLeaf::readConfig()
{
  LEAF_ENTER(L_INFO);

  StorageLeaf *prefs = prefsLeaf;
  // todo: refactor
  if (prefs) {
    String value;

    value = prefs->get("sta_ssid");
    if (value.length() > 0) {
      LEAF_INFO("Read preference sta_ssid=[%s]", value.c_str());
      strlcpy(sta_ssid, value.c_str(), sizeof(sta_ssid));
    }

    value = prefs->get("sta_pass");
    if (value.length() > 0) {
      LEAF_INFO("Read preference sta_pass_host=[%s]", value.c_str());
      strlcpy(sta_pass, value.c_str(), sizeof(sta_pass));
    }

    value = prefs->get("mqtt_host");
    if (value.length() > 0) {
      LEAF_INFO("Read preference mqtt_host=[%s]", value.c_str());
      strlcpy(mqtt_host, value.c_str(), sizeof(mqtt_host));
    }

    value = prefs->get("mqtt_port");
    if (value.length() > 0) {
      LEAF_INFO("Read preference mqtt_port=[%s]", value.c_str());
      strlcpy(mqtt_port, value.c_str(), sizeof(mqtt_port));
    }

    value = prefs->get("mqtt_user");
    if (value.length() > 0) {
      LEAF_INFO("Read preference mqtt_user=[%s]", value.c_str());
      strlcpy(mqtt_user, value.c_str(), sizeof(mqtt_user));
    }

    value = prefs->get("mqtt_pass");
    if (value.length() > 0) {
      LEAF_INFO("Read preference mqtt_pass=[%s]", value.c_str());
      strlcpy(mqtt_pass, value.c_str(), sizeof(mqtt_pass));
    }

    value = prefs->get("device_id");
    if (value.length() > 0) {
      LEAF_INFO("Read preference device_id=[%s]", value.c_str());
      strlcpy(device_id, value.c_str(), sizeof(device_id));
    }

    value = prefs->get("ota_password");
    if (value.length() > 0) {
      LEAF_INFO("Read preference ota_password=[%s]", value.c_str());
      strlcpy(ota_password, value.c_str(), sizeof(ota_password));
    }
  }
  else {
    LEAF_ALERT("No preferences module found");
  }

  LEAF_LEAVE;
  return true;
}

void IpEspLeaf::writeConfig(bool force_format)
{
  LEAF_ENTER(L_INFO);

  ALERT("saving config to flash");

  StorageLeaf *prefs = (StorageLeaf *)get_tap("prefs");
  if (prefs) {
    prefs->put("mqtt_host", mqtt_host);
    prefs->put("mqtt_port", mqtt_port);
    prefs->put("mqtt_user", mqtt_user);
    prefs->put("mqtt_pass", mqtt_pass);
    prefs->put("device_id", device_id);
    prefs->put("ota_password", ota_password);
    prefs->save(force_format);
  }
  LEAF_LEAVE;
}

void  IpEspLeaf::wifi_connect_callback(const char *ip_addr) {
  strlcpy(ip_addr_str, ip_addr, sizeof(ip_addr_str));
  NOTICE("WiFi connected, IP: %s", ip_addr_str);

  // Get the time from NTP server
#ifdef ESP8266
#ifdef USE_NTP
  configTime(TZ_Australia_Brisbane, "pool.ntp.org");
#endif
#else // ESP32
  configTime(10*60*60, 10*60*60, "pool.ntp.org");

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    ALERT("Failed to obtain time");
  }
#endif

  wifiConnected = true;
  idle_pattern(500,50);
}

void IpEspLeaf::onDisconnect(void)
{
  LEAF_ENTER(L_ALERT);

  wifiConnected = false;
  post_error(POST_ERROR_MODEM, 3);
  ERROR("Modem disconnect");
  idle_pattern(200,50);

  publish("_ip_disconnect", String(NETWORK_RECONNECT_SECONDS).c_str());
#ifdef NETWORK_DISCONNECT_REBOOT
  reboot();
#else
  wifiReconnectTimer.once(
    NETWORK_RECONNECT_SECONDS,
    [](){
      IpEspLeaf *that = (IpEspLeaf *)Leaf::get_leaf_by_type(leaves, String("ip"));
      if (that) {
	if (that->isConnected()) {
	  NOTICE("Ignore reconnect, already connected");
	}
	else {
	  that->setup();
	}
      }
    });
#endif
  LEAF_LEAVE;
}

void IpEspLeaf::wifiMgr_setup(bool reset)
{
  ENTER(L_INFO);
  LEAF_NOTICE("Wifi manager setup commencing");
  if (!readConfig()) {
    LEAF_ALERT("Could not read configuration file, forcing config portal");
    reset= true;
  }

  if (strlen(sta_ssid)>1) {
    LEAF_NOTICE("Connecting to saved SSID %s", sta_ssid);
    WiFi.begin(sta_ssid, sta_pass);
    int wait = 40;
    while (wait && (WiFi.status() != WL_CONNECTED)) {
      delay(500);
      --wait;
      Serial.print(".");
    }
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println();
      wifiConnected = true;
    }
  }

  if (!wifiConnected) {
#ifdef DEVICE_ID_APPEND_MAC
    strlcpy(ap_ssid, device_id, sizeof(ap_ssid));
#else
    snprintf(ap_ssid, sizeof(ap_ssid), "%s_%s", device_id, mac_short);
#endif
    INFO("Using AP SSID [%s]", ap_ssid);

    // The extra parameters to be configured (can be either global or just in the setup)
    // After connecting, parameter.getValue() will get you the configured value
    // id/name placeholder/prompt default length
#ifdef USE_WIFIMGR_CONFIG
    WiFiManagerParameter custom_mqtt_host("mqtt_host", "mqtt server", mqtt_host, sizeof(mqtt_host));
    WiFiManagerParameter custom_mqtt_port("mqtt_port", "mqtt port", mqtt_port, sizeof(mqtt_port));
    WiFiManagerParameter custom_mqtt_user("mqtt_user", "mqtt username", mqtt_user, sizeof(mqtt_user));
    WiFiManagerParameter custom_mqtt_pass("mqtt_pass", "mqtt password", mqtt_pass, sizeof(mqtt_pass));
    WiFiManagerParameter custom_device_id("device_id", "Device ID", device_id, sizeof(device_id));
    WiFiManagerParameter custom_ota_password("ota_password", "Update Password", ota_password, sizeof(ota_password));
    WiFiManagerParameter custom_reformat("reformat", "Force format", reformat, sizeof(reformat));
#endif

    //WiFiManager
    //Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wifiManager;

    //set config save notify callback
    wifiManager.setSaveConfigCallback(
      [](){
	IpEspLeaf *that = (IpEspLeaf *)Leaf::get_leaf_by_type(leaves, String("ip"));
	if (that) that->_shouldSaveConfig=true;
      });
    wifiManager.setAPCallback(
      [](WiFiManager *mgr) {
	IpEspLeaf *that = (IpEspLeaf *)Leaf::get_leaf_by_type(leaves, String("ip"));
	if (that) {
	  that->onSetAP();
	}
      }
      );



    //set static ip
    ap_ip_str = ap_ip.toString();
    INFO("AP will use static ip %s", ap_ip_str.c_str());
    wifiManager.setAPStaticIPConfig(ap_ip, ap_gw, ap_sn);
    //wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));

    //add all your parameters here
#ifdef USE_WIFIMGR_CONFIG
    wifiManager.addParameter(&custom_mqtt_host);
    wifiManager.addParameter(&custom_mqtt_port);
    wifiManager.addParameter(&custom_mqtt_user);
    wifiManager.addParameter(&custom_mqtt_pass);
    wifiManager.addParameter(&custom_device_id);
    wifiManager.addParameter(&custom_ota_password);
    wifiManager.addParameter(&custom_reformat);
#endif

    //reset settings - for testing
#ifdef CLEAR_PIN
    pinMode(CLEAR_PIN, INPUT_PULLUP);
    delay(50);
    if (digitalRead(CLEAR_PIN) == LOW) {
      ALERT("Settings reset via pin %d low", CLEAR_PIN);
      reset=true;
    }
    pinMode(CLEAR_PIN, INPUT);
#endif

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
    //wifiManager.setDebugOutput(true);

    //fetches ssid and pass and tries to connect
    //if it does not connect it starts an access point with the specified name
    //and goes into a blocking loop awaiting configuration
#if USE_OLED
    oled_text(0,10, "Joining wifi...");
#endif

#ifdef ESP8266
  ESP.wdtFeed();
  ESP.wdtDisable();
#endif
  if (!wifiManager.autoConnect(ap_ssid)) {
    ALERT("Failed to connect to WiFi after timeout");
#if USE_OLED
      oled_text(0,20, "WiFi timeout");
#endif
      delay(3000);
      //reset and try again, or maybe put it to deep sleep
      reboot();
      delay(5000);
    }
  }
#ifdef ESP8266
  ESP.wdtEnable(0);
#endif

  //if you get here you have connected to the WiFi
  NOTICE("Connected to WiFi");
  wifiConnected = true;

  //read updated parameters
#ifdef USE_WIFIMGR_CONFIG
  strlcpy(mqtt_host, custom_mqtt_host.getValue(),sizeof(mqtt_host));
  strlcpy(mqtt_port, custom_mqtt_port.getValue(), sizeof(mqtt_port));
  strlcpy(mqtt_user, custom_mqtt_user.getValue(),sizeof(mqtt_user));
  strlcpy(mqtt_pass, custom_mqtt_pass.getValue(), sizeof(mqtt_pass));
  strlcpy(device_id, custom_device_id.getValue(), sizeof(device_id));
  strlcpy(ota_password, custom_ota_password.getValue(), sizeof(ota_password));
  strlcpy(reformat, custom_reformat.getValue(), sizeof(reformat));
#endif

  bool force_format = false;
  if (strcasecmp(reformat, "yes") == 0) force_format = true;
  //save the custom parameters to FS
  if (_shouldSaveConfig) {
    writeConfig(force_format);
  }

  MDNS.begin(device_id);

  LEAF_NOTICE("My IP Address: %s", WiFi.localIP().toString().c_str());
#if USE_OLED
  oled_text(0,20, WiFi.localIP().toString());
#endif

}

void IpEspLeaf::onSetAP()
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


#if USE_OTA
void IpEspLeaf::OTAUpdate_setup() {
  ENTER(L_INFO);

  ArduinoOTA.setHostname(device_id);
  ArduinoOTA.setPassword(ota_password);

  ArduinoOTA.onStart(
    [](){
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH) {
	type = "sketch";
      } else { // U_SPIFFS
	type = "filesystem";
      }

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      ALERT("Start OTA update (%s)", type.c_str());
    });
  ArduinoOTA.onEnd(
    []() {
      ALERT("OTA Complete");
    });
  ArduinoOTA.onProgress(
    [](unsigned int progress, unsigned int total) {
      NOTICE("OTA in progress: %3u%% (%u/%u)", (progress / (total / 100)), progress, total);
    });
  ArduinoOTA.onError(
    [](ota_error_t error) {
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

void IpEspLeaf::rollbackUpdate(String url)
{
#ifdef ESP32
  if (Update.canRollBack()) {
    ALERT("Rolling back to previous version");
    if (Update.rollBack()) {
      NOTICE("Rollback succeeded.  Rebooting.");
      delay(1000);
      reboot();
    }
    else {
      ALERT("Rollback failed");
    }
  }
  else
#endif
  {
    ALERT("Rollback is not possible");
  }
}


void IpEspLeaf::pullUpdate(String url)
{
#ifdef ESP32
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
	Update.onProgress(
	  [](size_t done, size_t size) {
	    NOTICE("Update progress %lu / %lu", done, size);
	  });
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
	    reboot();
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
#else
  LEAF_ALERT("OTA not implemented yet for ESP8266");
#endif  
}
#else
void IpEspLeaf::pull_update(String url)
{
  LEAF_ALERT("OTA not supported");
}

void IpEspLeaf::rollback_update(String url);
{
  LEAF_ALERT("OTA not supported");
}

#endif // USE_OTA

// Local Variables:
// mode: C++
// c-basic-offset: 2
// End:
