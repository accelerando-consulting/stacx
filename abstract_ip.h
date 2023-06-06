#pragma once

#ifdef ESP32
#include "esp_system.h"
//#include <esp32/rom/md5_hash.h>
#include <Update.h>
#endif

#ifndef IP_LOG_CONNECT
#define IP_LOG_CONNECT false
#endif

#ifndef IP_LOG_FILE
#define IP_LOG_FILE STACX_LOG_FILE
#endif

#ifndef IP_ENABLE_OTA
#define IP_ENABLE_OTA false
#endif

#ifdef ESP8266
#define USE_IP_TCPCLIENT 0
#else
#define USE_IP_TCPCLIENT 1
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
  static const int CLIENT_SESSION_MAX=8;

  static const int TIME_SOURCE_NONE=0;
  static const int TIME_SOURCE_RTC=1;
  static const int TIME_SOURCE_GPS=2;
  static const int TIME_SOURCE_GSM=3;
  static const int TIME_SOURCE_NTP=4;
  static const int TIME_SOURCE_BROKER=5;

  AbstractIpLeaf(String name, String target, pinmask_t pins=NO_PINS) :
    Leaf("ip", name, pins),
    Debuggable(name)
  {
    this->tap_targets = target;
    do_heartbeat = false;
#if USE_IP_TCPCLIENT
    for (int i=0; i<CLIENT_SESSION_MAX; i++) {
      this->ip_clients[i] = NULL;
    }
#endif // USE_IP_TCPCLIENT
  }

  virtual void setup(void);
  virtual void loop(void);
  virtual void ipScheduleReconnect();

  virtual bool isPresent() { return true; }
  virtual bool isConnected(codepoint_t where=undisclosed_location) { return ip_connected; }
  virtual bool gpsConnected() { return false; }
  virtual bool isAutoConnect() { return ip_autoconnect; }
  virtual void setIpAddress(IPAddress address) { ip_addr_str = address.toString(); }
  virtual void setIpAddress(String address_str) { ip_addr_str = address_str; }
  virtual String ipAddressString() { return ip_addr_str; }
  virtual int getRssi() { return 0; }
  virtual int getConnectCount() { return ip_connect_count; }
  virtual int getConnectAttemptCount() { return ip_connect_attempt_count; }
  virtual bool isPrimaryComms() {
    if (isPriority("service")) return false;
    if (!ipLeaf) return true; // dunno, presume true
    return (ipLeaf==this);
  }

  virtual void ipConfig(bool reset=false) {}
  virtual bool ipPing(String host) {return false;}
  virtual String ipDnsQuery(String host, int timeout=1) {return "ENOTIMPL";}
  virtual void ipPullUpdate(String url) {}
  virtual void ipRollbackUpdate(String url) {}
  virtual bool ftpPut(String host, String user, String pass, String path, const char *buf, int buf_len) { return false; }
  virtual int ftpGet(String host, String user, String pass, String path, char *buf, int buf_max) { return -1; }
  virtual void ipCommsState(enum comms_state s, codepoint_t where=undisclosed_location)
  {
    comms_state(s, CODEPOINT(where), this);
  }

  virtual bool ipConnect(String reason="");
  virtual bool ipDisconnect(bool retry=false);
  virtual bool netStatus(){return false;};
  virtual bool connStatus(){return false;};

  virtual void ipOnConnect();
  virtual void ipOnDisconnect();
  virtual void ipStatus(String status_topic="ip_status");
  virtual void status_pub() { ipStatus(); }
  void ipSetReconnectDue() {ip_reconnect_due=true;}
  void ipSetNotify(bool n) { ip_do_notify = n; }
  AbstractIpLeaf *noNotify() { ip_do_notify = false; return this;}
  virtual void ipPublishTime(String fmt = "", String action="", bool mqtt_pub = true);

  virtual bool commandHandler(String type, String name, String topic, String payload);
  
#if USE_IP_TCPCLIENT
  virtual Client *tcpConnect(String host, int port, int *slot_r=NULL);
  virtual void tcpRelease(Client *client);
  // subclasses that implement stream connections must override this method (eg see abstract_ip_lte.h)
  virtual Client *newClient(int slot){return NULL;};
#endif // USE_IP_TCPCLIENT
  int getTimeSource() { return
      ip_time_source; }
  void setTimeSource(int s) 
  {
    LEAF_ENTER_INT(L_NOTICE, s);
    ip_time_source = s;
    ipPublishTime("", "TIME", false);
    LEAF_LEAVE;
  }

protected:
  String ip_ap_name="";
  String ip_ap_user="";
  String ip_ap_pass="";
  String ip_addr_str="unset";

  bool ip_connected = false;
  bool ip_do_notify = true;
  bool ip_connect_notified=false;
  bool ip_log_connect = IP_LOG_CONNECT;
  int ip_time_source = 0;
  int ip_reconnect_interval_sec = NETWORK_RECONNECT_SECONDS;
  int ip_connect_count = 0;
  int ip_connect_attempt_count = 0;
  int ip_connect_attempt_max = 0;
  bool ip_reuse_connection = true;
  Ticker ipReconnectTimer;
  unsigned long ip_connect_time = 0;
  unsigned long ip_disconnect_time = 0;
  bool ip_autoconnect = true;
  bool ip_reconnect = true;
  bool ip_reconnect_due = false;
  int  ip_delay_connect = 0;
  bool ip_enable_ssl = false;
  bool ip_enable_ota = IP_ENABLE_OTA;
  int ip_rssi=0;
#if USE_IP_TCPCLIENT
  Client *ip_clients[CLIENT_SESSION_MAX];
#endif
};

bool AbstractIpLeaf::ipConnect(String reason) {
  ACTION("IP try (%s)", getNameStr());
  ipCommsState(TRY_IP, HERE);
  ip_connect_attempt_count++;
  if (ip_log_connect && isPrimaryComms()) {
    char buf[80];
    snprintf(buf, sizeof(buf), "%s attempt %d uptime=%lu clock=%lu",
	     getNameStr(),
	     ip_connect_attempt_count,
	     (unsigned long)millis(),
	     (unsigned long)time(NULL));
    LEAF_NOTICE("%s", buf);
    message("fs", "cmd/appendl/" IP_LOG_FILE, buf);
  }
  return true;
}

bool AbstractIpLeaf::ipDisconnect(bool retry) {
  LEAF_ENTER_BOOL(L_NOTICE, retry);
    if (retry) {
      ipScheduleReconnect();
    } else {
      ipCommsState(OFFLINE, HERE);
      ipReconnectTimer.detach();
    }
    LEAF_BOOL_RETURN(true);
};

void AbstractIpLeaf::ipOnConnect(){
  ipCommsState(WAIT_PUBSUB, HERE);
  ip_connected=true;
  ip_connect_time=millis();
  ++ip_connect_count;
  if (ip_log_connect) {
    char buf[80];
    snprintf(buf, sizeof(buf), "%s connect %d attempts=%d uptime=%lu clock=%lu",
	     getNameStr(),
	     ip_connect_count,
	     ip_connect_attempt_count,
	     (unsigned long)millis(),
	     (unsigned long)time(NULL));
    LEAF_NOTICE("%s", buf);
    message("fs", "cmd/appendl/" IP_LOG_FILE, buf);
  }
  ip_connect_attempt_count=0;

  ACTION("IP conn (%s)", getNameStr());
}

void AbstractIpLeaf::ipOnDisconnect(){
  if (isPrimaryComms()) {
    if (ip_autoconnect) {
      ipCommsState(WAIT_IP, HERE);
    }
    else {
      ipCommsState(OFFLINE, HERE);
    }
  }
  ip_connected=false;
  ACTION("IP disc");
  ip_disconnect_time=millis();
  if (ip_log_connect) {
    char buf[80];
    int duration_sec = (ip_disconnect_time-ip_connect_time)/1000;
    snprintf(buf, sizeof(buf), "%s disconnect %d duration=%d uptime=%lu clock=%lu",
	     getNameStr(),
	     ip_connect_count,
	     duration_sec,
	     (unsigned long)millis(),
	     (unsigned long)time(NULL));
    LEAF_NOTICE("%s", buf);
    message("fs", "cmd/appendl/" IP_LOG_FILE, buf);
  }
}


#if USE_IP_TCPCLIENT
Client *AbstractIpLeaf::tcpConnect(String host, int port, int *slot_r) {
  LEAF_ENTER(L_NOTICE);

  int slot;
  for (slot = 0; slot < CLIENT_SESSION_MAX; slot++) {
    if (ip_clients[slot] == NULL) {
      break;
    }
  }
  if (slot >= CLIENT_SESSION_MAX) {
    LEAF_ALERT("No free slots for TCP connection");
    LEAF_RETURN(NULL);
  }
  Client *client = ip_clients[slot] = this->newClient(slot);
  LEAF_NOTICE("New TCP client at slot %d", slot);

  int conn_result = client->connect(host.c_str(), port);
  if (conn_result==1) {
    LEAF_NOTICE("TCP client %d connected", slot);
  }
  else if (conn_result==2) {
    LEAF_NOTICE("TCP client %d connection pending", slot);
  }
  else {
    LEAF_ALERT("TCP client %d connect failed", slot);
  }

  if (slot_r) *slot_r=slot;
  LEAF_RETURN(client);
}

void AbstractIpLeaf::tcpRelease(Client *client)
{
  LEAF_ENTER(L_NOTICE);
  for (int slot=0; slot<CLIENT_SESSION_MAX;slot++) {
    if (ip_clients[slot] == client) {
      ip_clients[slot] = NULL;
      break;
    }
  }
  delete client;

  LEAF_LEAVE;
}
#endif // USE_IP_TCPCLIENT

void AbstractIpLeaf::ipPublishTime(String fmt, String action, bool mqtt_pub)
{
    time_t now;
    struct tm localtm;
    char ctimbuf[80];
    if (fmt=="" || fmt=="1") {
      fmt="%FT%T";
    }
    time(&now);
    localtime_r(&now, &localtm);
    strftime(ctimbuf, sizeof(ctimbuf), fmt.c_str(), &localtm);
    if (mqtt_pub) {
      mqtt_publish("status/time", ctimbuf);
      mqtt_publish("status/unix_time", String(now));
    }
    else {
      publish("status/time", ctimbuf);
      publish("status/unix_time", String(now));
    }
    if (action != "") {
      ACTION("%s %s", action.c_str(), ctimbuf);
    }
    const char *time_source = "UNK";
    switch (ip_time_source) {
    case TIME_SOURCE_GPS:
      time_source = "GPS";
      break;
    case TIME_SOURCE_GSM:
      time_source = "GSM";
      break;
    case TIME_SOURCE_NTP:
      time_source = "NTP";
      break;
    case TIME_SOURCE_RTC:
      time_source = "RTC";
      break;
    }
    if (mqtt_pub) {
      mqtt_publish("status/time_source", time_source);
    }
    else {
      publish("status/time_source", time_source);
    }
}

void AbstractIpLeaf::setup()
{
    Leaf::setup();
    LEAF_ENTER(L_INFO);
    ipCommsState(OFFLINE, HERE);


    registerStrValue("ip_ap_name", &ip_ap_name, "IP Access point name");
    registerStrValue("ip_ap_user", &ip_ap_user, "IP Access point username");
    registerStrValue("ip_ap_pass", &ip_ap_pass, "IP Access point password");
    registerBoolValue("ip_autoconnect", &ip_autoconnect, "Automatically connect to IP at startup");
    registerIntValue("ip_delay_connect", &ip_delay_connect, "Delay connect to IP at startup (sec)");
    registerBoolValue("ip_reconnect", &ip_reconnect, "Automatically schedule a reconnect after loss of IP");
    registerBoolValue("ip_log_connect", &ip_log_connect, "Log connect/disconnect events to flash");
    registerIntValue("ip_reconnect_interval_sec", &ip_reconnect_interval_sec, "IP reconnect time in seconds (0=immediate)");
    registerIntValue("ip_connect_attempt_max", &ip_connect_attempt_max, "Maximum IP connect attempts (0=indefinite)");

    registerBoolValue("ip_reuse_connection", &ip_reuse_connection, "If IP is found already connected, re-use connection");
    registerIntValue("ip_connect_count", &ip_reuse_connection, "IP connection counter", ACL_GET_ONLY, VALUE_NO_SAVE);
    registerUlongValue("ip_connect_time", &ip_connect_time, "IP connection time", ACL_GET_ONLY, VALUE_NO_SAVE);
    registerUlongValue("ip_disconnect_time", &ip_connect_time, "IP disconnection time", ACL_GET_ONLY, VALUE_NO_SAVE);

    registerBoolValue("ip_enable_ota", &ip_enable_ota, "Support over-the-air firmware update");


    registerCommand(HERE,"ip_connect", "initiate connection to IP network");
    registerCommand(HERE,"ip_disconnect", "terminate connection to IP network");
    registerCommand(HERE,"ip_status", "publish the status of the IP connection");
    registerCommand(HERE,"ip_time", "publish the time and source");

    LEAF_LEAVE;
}

void AbstractIpLeaf::loop()
{
  LEAF_ENTER(L_DEBUG);
  Leaf::loop();

  unsigned long now_sec = millis()/1000;
  
  if (ip_reconnect_due && (now_sec >= ip_delay_connect)) {
    // A scheduled reconnect timer has expired.   Do the thing.  Maybe.
    ip_reconnect_due = false;
    if (!ip_connected) {
      // The modem may have auto reconnected in response to URC during the interim
      LEAF_NOTICE("Attempting scheduled %s connect", getNameStr());
      ipConnect("reconnect");
    }
  }

  if (ip_do_notify && (ip_connect_notified != ip_connected)) {
    if (ip_connected) {
      LEAF_INFO("Announcing IP connection, ip=%s", ip_addr_str.c_str());
      publish("_ip_connect", ip_addr_str, L_NOTICE, HERE);
    }
    else {
      LEAF_INFO("Announcing IP disconnection, ip=%s", ip_addr_str.c_str());
      publish("_ip_disconnect", "", L_INFO, HERE);
    }
    ip_connect_notified = ip_connected;
  }
  LEAF_LEAVE;
}

void ipReconnectTimerCallback(AbstractIpLeaf *leaf) { leaf->ipSetReconnectDue(); }

void AbstractIpLeaf::ipScheduleReconnect()
{
  LEAF_ENTER(L_NOTICE);
  if (!ip_reconnect) {
    LEAF_WARN("Auto reconnect is disabled (ip_reconnect OFF)");
  }
  else if (ip_reconnect_interval_sec < 0) {
    LEAF_WARN("Auto reconnect is disabled (interval < 0)");
  }
  else if (ip_connect_attempt_count >= ip_connect_attempt_max) {
    if (ip_log_connect) {
      char buf[80];
      snprintf(buf, sizeof(buf), "%s retry count (%d) exceeded",
	       getNameStr(),
	       ip_connect_attempt_count);
      LEAF_WARN("%s", buf);
      message("fs", "cmd/appendl/" IP_LOG_FILE, buf);
    }
    LEAF_VOID_RETURN;
  }
  else if (ip_reconnect_interval_sec == 0) {
    LEAF_NOTICE("Imediate reconnect attempt");
    ipSetReconnectDue();
  }
  else {
    LEAF_NOTICE("Will attempt reconnect in %ds", ip_reconnect_interval_sec);
    ipReconnectTimer.once(ip_reconnect_interval_sec,
			  &ipReconnectTimerCallback,
			  this);
    if (isPrimaryComms()) {
      ipCommsState(WAIT_IP, HERE);
    }
  }
  LEAF_LEAVE;
}

void AbstractIpLeaf::ipStatus(String status_topic)
{
  char status[64];
  uint32_t secs;
  if (ip_connected) {
    secs = (millis() - ip_connect_time)/1000;
    snprintf(status, sizeof(status), "%s online %d:%02d %s %ddB %s",
	     leaf_name.c_str(), secs/60, secs%60,
	     ip_ap_name.c_str(),
	     ip_rssi,
	     ip_addr_str.c_str());
  }
  else {
    secs = (millis() - ip_disconnect_time)/1000;
    snprintf(status, sizeof(status), "%s offline %d:%02d", leaf_name.c_str(), secs/60, secs%60);
  }
  LEAF_NOTICE("ipStatus %s", status);
  mqtt_publish(status_topic, status);
}

bool AbstractIpLeaf::commandHandler(String type, String name, String topic, String payload) {
  LEAF_HANDLER(L_INFO);

  WHEN("ip_connect",ipConnect("cmd"))
  ELSEWHEN("ip_disconnect",ipDisconnect())
  ELSEWHEN("cmd/ip_status",ipStatus())
  ELSEWHEN("ip_time", ipPublishTime(payload))
  else handled = Leaf::commandHandler(type, name, topic, payload);

  LEAF_HANDLER_END;
}



// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
