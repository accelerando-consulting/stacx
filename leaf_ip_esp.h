#pragma once

//#include <DNSServer.h>
#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#endif
#include <WiFiClient.h>
#ifdef ESP32
#include <WiFi.h>
#include <WiFiMulti.h>
#include "esp_system.h"
#include <HTTPClient.h>
#include <ESP32_FTPClient.h>
#endif

#include "abstract_ip.h"
#include "ip_client_wifi.h"

//@***************************** constants *******************************

#ifndef USE_OTA
#define USE_OTA 1
#endif

#ifndef USE_TELNETD
#define USE_TELNETD 0
#endif

#ifndef IP_WIFI_OWN_LOOP
#define IP_WIFI_OWN_LOOP false
#endif

#ifndef IP_WIFI_USE_AP
#define IP_WIFI_USE_AP true
#endif

#ifndef USE_TELNETD_DEFAULT
#define USE_TELNETD_DEFAULT false
#endif

#if USE_OTA
#include <ArduinoOTA.h>
#endif

#if IP_WIFI_USE_AP
#include <WiFiManager.h>
#include <WiFiServer.h>
#endif

//
//@**************************** class IpEsp ******************************
//
// This class encapsulates the wifi interface on esp8266 or esp32
//

class IpEspLeaf : public AbstractIpLeaf
{
public:
  IpEspLeaf(String name, String target="", bool run=true)
    : AbstractIpLeaf(name, target)
    , Debuggable(name)
  {
    LEAF_ENTER(L_INFO);
    this->run = run;
    this->own_loop = IP_WIFI_OWN_LOOP;
    LEAF_LEAVE;
  }
  virtual void setup();
  virtual void loop();
#if IP_WIFI_USE_AP
  virtual void ipConfig(bool reset=false);
#endif
  virtual void start();
  virtual void stop();
  virtual void recordWifiConnected(IPAddress addr) {
    // do minimum work here as this is expected to be called from on OS callback
    ip_addr_str = addr.toString();
    ip_wifi_known_state=true; /* loop will act on this */
#if DEBUG_SYSLOG
    if (debug_syslog_enable) {
      LEAF_WARN("Activating syslog client");
      debug_syslog_ready=true;
    }
#endif
  }
  virtual void recordWifiDisconnected(int reason) {
    // do minimum work here as this is expected to be called from on OS callback
    ip_wifi_disconnect_reason = reason;
    ip_wifi_known_state=false; /* loop will act on this */
#if DEBUG_SYSLOG
    if (debug_syslog_enable) {
      LEAF_WARN("Deactivating syslog client");
      debug_syslog_ready=false;
    }
#endif
  }
  virtual void ipPullUpdate(String url);
  virtual void ipRollbackUpdate(String url);
  virtual void ipOnConnect();
  virtual void ipOnDisconnect();
  virtual bool ftpPut(String host, String user, String pass, String path, const char *buf, int buf_len);
  virtual bool ipConnect(String reason="");
  virtual void mqtt_do_subscribe();
  virtual bool commandHandler(String type, String name, String topic, String payload);

  virtual Client *newClient(int slot);

  int wifi_retry = 3;
  static const int wifi_multi_max=8;
private:
  //
  // Network resources
  //
  WiFiClient espClient;
#ifdef ESP8266
  ESP8266WiFiMulti wifiMulti;
#else
  WiFiMulti wifiMulti;
#endif
  int wifi_multi_ssid_count=0;
#if USE_TELNETD
  bool ip_use_telnetd = USE_TELNETD_DEFAULT;
  bool ip_telnet_log = false;
  bool ip_telnet_shell = true;
  int ip_telnet_port = 23;
  int ip_telnet_timeout = 10;
  int ip_telnet_pass_min = 8;
  String ip_telnet_pass = ""; // empty means NO, we are not that kind of company
  WiFiServer *telnetd = NULL;
  WiFiClient telnet_client;
  Stream *default_debug_stream;
  bool has_telnet_client=false;
#ifdef _LEAF_SHELL_H_
  Stream *default_shell_stream;
#endif //shell
#endif // telnet
  int wifi_multi_timeout_msec = 30000;
  String wifi_multi_ssid[wifi_multi_max];
  String wifi_multi_pass[wifi_multi_max];
  bool ip_wifi_use_ap = IP_WIFI_USE_AP;
  bool ip_wifi_fallback_lte = false;

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
  String ap_ip_str = ap_ip.toString();

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

  //describe_taps(L_INFO);

  WiFi.persistent(false); // clear settings
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  WiFi.mode(WIFI_STA);
  WiFi.hostname(device_id);
  WiFi.setHostname(device_id);

  registerCommand(HERE,"ip_wifi_connect","initiate wifi connect");
  registerCommand(HERE,"ip_wifi_scan","perform a wifi SSID scan");
  registerCommand(HERE,"ip_wifi_status","report status of wifi connection");
  registerCommand(HERE,"ip_wifi_signal","report wifi signal strength");
  registerCommand(HERE,"ip_wifi_network","report wifi network status");
  registerCommand(HERE,"ip_wifi_disconnect","disconnect wifi");

#if USE_TELNETD
  if (telnetd!=NULL) delete telnetd;
  telnetd = NULL;
  registerBoolValue("ip_use_telnetd", &ip_use_telnetd, "Enable diagnostic connection via telnet");
  registerBoolValue("ip_telnet_shell", &ip_telnet_shell, "Divert command shell to telenet client when present");
  registerBoolValue("ip_telnet_log", &ip_telnet_log, "Divert log stream to telnet clinet when present");
  registerIntValue("ip_telnet_port", &ip_telnet_port, "TCP port for telnet (if enabled)");
  registerIntValue("ip_telnet_timeout", &ip_telnet_timeout, "Password timeout for telnet");
  registerStrValue("ip_telnet_pass", &ip_telnet_pass, "Password for telnet access (NO ACCESS IF NOT SET)");
#endif

#ifdef ESP32
  registerBoolValue("ip_wifi_own_loop", &own_loop, "Use a separate thread for wifi connection management");
#endif
#if IP_WIFI_USE_AP
  registerBoolValue("ip_wifi_use_ap", &ip_wifi_use_ap, "Spawn an access point if no known wifi network found");
#endif
  registerBoolValue("ip_wifi_fallback_lte", &ip_wifi_fallback_lte, "Fall back to LTE (where present) if no known wifi network found");

  for (int i=0; i<wifi_multi_max; i++) {
    wifi_multi_ssid[i]="";
  }

#ifdef IP_WIFI_AP_0_SSID
  wifi_multi_ssid[0] = IP_WIFI_AP_0_SSID;
  wifi_multi_pass[0] = IP_WIFI_AP_0_PASS; // mul tee pass
#ifdef IP_WIFI_AP_1_SSID
  wifi_multi_ssid[1] = IP_WIFI_AP_1_SSID;
  wifi_multi_pass[1] = IP_WIFI_AP_1_PASS; // mul tee pass
#ifdef IP_WIFI_AP_2_SSID
  wifi_multi_ssid[2] = IP_WIFI_AP_2_SSID;
  wifi_multi_pass[2] = IP_WIFI_AP_2_PASS; // mul tee pass
#ifdef IP_WIFI_AP_3_SSID
  wifi_multi_ssid[3] = IP_WIFI_AP_3_SSID;
  wifi_multi_pass[3] = IP_WIFI_AP_3_PASS; // mul tee pass
#endif
#endif
#endif
#endif

  for (int i=0; i<wifi_multi_max; i++) {
    registerStrValue(String("ip_wifi_ap_")+String(i)+"_name", wifi_multi_ssid+i, (i==0)?"Wifi Access point N name":"");
    registerStrValue(String("ip_wifi_ap_")+String(i)+"_pass", wifi_multi_pass+i, (i==0)?"Wifi Access point N password":""); // mul tee pass
    if (wifi_multi_ssid[i].length()) {
      LEAF_NOTICE("Access point #%d: [%s]", i+1, wifi_multi_ssid[i].c_str());
    }
  }


#ifdef ESP8266
  _gotIpEventHandler = WiFi.onStationModeGotIP(
    [](const WiFiEventStationModeGotIP& event){
      IpEspLeaf *that = (IpEspLeaf *)Leaf::get_leaf(leaves, "ip", "wifi");
      if (that) {
	that->recordWifiConnected(event.ip);
      }
    });

