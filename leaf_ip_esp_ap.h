#pragma once

//#include <DNSServer.h>
#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#endif
#ifdef ESP32
#include <WiFi.h>
#include <WebServer.h>
#include "esp_system.h"
#include <HTTPClient.h>
#include <DNSServer.h>
#include <memory>
#endif
#include <dhcpserver/dhcpserver.h>

#include "leaf_ip_esp.h"


//@***************************** constants *******************************


// 
//
//@******************************* callbacks *********************************

void handle_dhcp_lease(u8_t ip[4]) 
{
  WARN("New DHCP client %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
}

// 
//
//@************************* class IpEspApLeaf ***************************
//
// This class encapsulates the wifi interface on esp8266 or esp32
//
#ifdef ESP8266
typedef ESP8266WebServer WebServer;
#endif

class IpEspApLeaf : public IpEspLeaf
{
protected:
  std::unique_ptr<DNSServer>        dnsServer;
  std::unique_ptr<WebServer>        webServer;

public:
  IpEspApLeaf(String name, String target="", bool run=true, String ap_name="", String ap_pass="", bool ap_mode=false)
    : IpEspLeaf(name, target, run, ap_name, ap_pass, ap_mode)
    , Debuggable(name)
  {
    LEAF_ENTER(L_INFO);
    LEAF_LEAVE;
  }

  int apClientCount() 
  {
    int c = 0;
    for (int i=0; i<ap_client_max; i++) {
      if (ap_clients[i].valid) ++c;
    }
    return c;
  }

  ap_client_t * getApClientRecord(int n) 
  {
    int c = 0;
    for (int i=0; i<ap_client_max; i++) {
      if (!ap_clients[i].valid) continue;
      if (c==n) {
	return &ap_clients[i];
      }
      ++c;
    }
    return NULL;
  }

  WebServer *getWebServer(){return webServer.get();}

  virtual void setup()
  {
    IpEspLeaf::setup();
    LEAF_ENTER(L_NOTICE);

    LEAVE;
  }

  virtual void start()
  {
    LEAF_ENTER(L_INFO);

    LEAF_NOTICE("Saved AP client credentials:");
    for (int i=0; i<wifi_multi_max; i++) {
      if (wifi_multi_ssid[i] == "") continue;
      LEAF_NOTICE("  wifi_multi_ssid[%d] = [%s]", i, wifi_multi_ssid[i]);
    }

    LEAF_NOTICE("Create DNS and HTTP servers");
    dnsServer.reset(new DNSServer());
    webServer.reset(new WebServer(80));

    WiFi.persistent(false); // clear settings
    WiFi.disconnect(true); // disconnect and power off, necessary to make setHostname work
    WiFi.setHostname(device_id);

    WiFi.mode(WIFI_OFF);
    WiFi.mode(WIFI_AP_STA);

    IpEspLeaf::start();
    
    LEAF_VOID_RETURN;
  }

  virtual void stop()
  {
    IpEspLeaf::stop();
  }

  virtual void loop()
  {
    IpEspLeaf::loop();
    dnsServer->processNextRequest();
    webServer->handleClient();
    if (!ip_wifi_known_state && wifi_multi_ssid_count) {
      this->tryIpConnect();
    }
    yield();
  }

  virtual bool tryIpConnect() { return IpEspLeaf::tryIpConnect(); }
  virtual bool tryIpConnect(String ap) 
  {
    for (int i=0; i<wifi_multi_max; i++) {
      if (wifi_multi_ssid[i] != ap) {
	continue;
      }
      LEAF_NOTICE("Will search for wifi AP %d: [%s] / [%s]",
		  i+1, wifi_multi_ssid[i].c_str(),
		  wifi_multi_pass[i].c_str() // MUL! TEE! PASS!
	);
      wifiMulti.addAP(wifi_multi_ssid[i].c_str(), wifi_multi_pass[i].c_str()); // MUL! TEE! PASS!
      wifi_multi_ssid_count ++;
    }
    return this->tryIpConnect();
  }
  
  virtual bool ipConnect(String reason="");
  virtual void ap_setup();
  virtual bool commandHandler(String type, String name, String topic, String payload);
};

bool IpEspApLeaf::ipConnect(String reason) 
{
  if (!AbstractIpLeaf::ipConnect(reason)) {
    // Superclass said no can do
    return(false);
  }
  LEAF_ENTER_STR(L_NOTICE, reason);
  // todo: scan and maybe call superclass ip_connect
  ap_setup();
  LEAF_BOOL_RETURN(true);
}

void IpEspApLeaf::ap_setup()
{
  LEAF_ENTER(L_NOTICE);

  dhcps_set_new_lease_cb(&handle_dhcp_lease);

  IPAddress ap_ip = IPAddress(192,168,  4,  1);
  IPAddress ap_gw = IPAddress(192,168,  4,  1);
  IPAddress ap_sn = IPAddress(255,255,255,  0);
  String ap_ip_str = ap_ip.toString();

  // operate as a wifi access point
  LEAF_WARN("Activating WiFi AP %s", ip_ap_name.c_str());
  if (!WiFi.softAPConfig(ap_ip, ap_gw, ap_sn)) {
    LEAF_ALERT("WiFi AP address configuration failed.");
    LEAF_VOID_RETURN;
  }
  if (!WiFi.softAP(ip_ap_name, ip_ap_pass)) {
    LEAF_ALERT("WiFi AP creation failed.");
    LEAF_VOID_RETURN;
  }
  dnsServer->start(53, "*", ap_ip);
  LEAF_NOTICE("DNS server started");

  publish("_wifi_ap", "1");
  webServer->begin();
  LEAF_NOTICE("HTTP server started");
  
  LEAF_LEAVE;
}

bool IpEspApLeaf::commandHandler(String type, String name, String topic, String payload)
{
  LEAF_HANDLER(L_INFO);

  WHEN("ip_wifi_try_ap", if (!isConnected()) tryIpConnect(payload))
  else handled = IpEspLeaf::commandHandler(type, name, topic, payload);
  LEAF_HANDLER_END;
}




// Local Variables:
// mode: C++
// c-basic-offset: 2
// End:
