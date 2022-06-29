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
    do_heartbeat = false;
  }

  virtual void setup(void);
  virtual void loop(void);
  virtual void ipScheduleReconnect();

  virtual bool isPresent() { return true; }
  virtual bool isConnected() { return ip_connected; }
  virtual bool isAutoConnect() { return ip_autoconnect; }
     
  virtual int getRssi() { return 0; }

  virtual void ipConfig(void) {}
  virtual void pullUpdate(String url) {}
  virtual void rollbackUpdate(String url) {}
  virtual bool ftpPut(String host, String user, String pass, String path, const char *buf, int buf_len) { return false; }
  virtual int ftpGet(String host, String user, String pass, String path, char *buf, int buf_max) { return -1; }

  virtual bool ipConnect(String reason="")=0;
  virtual bool ipDisconnect(bool retry=false){if (retry) ipScheduleReconnect(); return true;};
  virtual bool netStatus(){return false;};
  virtual bool connStatus(){return false;};

  virtual void onConnect(){ip_connected=true; ip_connect_time=millis();};
  virtual void onDisconnect(){ip_connected=false; ip_connect_time=millis();};

  void ipSetReconnectDue() {ip_reconnect_due=true;};

  virtual bool mqtt_receive(String type, String name, String topic, String payload);

protected:
  String ip_ap_name="";
  String ip_ap_user="";
  String ip_ap_pass="";
  
  bool ip_connected = false;
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
    LEAF_ENTER(L_DEBUG);
    Leaf::setup();

    run = getBoolPref("ip_enable", run);
    ip_ap_name = getPref("ip_ap_name", ip_ap_name);
    ip_ap_user = getPref("ip_ap_user", ip_ap_user);
    ip_ap_pass= getPref("ip_ap_pass", ip_ap_pass);
    ip_autoconnect = getBoolPref("ip_autoconnect", ip_autoconnect);
    ip_reconnect = getBoolPref("ip_reconnect", ip_reconnect);
    
    LEAF_LEAVE;
}

void AbstractIpLeaf::loop() 
{
  Leaf::loop();

  if (ip_reconnect_due) {
    ip_reconnect_due = false;
    ipConnect("reconnect");
  }

  if (ip_connect_notified != ip_connected) {
    if (ip_connected) {
      publish("_ip_connect", String(ip_addr_str));
    }
    else {
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
  LEAF_ENTER(L_INFO);
  bool handled = Leaf::mqtt_receive(type, name, topic, payload);

  WHEN("cmd/ip_connect",{
      ipConnect("cmd");
    })
    ELSEWHEN("cmd/ip_disconnect",{
	ipDisconnect();
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