  _disconnectedEventHandler = WiFi.onStationModeDisconnected(
    [](const WiFiEventStationModeDisconnected& Event) {
      IpEspLeaf *that = (IpEspLeaf *)Leaf::get_leaf(leaves, "ip", "wifi");
      if (that) {
	that->recordWifiDisconnected((int)Event.reason);
      }
    });
#else // ESP32
  WiFi.onEvent(
    [](arduino_event_id_t event, arduino_event_info_t info) {
      IpEspLeaf *that = (IpEspLeaf *)Leaf::get_leaf(leaves, "ip", "wifi");
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
      IpEspLeaf *that = (IpEspLeaf *)Leaf::get_leaf(leaves, "ip", "wifi");
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

void IpEspLeaf::start(void)
{
  Leaf::start();
  LEAF_ENTER(L_INFO);

  ip_wifi_known_state = false;
#if USE_TELNETD
  default_debug_stream = debug_stream;
#ifdef _LEAF_SHELL_H_
  default_shell_stream = shell_stream;
#endif
#endif

  if (isAutoConnect()) {
    ipSetReconnectDue();
  }
  started=true;

  LEAF_VOID_RETURN;
}

void IpEspLeaf::stop()
{
#ifdef ESP32
  WiFi.mode( WIFI_MODE_NULL );
#endif
}

bool IpEspLeaf::ipConnect(String reason)
{
  LEAF_ENTER_STR(L_NOTICE, leaf_priority);
  if (!AbstractIpLeaf::ipConnect(reason)) {
    // Superclass said no can do
    return(false);
  }

  bool use_multi = false;

  wifi_multi_ssid_count = 0;
  for (int i=0; i<wifi_multi_max; i++) {
    if (wifi_multi_ssid[i].length() > 0) {
      wifi_multi_ssid_count ++;
      LEAF_NOTICE("Configured wifi AP %d: %s", i+1, wifi_multi_ssid[i].c_str());

      wifiMulti.addAP(wifi_multi_ssid[i].c_str(), wifi_multi_pass[i].c_str()); // MUL! TEE! PASS!
    }
  }

  if (wifi_multi_ssid_count > 1) {
    unsigned long now = millis();
    unsigned long whinge = now;
    unsigned long until = now + wifi_multi_timeout_msec;
    WiFi.setHostname(device_id);
    LEAF_NOTICE("Activating multi-ap wifi (%d APs)", wifi_multi_ssid_count);
    while (millis() < until) {
      if(wifiMulti.run() == WL_CONNECTED) {
	ip_rssi=(int)WiFi.RSSI();
	ip_ap_name = WiFi.SSID();
	LEAF_NOTICE("Wifi connected via wifiMulti \"%s\" RSSI=%d",ip_ap_name.c_str(), ip_rssi);
	recordWifiConnected(WiFi.localIP());
	// don't set ip_connected nor call onConnect here, leave it for the main loop
	break;
      }
      else {
	now = millis();
	if (now > (whinge + 5000)) {
	  LEAF_NOTICE("WifiMulti did bupkis so far...");
	  whinge = now;
	}
      }
    }
  }

  if (ip_wifi_known_state) {
    LEAF_INFO("IP is connected"); // but wait for the loop to publish this fact

    // Wait a few seconds for NTP result (give up if taking too long)
    if (ip_time_source == 0) {
      int wait = 4;
      while (wait) {
	time_t now = time(NULL);
	if (now > 1674802046) {
	  LEAF_NOTICE("Got time from NTP %s", ctime(&now));
	  setTimeSource(TIME_SOURCE_NTP);
	  break;
	}
	LEAF_NOTICE("Wait for NTP...");
	delay(1000);
	--wait;
      }
    }
  }
  else {
    LEAF_NOTICE("No IP connection, falling back to wifi manager");
    wifiMgr_setup(false);
  }
  LEAF_BOOL_RETURN(ip_wifi_known_state);
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
      LEAF_WARN("Recognizing wifi disconnected state (reason %d)", ip_wifi_disconnect_reason);
      this->ipOnDisconnect();
    }
  }

  // Now let the superclass do its normal thing (eg. notify about any changes of state)
  AbstractIpLeaf::loop();

  if (!isConnected(HERE)) return;

  if (ip_time_source == 0) {
    static unsigned long last_time_check_msec = 0;
    unsigned long msec = millis();

    if (msec > (last_time_check_msec+1000)) {
      last_time_check_msec = msec;
      time_t now = time(NULL);
      if (now > 1674802046) {
	LEAF_NOTICE("Got time from NTP %s", ctime(&now));
	setTimeSource(TIME_SOURCE_NTP);
      }
    }
  }

#if USE_OTA
  if (ip_enable_ota) {
    ArduinoOTA.handle();
  }
#endif

#if USE_TELNETD
  // Check for dead clients
  if (has_telnet_client && !telnet_client.connected()) {
    LEAF_NOTICE("Telnet client disconnected");
#ifdef _LEAF_SHELL_H_
    if (ip_telnet_shell) {
      shell_stream = default_shell_stream;
      shell_stream->flush();
      shell_stream->println("Reverted console from telnet session");
      shell_stream->flush();
    }
#endif
    if (ip_telnet_log) {
      debug_stream = default_debug_stream;
      DBGPRINTLN("Reverted debug stream from telnet session");
      debug_stream->flush();
    }
    has_telnet_client = false;
  }

  // Check for new telnet clients
  if (ip_connected && telnetd && telnetd->hasClient()) {
    if (has_telnet_client && telnet_client.connected()) {
      telnet_client.println("Terminating session");
      telnet_client.stop();
      if (ip_telnet_log) {
	debug_stream = default_debug_stream;
	DBGPRINTLN("Reverted debug stream from telnet session");
      }
#ifdef _LEAF_SHELL_H_
      if (ip_telnet_shell) {
	shell_stream = default_shell_stream;
	shell_stream->println("Reverted console from telnet session");
      }
#endif
      has_telnet_client = false;

    }
    LEAF_NOTICE("New telnet client");
    telnet_client = telnetd->available();
    if (telnet_client) {
      LEAF_NOTICE("Accepted telnet connection from %s", telnet_client.remoteIP().toString().c_str());
      has_telnet_client = true;
      telnet_client.println("Connected to Stacx");
      telnet_client.setTimeout(ip_telnet_timeout);
      int pwd_tries=3;
      while (pwd_tries > 0) {
	char pwd_buf[33];
	char pwd_len=0;
	unsigned long pwd_start=0;

	LEAF_NOTICE("Prompting for password (%d tries left)", pwd_tries);
	telnet_client.print("Password (hah, you thought it'd be this easy?): ");
	telnet_client.flush();
	pwd_len = telnet_client.readBytesUntil('\n', pwd_buf, sizeof(pwd_buf)-1);
	while ((pwd_len > 0) && ((pwd_buf[pwd_len-1]=='\r') || (pwd_buf[pwd_len-1]=='\n'))) {
	  // drop trailing CRLF kibble if any
	  --pwd_len;
	  pwd_buf[pwd_len]='\0';
	}
	if (pwd_len==0) {
	  telnet_client.println();
	  telnet_client.println("Bye, Felicia.");
	  telnet_client.stop();
	  pwd_tries = 0;
	  break;
	}

	pwd_buf[pwd_len]='\0';
	LEAF_NOTICE("Password supplied is [%s]", pwd_buf); // nocommit
	if (pwd_len<ip_telnet_pass_min) {
	  LEAF_NOTICE("Password too short (%d < %d)", pwd_len, ip_telnet_pass_min);
	  --pwd_tries;
	  continue;
	}
	if (strncmp(pwd_buf, ip_telnet_pass.c_str(),sizeof(pwd_buf)-1) !=0 ) {
	  LEAF_NOTICE("Password mismatch", pwd_len, ip_telnet_pass_min);
	  --pwd_tries;
	  continue;
	}
	LEAF_NOTICE("Telnet password accepted");

#ifdef _LEAF_SHELL_H_
	if (ip_telnet_shell) {
	  DBGPRINTLN("Diverting shell console to telnet client");
	  debug_stream->flush();
	  shell_stream = &telnet_client;
	  telnet_client.println("Shell console diverted to this client");
	  telnet_client.flush();
	}
#endif
	if (ip_telnet_log) {
	  DBGPRINTLN("Diverting diagnostic log to telnet client");
	  debug_stream->flush();
	  debug_stream = &telnet_client;
	  telnet_client.println("Log stream diverted to this client");
	  telnet_client.flush();
	}
	break;
      } // end while pwd_tries
      if (pwd_tries==0) {
	telnet_client.println("Bye, Felicia.");
	telnet_client.stop();
      }
    }
    else {
      LEAF_WARN("Telnet accept failed");
    }
  }
#endif // USE_TELNETD

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

  MDNS.begin(device_id);

#if USE_TELNETD
  if (ip_use_telnetd) {
    LEAF_NOTICE("Listening for telnet connections at %s:%d", ip_addr_str.c_str(), ip_telnet_port);
    if (telnetd) delete(telnetd);
    telnetd = new WiFiServer(ip_telnet_port);
  }
#endif // USE_TELNETD

#if USE_OTA
  if (ip_enable_ota) {
    OTAUpdate_setup();
  }
  else {
    LEAF_NOTICE("OTA update over wifi is disabled");
  }
#endif

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

#if USE_TELNETD
  if (telnetd) {
    telnetd->begin();
  }
#endif
}

void IpEspLeaf::ipOnDisconnect()
{
  AbstractIpLeaf::ipOnDisconnect();
  LEAF_ENTER(L_NOTICE);

#if USE_TELNETD
  if (telnetd) {
    telnetd->end();
  }
#endif

  post_error(POST_ERROR_MODEM, 3);
  ERROR("WiFi disconnect");

#ifdef NETWORK_DISCONNECT_REBOOT
  reboot();
#else
  ipScheduleReconnect();
#endif

  LEAF_VOID_RETURN;
}

void IpEspLeaf::mqtt_do_subscribe()
{
  AbstractIpLeaf::mqtt_do_subscribe();
}

bool IpEspLeaf::commandHandler(String type, String name, String topic, String payload) {
  LEAF_HANDLER(L_INFO);

  WHEN("ip_wifi_status",{
      ipStatus("ip_wifi_status");
    })
  ELSEWHEN("ip_wifi_signal",{
      ip_rssi = (int)WiFi.RSSI();
      mqtt_publish("status/ip_wifi_signal", String(ip_rssi, 10));
    })
  ELSEWHEN("ip_wifi_network",{
      String ip_ap_name = WiFi.SSID();
      mqtt_publish("status/ip_wifi_network", ip_ap_name);
    })
  ELSEWHEN("ip_wifi_connect",ipConnect("cmd");)
  ELSEWHEN("ip_wifi_disconnect",ipDisconnect();)
  ELSEWHEN("ip_wifi_scan",{
    LEAF_NOTICE("Doing WiFI SSID scan");
    int n = WiFi.scanNetworks();
    mqtt_publish("status/ip_wifi_scan_count", String(n));
    for (int i=0; i<n; i++) {
      mqtt_publish("status/ip_wifi_scan_result_"+String(i), WiFi.SSID(i));
      mqtt_publish("status/ip_wifi_scan_signal_"+String(i), String(WiFi.RSSI(i)));
    }
  })
  else handled = AbstractIpLeaf::commandHandler(type,name,topic,payload);
  
  LEAF_HANDLER_END;
}

#if IP_WIFI_USE_AP
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
#if IP_WIFI_USE_AP
  wifiManager.setAPCallback(
    [](WiFiManager *mgr) {
      IpEspLeaf *that = (IpEspLeaf *)Leaf::get_leaf_by_type(leaves, String("ip"));
      if (that) {
	that->onSetAP();
	that->publish("_wifi_ap", "0");
      }
    }
    );
#endif
  
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
    publish("_wifi_ap", "1");
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
  ipCommsState(TRY_IP,HERE);
  if (!wifiManager.autoConnect(ap_ssid)) {
    ALERT("Failed to connect to WiFi after timeout");
#if USE_OLED
    oled_text(0,20, "WiFi timeout");
#endif

#ifdef ESP32
    if (own_loop) {
      // we have our own thread, we can afford to retry all day
      ipScheduleReconnect();
    }
    else 
#endif
    {
      // reboot and hope for a better life next time

      delay(3000);
      //reset and try again, or maybe put it to deep sleep
      reboot();
      delay(5000);
    }
  }

