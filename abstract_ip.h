#pragma once

#ifdef ESP32
#include "esp_system.h"
//#include <esp32/rom/md5_hash.h>
#include <Update.h>
#endif

//
//@************************** class AbstractIpLeaf ****************************
//
// This class encapsulates a TCP/IP interface such as the ESP32's built in
// wifi, or that provided by a cellular modem
//

class AbstractIpLeaf : public Leaf
{
public:

  AbstractIpLeaf(String name, String target, pinmask_t pins=NO_PINS) :
    Leaf("ip", name, pins)
  {
    this->tap_targets = target;
    this->ipLeaf = this;
    do_heartbeat = false;
  }

  virtual void setup(void);
  virtual void loop(void);
  virtual void ipScheduleReconnect();

  virtual bool isPresent() { return true; }
  virtual bool isConnected() { return ip_connected; }
  virtual bool gpsConnected() { return false; }
  virtual bool isAutoConnect() { return ip_autoconnect; }
  virtual void setIpAddress(IPAddress address) { ip_addr_str = address.toString(); }
  virtual void setIpAddress(String address_str) { ip_addr_str = address_str; }
  virtual String ipAddressString() { return ip_addr_str; }
  virtual int getRssi() { return 0; }

  virtual void ipConfig(bool reset=false) {}
  virtual bool ipPing(String host) {return false;}
  virtual void pullUpdate(String url) {}
  virtual void rollbackUpdate(String url) {}
  virtual bool ftpPut(String host, String user, String pass, String path, const char *buf, int buf_len) { LEAF_ENTER(L_ALERT);return false; }
  virtual int ftpGet(String host, String user, String pass, String path, char *buf, int buf_max) { return -1; }

  virtual bool ipConnect(String reason="")=0;
  virtual bool ipDisconnect(bool retry=false){if (retry) ipScheduleReconnect(); return true;};
  virtual bool netStatus(){return false;};
  virtual bool connStatus(){return false;};

  virtual void ipOnConnect(){
    idle_pattern(500,1, HERE);
    idle_color(PC_YELLOW);
    ip_connected=true;
    ip_connect_time=millis();
  }
  virtual void ipOnDisconnect(){
    idle_pattern(200,1, HERE);
    idle_color(PC_RED);
    ip_connected=false;
    ip_disconnect_time=millis();
  }

  void ipSetReconnectDue() {ip_reconnect_due=true;}
  void ipSetNotify(bool n) { ip_do_notify = n; }
  AbstractIpLeaf *noNotify() { ip_do_notify = false; return this;}

  virtual bool mqtt_receive(String type, String name, String topic, String payload);
    

protected:
  String ip_ap_name="";
  String ip_ap_user="";
  String ip_ap_pass="";
  String ip_addr_str="unset";
  
  bool ip_connected = false;
  bool ip_do_notify = true;
  bool ip_connect_notified=false;
  int ip_reconnect_interval_sec = NETWORK_RECONNECT_SECONDS;
  Ticker ipReconnectTimer;
  unsigned long ip_connect_time = 0;
  unsigned long ip_disconnect_time = 0;
  bool ip_autoconnect = true;
  bool ip_reconnect = true;
  bool ip_reconnect_due = false;
  bool ip_enable_ssl = false;
  int ip_rssi=0;
};

void AbstractIpLeaf::setup() 
{
    Leaf::setup();
    LEAF_ENTER(L_NOTICE);

    run = getBoolPref("ip_enable", run, "Enable IP connection");
    getPref("ip_ap_name", ip_ap_name, "IP Access point name");
    ip_ap_user = getPref("ip_ap_user", ip_ap_user, "IP Access point username");
    ip_ap_pass = getPref("ip_ap_pass", ip_ap_pass, "IP Access point password");
    ip_autoconnect = getBoolPref("ip_autoconnect", ip_autoconnect, "Automatically connect to IP at startup");
    ip_reconnect = getBoolPref("ip_reconnect", ip_reconnect, "Automatically schedule a reconecct after loss of IP");
    
    LEAF_LEAVE;
}

void AbstractIpLeaf::loop() 
{
  Leaf::loop();

  if (ip_reconnect_due) {
    ip_reconnect_due = false;
    LEAF_NOTICE("Attempting scheduled IP reconnect");
    ipConnect("reconnect");
  }

  if (ip_do_notify && (ip_connect_notified != ip_connected)) {
    if (ip_connected) {
      LEAF_NOTICE("Announcing IP connection, ip=%s", ip_addr_str.c_str());
      publish("_ip_connect", ip_addr_str);
    }
    else {
      LEAF_NOTICE("Announcing IP disconnection, ip=%s", ip_addr_str.c_str());
      publish("_ip_disconnect", "");
    }
    ip_connect_notified = ip_connected;
  }
}

void ipReconnectTimerCallback(AbstractIpLeaf *leaf) { leaf->ipSetReconnectDue(); }

void AbstractIpLeaf::ipScheduleReconnect() 
{
  if (ip_reconnect_interval_sec == 0) {
    ipSetReconnectDue();
  }
  else {
    ipReconnectTimer.once(ip_reconnect_interval_sec,
			  &ipReconnectTimerCallback,
			  this);
  }
}

bool AbstractIpLeaf::mqtt_receive(String type, String name, String topic, String payload)
{
  LEAF_ENTER(L_DEBUG);
  bool handled = Leaf::mqtt_receive(type, name, topic, payload);

  WHEN("cmd/ip_connect",{
      ipConnect("cmd");
    })
    ELSEWHEN("cmd/ip_disconnect",{
	ipDisconnect();
      })
    ELSEWHEN("set/ip_ap_name",{
	ip_ap_name = payload;
	setPref("ip_ap_name", ip_ap_name);
      })
    ELSEWHEN("get/ip_ap_name",{
	mqtt_publish("status/ip_ap_name", ip_ap_name);
      })
    ELSEWHEN("cmd/ip_status",{
	char status[32];
	uint32_t secs;
	if (ip_connected) {
	  secs = (millis() - ip_connect_time)/1000;
	  snprintf(status, sizeof(status), "online %d:%02d", secs/60, secs%60);
	}
	else {
	  secs = (millis() - ip_disconnect_time)/1000;
	  snprintf(status, sizeof(status), "offline %d:%02d", secs/60, secs%60);
	}
	mqtt_publish("status/ip_status", status);
      });

  return handled;
}


// local Variables:
// mode: C++
// c-basic-offset: 2
// End:

