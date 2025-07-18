#pragma once

#ifndef USE_OTA
#define USE_OTA 1
#endif

#ifdef ESP32
#include "esp_system.h"
//#include <esp32/rom/md5_hash.h>
#if USE_OTA
#include <Update.h>
#include "esp_app_format.h"
#include "esp_ota_ops.h"
#endif
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

#if USE_OTA
struct _ota_context {
  AbstractIpLeaf *leaf;
  bool noaction;
  size_t size;
  size_t done;
  int percent;
  int level;
  MD5Builder checksum;
} _ota_context;
#endif

#ifdef ESP32
#define USE_IP_TCPCLIENT 1
#else
#define USE_IP_TCPCLIENT 0
#endif

#ifndef IP_REPORT_INTERVAL_SEC
#define IP_REPORT_INTERVAL_SEC 0
#endif

#if defined(ESP32) || defined(ESP8266)
#define HAS_TICKER 1
#include <Ticker.h>
#else
#define HAS_TICKER 0
#endif

#ifndef IP_TCP_KEEPALIVE_ENABLE
#define IP_TCP_KEEPALIVE_ENABLE true
#endif
#ifndef IP_TCP_KEEPALIVE_IDLE_SEC
#define IP_TCP_KEEPALIVE_IDLE_SEC 30
#endif
#ifndef IP_TCP_KEEPALIVE_INTERVAL_SEC
#define IP_TCP_KEEPALIVE_INTERVAL_SEC 60
#endif
#ifndef IP_TCP_KEEPALIVE_COUNT
#define IP_TCP_KEEPALIVE_COUNT 1
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
    if (ip_log_connect) do_log=true;
#if USE_IP_TCPCLIENT
    for (int i=0; i<CLIENT_SESSION_MAX; i++) {
      this->ip_clients[i] = NULL;
    }
#endif // USE_IP_TCPCLIENT
  }

  virtual void setup(void);
  virtual void loop(void);
  virtual void ipScheduleReconnect();
  virtual void ipScheduleProbe(int delay=-1) {};
  virtual bool ipLinkStatus(bool force_correction=false) {
    return true;
  };

  virtual bool isPresent() { return true; }
  virtual bool isConnected(codepoint_t where=undisclosed_location) { return ip_connected; }
  virtual bool gpsConnected() { return false; }
  virtual bool isAutoConnect() { return ip_autoconnect; }
  virtual bool ipConnectLogEnabled() { return ip_log_connect; }
  virtual void setIpAddress(IPAddress address) { ip_addr_str = address.toString(); }
  virtual void setIpAddress(String address_str) { ip_addr_str = address_str; }
  virtual String ipAddressString() { return ip_addr_str; }
  virtual int getRssi() { return 0; }
  virtual String getApName() {return ip_ap_name;}
  virtual int getConnectCount() { return ip_connect_count; }
  virtual int getConnectAttemptCount() { return ip_connect_attempt_count; }
  virtual int getProbeCount() { return ip_probe_count; }
  virtual int getProbeLimit() { return ip_probe_limit; }
  virtual void incrementProbeCount() { ip_probe_count++; }
  virtual void resetProbeCount() { ip_probe_count=0; }
  virtual bool isPrimaryComms() {
    if (isPriority("service")) return false;
    if (!ipLeaf) return true; // dunno, presume true
    return (ipLeaf==this);
  }

  virtual void ipConfig(bool reset=false) {}
  virtual bool ipPing(String host) {return false;}
  virtual String ipDnsQuery(String host, int timeout=1) {return "ENOTIMPL";}
  virtual bool ipPullUpdate(String url, bool noaction=false) {return false;}
  virtual void ipRollbackUpdate(String url) {}
  virtual bool ftpPut(String host, String user, String pass, String path, const char *buf, int buf_len) { return false; }
  virtual int ftpGet(String host, String user, String pass, String path, char *buf, int buf_max) { return -1; }
  virtual void ipCommsState(enum comms_state s, codepoint_t where=undisclosed_location)
  {
    comms_state(s, CODEPOINT(where), this);
  }

  virtual bool ipConnect(String reason="");
  void post_error(enum post_error e, int count);

  virtual bool ipDisconnect(bool retry=false);
  virtual bool netStatus(){return false;};
  virtual bool connStatus(){return false;};

  virtual void ipOnConnect();
  virtual void ipOnDisconnect();
  virtual void ipStatus(String status_topic="ip_status");
#if USE_IP_TCPCLIENT
  virtual void ipClientStatus();
#endif
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
  const char *timeSourceName(int s);
  void setTimeSource(int s)
  {
    LEAF_ENTER_INT(L_NOTICE, s);
    LEAF_WARN("setTimeSource %d (%s)", s, timeSourceName(s));
    ip_time_source = s;
    ipPublishTime("", "", false);
    LEAF_LEAVE;
  }

  // sneaky internals
  void reportOtaState(const esp_partition_t *part=NULL) ;
    
protected:
  String ip_ap_name="";
  String ip_ap_user="";
  String ip_ap_pass="";
  String ip_addr_str="unset";
  bool ip_ap_mode = false;

  bool ip_connected = false;
  bool ip_do_notify = true;
  bool ip_connect_notified=false;
  bool ip_log_connect = IP_LOG_CONNECT;
  int ip_time_source = 0;
  int ip_reconnect_interval_sec = NETWORK_RECONNECT_SECONDS;
  int ip_connect_count = 0;
  int ip_connect_attempt_count = 0;
  int ip_connect_attempt_max = 0;
  int ip_probe_count = 0;
  int ip_probe_limit = 4;
  unsigned long ip_report_interval_sec = IP_REPORT_INTERVAL_SEC;
  unsigned long ip_report_last_sec = 0;

  bool ip_reuse_connection = true;
#if HAS_TICKER
  Ticker ipReconnectTimer;
#endif
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
public:
  bool ip_tcp_keepalive_enable = IP_TCP_KEEPALIVE_ENABLE;
  int ip_tcp_keepalive_idle = IP_TCP_KEEPALIVE_IDLE_SEC;
  int ip_tcp_keepalive_interval = IP_TCP_KEEPALIVE_INTERVAL_SEC;
  int ip_tcp_keepalive_count = IP_TCP_KEEPALIVE_COUNT;
#endif
};

bool AbstractIpLeaf::ipConnect(String reason) {
  ip_connect_attempt_count++;
  ipCommsState(TRY_IP, HERE);
  ACTION("IP try (%s) #%d/%d", getNameStr(), ip_connect_attempt_count, ip_connect_attempt_max);
  fslog(HERE, IP_LOG_FILE, "attempt %d max %d uptime_sec=%lu",
	ip_connect_attempt_count,
	ip_connect_attempt_max,
	(unsigned long)millis()/1000);
  return true;
}

void AbstractIpLeaf::post_error(enum post_error e, int count)
{
  if (ip_log_connect) {
    fslog(HERE, IP_LOG_FILE, "ERROR %s %d", post_error_names[e], count);
  }

  ::post_error(e, count);
}

bool AbstractIpLeaf::ipDisconnect(bool retry) {
  LEAF_ENTER_BOOL(L_NOTICE, retry);
    if (retry) {
      ipScheduleReconnect();
    } else {
      ipCommsState(OFFLINE, HERE);
#if HAS_TICKER
      ipReconnectTimer.detach();
#endif
    }
    LEAF_BOOL_RETURN(true);
};

void AbstractIpLeaf::ipOnConnect(){
  ipCommsState(WAIT_PUBSUB, HERE);
  ip_connected=true;
  ip_connect_time=millis();
  ++ip_connect_count;
  fslog(HERE, IP_LOG_FILE, "connect %d attempts=%d uptime_sec=%lu",
	ip_connect_count,
	ip_connect_attempt_count,
	(unsigned long)millis()/1000);
  ip_connect_attempt_count=0;

  ACTION("IP conn (%s)", getNameStr());
}

void AbstractIpLeaf::ipOnDisconnect(){
  if (isPrimaryComms()) {
    if (ip_autoconnect) {
      ipCommsState(WAIT_IP, HERE);
      ipScheduleReconnect();
    }
    else {
      ipCommsState(OFFLINE, HERE);
    }
  }
  ip_connected=false;
  ACTION("IP disc");
  ip_disconnect_time=millis();
  fslog(HERE, IP_LOG_FILE, "disconnect %d duration=%d uptime_sec=%lu",
	ip_connect_count,
	((ip_disconnect_time-ip_connect_time)/1000),
	(unsigned long)millis()/1000);
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
  if (client == NULL) {
    LEAF_ALERT("Client allocation failed");
    LEAF_RETURN(NULL);
  }
  LEAF_NOTICE("New TCP client at slot %d", slot);

  int conn_result = client->connect(host.c_str(), port);
  if (conn_result==1) {
    LEAF_WARN("TCP client %d connected", slot);
  }
  else if (conn_result==2) {
    LEAF_WARN("TCP client %d connection pending", slot);
  }
  else {
    LEAF_ALERT("TCP client %d connect failed", slot);
    tcpRelease(client);
    client = NULL;
  }

  if (slot_r) *slot_r=slot;
  LEAF_RETURN(client);
}

void AbstractIpLeaf::tcpRelease(Client *client)
{
  LEAF_ENTER_PTR(L_NOTICE, client);
  bool found = false;
  for (int slot=0; slot < CLIENT_SESSION_MAX ; slot++) {
    if (ip_clients[slot] == client) {
      LEAF_NOTICE("Release TCP slot %d", slot);
      ip_clients[slot] = NULL;
      found = true;
      break;
    }
    else if (ip_clients[slot]) {
      LEAF_NOTICE("Skip slot %d (Client %p, %s)", slot, ip_clients[slot], HEIGHT(ip_clients[slot]->connected()));
    }
  }
  if (!found) {
    LEAF_ALERT("Did not find connection slot matching client %p", client);
  }
  delete client;

  LEAF_LEAVE;
}
#endif // USE_IP_TCPCLIENT

const char *AbstractIpLeaf::timeSourceName(int s)
{
  static const char *time_source_names[]={
    "NONE",
    "RTC",
    "GPS",
    "GSM",
    "NTP",
    "PUB",
  };
  static const char *time_source_unk="UNK";

  if ((s >= 0) && (s <= TIME_SOURCE_BROKER)) {
    return time_source_names[s];
  }
  return time_source_unk;
}


void AbstractIpLeaf::ipPublishTime(String fmt, String action, bool mqtt_pub)
{
  LEAF_ENTER_STR(L_WARN, action);
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
  time_source = timeSourceName(ip_time_source);

  if (mqtt_pub) {
    mqtt_publish("status/time_source", time_source);
  }
  else {
    publish("status/time_source", time_source);
  }
  LEAF_LEAVE;
}

void AbstractIpLeaf::setup()
{
    Leaf::setup();
    LEAF_ENTER(L_INFO);
    ipCommsState(OFFLINE, HERE);


    registerStrValue("ip_ap_name", &ip_ap_name, "IP Access point name");
    registerStrValue("ip_ap_user", &ip_ap_user, "IP Access point username");
    registerStrValue("ip_ap_pass", &ip_ap_pass, "IP Access point password");
    registerBoolValue("ip_ap_mode", &ip_ap_mode, "IP access point mode");
    registerBoolValue("ip_autoconnect", &ip_autoconnect, "Automatically connect to IP at startup");
    registerIntValue("ip_delay_connect", &ip_delay_connect, "Delay connect to IP at startup (sec)");
    registerBoolValue("ip_reconnect", &ip_reconnect, "Automatically schedule a reconnect after loss of IP");
    registerBoolValue("ip_log_connect", &ip_log_connect, "Log connect/disconnect events to flash");
    registerIntValue("ip_reconnect_interval_sec", &ip_reconnect_interval_sec, "IP reconnect time in seconds (0=immediate)");
    registerIntValue("ip_connect_attempt_max", &ip_connect_attempt_max, "Maximum IP connect attempts (0=indefinite)");
    registerIntValue("ip_probe_limit", &ip_probe_limit, "Reinitialise IP hardware after this many failures (0=never)");


    registerBoolValue("ip_reuse_connection", &ip_reuse_connection, "If IP is found already connected, re-use connection");
    registerIntValue("ip_connect_count", &ip_reuse_connection, "IP connection counter", ACL_GET_ONLY, VALUE_NO_SAVE);
    registerUlongValue("ip_connect_time", &ip_connect_time, "IP connection time", ACL_GET_ONLY, VALUE_NO_SAVE);
    registerUlongValue("ip_disconnect_time", &ip_connect_time, "IP disconnection time", ACL_GET_ONLY, VALUE_NO_SAVE);
    registerUlongValue("ip_report_interval_sec", &ip_report_interval_sec, "Reporting interval for ip status (0=disable)");

    registerBoolValue("ip_enable_ota", &ip_enable_ota, "Support over-the-air firmware update");


    registerCommand(HERE,"ip_connect", "initiate connection to IP network");
    registerCommand(HERE,"ip_disconnect", "terminate connection to IP network");
    registerCommand(HERE,"ip_status", "publish the status of the IP connection");
    registerCommand(HERE,"ip_time", "publish the time and source");
#if USE_IP_TCPCLIENT
    registerCommand(HERE,"ip_client_status", "publish the status of IP clients");

    registerBoolValue("ip_tcp_keepalive_enable", &ip_tcp_keepalive_enable);
    registerIntValue("ip_tcp_keepalive_idle", &ip_tcp_keepalive_idle);
    registerIntValue("ip_tcp_keepalive_interval", &ip_tcp_keepalive_interval);
    registerIntValue("ip_tcp_keepalive_count", &ip_tcp_keepalive_count);
#endif

    if (ip_log_connect) do_log=true;

    LEAF_LEAVE;
}

void AbstractIpLeaf::loop()
{
  LEAF_ENTER(L_TRACE);
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
  if (ip_report_interval_sec &&
      (now_sec >= (ip_report_last_sec + ip_report_interval_sec))
    ) {
    ipStatus();
    ip_report_last_sec = now_sec;
  }


  LEAF_LEAVE;
}

void ipReconnectTimerCallback(AbstractIpLeaf *leaf) { leaf->ipSetReconnectDue(); }

void AbstractIpLeaf::ipScheduleReconnect()
{
  LEAF_ENTER(L_NOTICE);
  if (!ip_reconnect) {
    LEAF_NOTICE("Auto reconnect is disabled (ip_reconnect OFF)");
  }
  else if (ip_reconnect_interval_sec < 0) {
    LEAF_NOTICE("Auto reconnect is disabled (interval < 0)");
  }
  else if ((ip_connect_attempt_max>0) && (ip_connect_attempt_count >= ip_connect_attempt_max)) {
    // If this transport is configured to stop trying after a certain number of failures, stop here.
    fslog(HERE, IP_LOG_FILE, "retry count (%d) exceeded", ip_connect_attempt_count);
    ipCommsState(OFFLINE, HERE);
    LEAF_VOID_RETURN;
  }
  else if (ip_reconnect_interval_sec == 0) {
    LEAF_NOTICE("Imediate reconnect attempt");
    ipSetReconnectDue();
  }
  else {
#if HAS_TICKER
    LEAF_NOTICE("Will attempt reconnect in %ds", ip_reconnect_interval_sec);
    ipReconnectTimer.once(ip_reconnect_interval_sec,
			  &ipReconnectTimerCallback,
			  this);
#endif
    ipCommsState(WAIT_IP, HERE);
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
  fslog(HERE, IP_LOG_FILE, "%s", status);
  mqtt_publish(status_topic, status);

}

#if USE_IP_TCPCLIENT
void AbstractIpLeaf::ipClientStatus()
{
  char status[64];
  uint32_t secs;
  int count=0;

  for (int i=0; i<CLIENT_SESSION_MAX; i++) {
    if (ip_clients[i] == NULL) continue;
    ++count;
  }
  mqtt_publish(String("status/")+getName()+"_client_count", String(count));

  for (int i=0; i<CLIENT_SESSION_MAX; i++) {
    if (ip_clients[i] == NULL) continue;

    mqtt_publish(String("status/")+getName()+"_client/"+String(i),
		 (ip_clients[i]->connected()?"connected":"disconnected")
      );
  }
}
#endif

bool AbstractIpLeaf::commandHandler(String type, String name, String topic, String payload) {
  LEAF_HANDLER(L_INFO);

  WHEN("ip_connect",ipConnect("cmd"))
  ELSEWHEN("ip_disconnect",ipDisconnect())
  ELSEWHEN("ip_status",ipStatus())
#if USE_IP_TCPCLIENT
  ELSEWHEN("ip_client_status",ipClientStatus())
#endif
  ELSEWHEN("ip_time", ipPublishTime(payload))
  else {
    if (!handled) {
      handled = Leaf::commandHandler(type, name, topic, payload);
    }
  }

  LEAF_HANDLER_END;
}

#ifdef ESP32
void AbstractIpLeaf::reportOtaState(const esp_partition_t *part) 
{
  if (part == NULL) part = esp_ota_get_boot_partition();
  esp_ota_img_states_t ota_state;
  esp_ota_get_state_partition(part, &ota_state);
  LEAF_NOTICE("OTA state is %d", (int)ota_state);
  switch (ota_state) {
  case ESP_OTA_IMG_NEW:
    mqtt_publish("status/ota/state", "new");
    break;
  case ESP_OTA_IMG_UNDEFINED:
    mqtt_publish("status/ota/state", "undefined");
    break;
  case ESP_OTA_IMG_INVALID:
    mqtt_publish("status/ota/state", "invalid");
    break;
  case ESP_OTA_IMG_ABORTED:
    mqtt_publish("status/ota/state", "aborted");
    break;
  case ESP_OTA_IMG_VALID:
    mqtt_publish("status/ota/state", "valid");
    break;
  case ESP_OTA_IMG_PENDING_VERIFY:
    mqtt_publish("status/ota/state", "pending_verify");
    break;
  default:
    mqtt_publish("status/ota/state", "undocumented");
    break;
  }
}
#endif




// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