  //if you get here you have connected to the WiFi
  ALERT("Connected to WiFi");

  // Wait a few seconds for NTP result (give up if taking too long)
  if (ip_time_source == 0) {
    int wait = 4;
    while (wait) {
      time_t now = time(NULL);
      if (now > 1674802046) {
	LEAF_NOTICE("Got time from NTP %s", ctime(&now));
	ip_time_source = TIME_SOURCE_NTP;
	break;
      }
      LEAF_NOTICE("Wait for NTP...");
      delay(1000);
      --wait;
    }
  }

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
#endif // IP_WIFI_USE_AP

void IpEspLeaf::wifiMgr_setup(bool reset)
{
  ENTER(L_INFO);
  NOTICE("Wifi manager setup commencing");

  if (ip_ap_name.length() || (wifi_multi_ssid_count==1)) {
    if (!ip_ap_name.length()) {
      ip_ap_name = wifi_multi_ssid[0];
      ip_ap_pass = wifi_multi_pass[0]; // mul tee pass
    }
    LEAF_NOTICE("Connecting to saved SSID %s", ip_ap_name.c_str());
    WiFi.begin(ip_ap_name.c_str(), ip_ap_pass.c_str());
    int wait = 40;
    while (wait && (WiFi.status() != WL_CONNECTED)) {
      delay(500);
      --wait;
      DBGPRINT(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
      DBGPRINTLN();
      recordWifiConnected(WiFi.localIP());
    }
  } else {
    reset = 1;
  }

  if (!ip_wifi_known_state) {
    LEAF_NOTICE("Unable to activate wifi, investigating alternatives");
    if (ip_wifi_use_ap) {
      // use wifimanager access point library
#if IP_WIFI_USE_AP
      ipConfig(reset);
#endif
    }
    else if (ip_wifi_fallback_lte) {
      AbstractIpLeaf *lte = (AbstractIpLeaf *)find("lte","ip");
      AbstractPubsubLeaf *lte_pubsub =(AbstractPubsubLeaf *)find("ltemqtt", "pubsub");
      if (lte && lte_pubsub) {
	// LTE fallback is enabled and LTE leaves are present.  Switch to those.
	LEAF_ALERT("Disabling wifi in favour of LTE fallback");
	stacxSetComms(lte, lte_pubsub);
      }
    }
    else {
      ipScheduleReconnect();
    }
    return;
  }
#if USE_OLED
  oled_text(0,20, WiFi.localIP().toString());
#endif
}

#if IP_WIFI_USE_AP
void IpEspLeaf::onSetAP()
{
  NOTICE("Created wifi AP: %s %s", WiFi.SSID().c_str(), WiFi.softAPIP().toString().c_str());
#if USE_OLED
  oled_text(0,10, String("WiFi Access Point:"));
  oled_text(0,20, WiFi.SSID());
  oled_text(0,30, String("WiFi IP: ")+WiFi.softAPIP().toString());
  oled_text(0,40, String("setup at http://")+WiFi.softAPIP().toString()+"/");
#endif
}
#endif

#if USE_OTA
void IpEspLeaf::OTAUpdate_setup() {
  ENTER(L_NOTICE);

  ArduinoOTA.setHostname(device_id);
  ArduinoOTA.setPassword(ota_password);
  LEAF_NOTICE("This device supports OTA firmware update as \"%s\" (%s)", device_id, ip_addr_str.c_str());

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
#endif // USE_OTA

void IpEspLeaf::ipRollbackUpdate(String url)
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

void IpEspLeaf::ipPullUpdate(String url)
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

  mqtt_publish("status/update", "begin");
  http.collectHeaders(want_headers, sizeof(want_headers)/sizeof(char*));

  int httpCode = http.GET();
  if(httpCode > 0) {
    // HTTP header has been send and Server response header has been handled
    INFO("HTTP update request returned code %d", httpCode);

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
	mqtt_publish("status/update", "abort");
	// not enough space to begin OTA
	// Understand the partitions and
	// space availability
	ALERT("Not enough space to begin firmware update");
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


bool IpEspLeaf::ftpPut(String host, String user, String pass, String path, const char *buf, int buf_len)
{
  LEAF_INFO("PUT %s", path.c_str());

#ifdef ESP8266
  LEAF_ALERT("FIXME: FTP is not operational for ESP8266");
#else
  ESP32_FTPClient ftp ((char *)host.c_str(), (char *)user.c_str(), (char *)pass.c_str(), 10000, 2);
  char dir[80];
  char name[40];

  ftp.OpenConnection();

  strlcpy(dir, path.c_str(), sizeof(dir));
  char *dirsep = strrchr(dir, '/');
  if (dirsep == NULL) {
    // Path contains no slash, presume root
    //LEAF_INFO("Upload path does not contain directory, presuming /home/ftp/images/");
    strlcpy(name, dir, sizeof(dir));
    strcpy(dir, "/home/ftp/images/");
  }
  else {
    // Split path into dir and name
    strlcpy(name, dirsep+1, sizeof(name));
    dirsep[1] = '\0';
    //LEAF_INFO("Split upload path into '%s' and '%s'\n", dir, name);
  }

  ftp.ChangeWorkDir(dir);
  ftp.InitFile("Type I");
  ftp.NewFile(name);
  ftp.WriteData((unsigned char *)buf, buf_len);
  ftp.CloseFile();
  ftp.CloseConnection();
#endif
  return true;

}

Client *IpEspLeaf::newClient(int slot) {
  return (Client *)(new IpClientWifi(slot));
}

// Local Variables:
// mode: C++
// c-basic-offset: 2
// End:
