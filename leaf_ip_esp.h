#pragma once

#include <WiFiManager.h>
#include <WiFiMulti.h>
#include <DNSServer.h>
#ifdef ESP32
#include "esp_system.h"
#include <HTTPClient.h>
#endif
#include <ESP32_FTPClient.h>

#include "abstract_ip.h"

//@***************************** constants *******************************

#ifndef USE_OTA
#define USE_OTA 1
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
    LEAF_LEAVE;
  }
  virtual void setup();
  virtual void loop();
  virtual void ipConfig(bool reset=false);
  virtual void start();
  virtual void stop();
  virtual void recordWifiConnected(IPAddress addr) {
    // do minimum work here as this is expected to be called from on OS callback
    ip_addr_str = addr.toString();
    ip_wifi_known_state=true; /* loop will act on this */
  }
  virtual void recordWifiDisconnected(int reason) {
    // do minimum work here as this is expected to be called from on OS callback
    ip_wifi_disconnect_reason = reason;
    ip_wifi_known_state=false; /* loop will act on this */
  }
  virtual void pullUpdate(String url);
  virtual void rollbackUpdate(String url);
  virtual void ipOnConnect();
  virtual void ipOnDisconnect();
  virtual bool ftpPut(String host, String user, String pass, String path, const char *buf, int buf_len);
  virtual bool ipConnect(String reason="")   {
    // fixme refactor the code that should be here, to put it here
    return true;
  }
    
  int wifi_retry = 3;
  static const int wifi_multi_max=8;
private:
  //
  // Network resources
  //
  WiFiClient espClient;
  WiFiMulti wifiMulti;
  int wifi_multi_timeout_msec = 30000;
  String wifi_multi_ssid[wifi_multi_max];
  String wifi_multi_pass[wifi_multi_max];

  bool _shouldSaveConfig = false;
#ifdef ESP8266
  WiFiEventHandler _gotIpEventHandler, _disconnectedEventHandler;
#endif
  Ticker wifiReconnectTimer;
  bool ip_wifi_known_state = false;
  int ip_wifi_disconnect_reason = 0;

#ifdef USE_NTP
  boolean syncEventTriggered = false; // True if a time even has been triggered
  NTPSyncEvent_t ntpEvent; // Last triggered event
#endif

  char reformat[8] = "no";
  char ap_ssid[32]="";

  IPAddress ap_ip = IPAddress(192,168,  4,  1);
  IPAddress ap_gw = IPAddress(192,168,  4,  1);
  IPAddress ap_sn = IPAddress(255,255,255,  0);
  String ap_ip_str;

  bool readConfig();
  void writeConfig(bool force_format=false);
  void wifiMgr_setup(bool reset);
  void onSetAP();
  void OTAUpdate_setup();
};

void IpEspLeaf::setup()
{
  AbstractIpLeaf::setup();
  LEAF_ENTER(L_INFO);

  WiFi.persistent(false); // clear settings
  WiFi.disconnect();  
  WiFi.mode(WIFI_OFF);
  WiFi.mode(WIFI_STA);
  WiFi.hostname(device_id);

  for (int i=0; i<wifi_multi_max; i++) {
    wifi_multi_ssid[i]="";
    getPref(String("ip_wifi_ap_name_")+String(i), wifi_multi_ssid+i, "Wifi Access point name");
    getPref(String("ip_wifi_ap_pass_")+String(i), wifi_multi_pass+i, "Wifi Access point password");
    if (wifi_multi_ssid[i].length()) {
      LEAF_NOTICE("Access point #%d: [%s], i+1, wifi_multi_ssid[i].c_str());
    }
  }
    

#ifdef ESP8266
  _gotIpEventHandler = WiFi.onStationModeGotIP(
    [](const WiFiEventStationModeGotIP& event){
      IpEspLeaf *that = (IpEspLeaf *)Leaf::get_leaf_by_type(leaves, String("ip"));
      if (that) {
	that->recordWifiConnected(event.ip);
      }
    });

  _disconnectedEventHandler = WiFi.onStationModeDisconnected(
    [](const WiFiEventStationModeDisconnected& Event) {
      IpEspLeaf *that = (IpEspLeaf *)Leaf::get_leaf_by_type(leaves, String("ip"));
      if (that) {
	that->recordWifiDisconnected((int)Event.reason);
      }
    });
#else // ESP32
  WiFi.onEvent(
    [](arduino_event_id_t event, arduino_event_info_t info) {
      IpEspLeaf *that = (IpEspLeaf *)Leaf::get_leaf_by_type(leaves, String("ip"));
      if (that) {
	that->recordWifiConnected(IPAddress(info.got_ip.ip_info.ip.addr));
      }
    }
    ,
    ARDUINO_EVENT_WIFI_STA_GOT_IP
    );

  WiFi.onEvent(
    [](arduino_event_id_t event, arduino_event_info_t info)
    {
      IpEspLeaf *that = (IpEspLeaf *)Leaf::get_leaf_by_type(leaves, String("ip"));
      if (that) {
	that->recordWifiDisconnected((int)(info.wifi_sta_disconnected.reason));
      }
    },
    ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
#endif

#ifdef USE_NTP
  NTP.onNTPSyncEvent ([](NTPSyncEvent_t event) {
			ntpEvent = event;
			syncEventTriggered = true;
		      });
#endif

  LEAVE;
}

void IpEspLeaf::start()
{
  Leaf::start();
  LEAF_ENTER(L_NOTICE);
  
  ip_wifi_known_state = false;
  bool use_multi = false;
  LEAF_NOTICE("Check if multi-AP config in use");
  for (int i=0; i<wifi_multi_max; i++) {
    if (wifi_multi_ssid[i].length() > 0) {
      use_multi = true;
      LEAF_NOTICE("Add configured AP %s", wifi_multi_ssid[i].c_str());
      wifiMulti.addAP(wifi_multi_ssid[i].c_str(), wifi_multi_pass[i].c_str()); // MUL! TEE! PASS!
    }
  }
  if (use_multi) {
    unsigned long until = millis() + wifi_multi_timeout_msec;
    while (millis() < until) {
      LEAF_NOTICE("Activating multi-ap wifi");
      if(wifiMulti.run() == WL_CONNECTED) {
	LEAF_NOTICE("Wifi connected via wifiMulti");
	recordWifiConnected(WiFi.localIP());
	break;
      }
      else {
	LEAF_NOTICE("WifiMulti did bupkis so far...");
      }
    }
  }
  
  if (ip_wifi_known_state) {
    this->ipOnConnect();
  }
  else {
    LEAF_NOTICE("No IP connection, falling back to wifi manager");
    wifiMgr_setup(false);
  }
#if USE_OTA
  OTAUpdate_setup();
#endif
  LEAF_VOID_RETURN;
}

void IpEspLeaf::stop()
{
#ifdef ESP32
  WiFi.mode( WIFI_MODE_NULL );
#endif
}

void IpEspLeaf::loop()
{
  if (ip_wifi_known_state != ip_connected) {
    // A callback has recorded a change of state, leaving this routine to do the rest
    if (ip_wifi_known_state) {
      LEAF_NOTICE("Recognizing wifi connected state");
      this->ipOnConnect();
    }
    else {
      LEAF_NOTICE("Recognizing wifi disconnected state (reason %d)", ip_wifi_disconnect_reason);
      this->ipOnDisconnect();
    }
  }

  // Now let the superclass do its normal thing (eg. notify about any changes of state)
  AbstractIpLeaf::loop();
#if USE_OTA
  ArduinoOTA.handle();
#endif

#ifdef USE_NTP
  if (syncEventTriggered) {
    processSyncEvent (ntpEvent);
    syncEventTriggered = false;
  }
#endif
}

void IpEspLeaf::ipOnConnect() 
{
  AbstractIpLeaf::ipOnConnect();
  LEAF_ENTER(L_NOTICE);
  NOTICE("WiFi connected, IP: %s", ip_addr_str.c_str());

  // reset some state-machine-esque values
  ip_wifi_disconnect_reason = 0;
  wifi_retry = 3;

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

  LEAF_NOTICE("Publishing ip_connect %s", ip_addr_str.c_str());
  publish("_ip_connect", ip_addr_str);
}
  
void IpEspLeaf::ipOnDisconnect() 
{
  AbstractIpLeaf::ipOnDisconnect();
  LEAF_ENTER(L_NOTICE);

  post_error(POST_ERROR_MODEM, 3);
  ERROR("Modem disconnect");

#ifdef NETWORK_DISCONNECT_REBOOT
  reboot();
#else
  ipScheduleReconnect();
#endif

  LEAF_VOID_RETURN;
}

    


void IpEspLeaf::writeConfig(bool force_format)
{
  LEAF_ENTER(L_NOTICE);

  ALERT("saving config to flash");

#ifdef USE_WIFIMGR_CONFIG
  if (prefsLeaf) {
    prefsLeaf->put("mqtt_host", mqtt_host);
    prefsLeaf->put("mqtt_port", mqtt_port);
    prefsLeaf->put("mqtt_user", mqtt_user);
    prefsLeaf->put("mqtt_pass", mqtt_pass);
    prefsLeaf->put("device_id", device_id);
    prefsLeaf->put("ota_password", ota_password);
    prefsLeaf->save(force_format);
  }
#endif
  LEAF_LEAVE;
}

void IpEspLeaf::ipConfig(bool reset) 
{
  ip_wifi_known_state = false;
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
  //wifiManager.setStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));

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
  idle_pattern(200,50,HERE);

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

  //if you get here you have connected to the WiFi
  ALERT("Connected to WiFi");
  // FIXME: is this needed or already handled by callback?
  if (ip_wifi_known_state == true) {
    ALERT("did not need to notify of connection");
  }
  recordWifiConnected(WiFi.localIP());

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
}

void IpEspLeaf::wifiMgr_setup(bool reset)
{
  ENTER(L_INFO);
  ALERT("Wifi manager setup commencing");

  if (ip_ap_name.length()) {
    LEAF_NOTICE("Connecting to saved SSID %s", ip_ap_name.c_str());
    WiFi.begin(ip_ap_name.c_str(), ip_ap_pass.c_str());
    int wait = 40;
    while (wait && (WiFi.status() != WL_CONNECTED)) {
      delay(500);
      --wait;
      Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println();
      recordWifiConnected(WiFi.localIP());
    }
  } else {
    reset = 1;
  }
  
  if (!ip_wifi_known_state) {
    ipConfig(reset);
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

bool IpEspLeaf::ftpPut(String host, String user, String pass, String path, const char *buf, int buf_len)
{
  LEAF_INFO("PUT %s", path.c_str());
  
  ESP32_FTPClient ftp ((char *)host.c_str(), (char *)user.c_str(), (char *)pass.c_str(), 10000, 2);
  char dir[80];
  char name[40];

  ftp.OpenConnection();

  strlcpy(dir, path.c_str(), sizeof(dir));
  char *dirsep = strrchr(dir, '/');
  if (dirsep == NULL) {
    // Path contains no slash, presume root
    LEAF_INFO("Upload path does not contain directory, presuming /home/ftp/images/");
    strlcpy(name, dir, sizeof(dir));
    strcpy(dir, "/home/ftp/images/");
  }
  else {
    // Split path into dir and name
    strlcpy(name, dirsep+1, sizeof(name));
    dirsep[1] = '\0';
    LEAF_INFO("Split upload path into '%s' and '%s'\n", dir, name);
  }

  ftp.ChangeWorkDir(dir);
  ftp.InitFile("Type I");
  ftp.NewFile(name);
  ftp.WriteData((unsigned char *)buf, buf_len);
  ftp.CloseFile();
  ftp.CloseConnection();
  return true;
  
}


// Local Variables:
// mode: C++
// c-basic-offset: 2
// End:
