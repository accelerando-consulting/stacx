#pragma once
#include "abstract_ip_modem.h"

#include "ip_client_lte.h"

//
//@************************* class AbstractIpLTELeaf ***************************
//
// This class encapsulates a TCP/IP interface via an AT-command modem
//

class AbstractIpLTELeaf : public AbstractIpModemLeaf
{
public:
  static const int TIME_SOURCE_NONE=0;
  static const int TIME_SOURCE_GPS=1;
  static const int TIME_SOURCE_NETWORK=2;
  
  AbstractIpLTELeaf(String name, String target, int uart, int rxpin, int txpin, int baud=115200, uint32_t options=SERIAL_8N1, int8_t pwrpin=MODEM_PWR_PIN_NONE, int8_t keypin=MODEM_KEY_PIN_NONE, int8_t sleeppin=MODEM_SLP_PIN_NONE, bool run = LEAF_RUN, bool autoprobe=true)
    : AbstractIpModemLeaf(name,target,uart,rxpin,txpin,baud,options,pwrpin,keypin,sleeppin,run,autoprobe)
    , TraitDebuggable(name)
  {
    ip_ap_name = "telstra.m2m";
  }
  
  virtual void setup(void);
  virtual void start(void);
  virtual void loop(void);
  virtual void mqtt_do_subscribe();
  virtual bool mqtt_receive(String type, String name, String topic, String payload);
  virtual void onModemPresent(void);

  virtual int getRssi();
  virtual bool getNetStatus();
  virtual bool gpsConnected() { return gps_fix; }

  virtual int getSMSCount();
  virtual String getSMSText(int msg_index);
  virtual String getSMSSender(int msg_index);
  virtual bool cmdSendSMS(String rcpt, String msg);
  virtual bool cmdDeleteSMS(int msg_index);

  virtual bool ipConnect(String reason);
  virtual bool ipDisconnect(bool retry=false);
  virtual bool ipModemConfigure(){return true;};
  virtual bool ipLinkUp() { return modemSendCmd(HERE, "AT+CNACT=1"); }
  virtual bool ipLinkDown() { return modemSendCmd(HERE, "AT+CNACT=0"); }
  virtual bool ipLinkStatus();
  virtual Client *newClient(int slot);

protected:
  virtual void ipOnConnect();
  bool ipProcessSMS(int index=-1);
  bool ipProcessNetworkTime(String Time);
  bool ipCheckTime();
  bool ipCheckGPS();
  bool ipEnableGPS();
  bool ipGPSPowerStatus();
  bool ipDisableGPS(bool resumeIp=false);
  bool ipPollGPS(bool force=false);
  bool ipPollNetworkTime();
  virtual void ipPublishTime(String fmt = "");
  virtual bool parseNetworkTime(String datestr);
  virtual bool parseGPS(String gps);
  virtual bool modemProcessURC(String Message);
  
  bool ip_simultaneous_gps = true;
  bool ip_enable_gps = true;
  bool ip_enable_gps_always = false;
  bool ip_enable_gps_only = false; // using modem for GPS only, not for comms
  bool ip_initial_gps = false;
  bool ip_enable_rtc = true;
  bool ip_enable_sms = true;
  bool ip_clock_dst = false;
  String ip_lte_sms_password = "";
  bool ip_modem_probe_at_sms = false;
  bool ip_modem_probe_at_gps = false;
  bool ip_modem_publish_gps_raw = false;
  bool ip_modem_publish_location_always = false;
  unsigned long ip_modem_gps_timestamp = 0;  // a time_t in seconds
  unsigned long ip_modem_gps_fix_check_interval = 2500;
  int ip_modem_gps_fix_timeout_sec = 300;
  bool ip_modem_gps_autosave = false;
  int ip_time_source = 0;
  int ip_location_refresh_interval = 600;
  time_t ip_location_timestamp = 0;
  bool ip_gps_active = false;
  unsigned long ip_gps_active_timestamp = 0;
  unsigned long ip_gps_acquire_time = 0;
  int ip_ftp_timeout_sec = 30;
  
  bool ip_abort_no_service = false;
  bool ip_abort_no_signal = true;

  String ip_device_type="";
  String ip_device_imei="";
  String ip_device_iccid="";
  String ip_device_version="";

  int8_t battery_percent;
  float latitude=NAN, longitude=NAN, speed_kph=NAN, heading=NAN, altitude=NAN, second=NAN;
  uint16_t year;
  uint8_t month, day, hour, minute;
  unsigned long gps_check_interval = 600*1000;
  unsigned long sms_check_interval = 300*1000;
  unsigned long last_gps_check = 0;
  unsigned long last_sms_check = 0;
  unsigned long last_gps_fix_check = 0;
  bool gps_fix = false;

};

void AbstractIpLTELeaf::setup(void) {
    AbstractIpModemLeaf::setup();
    LEAF_ENTER(L_NOTICE);

    getBoolPref("ip_lte_autoconnect", &ip_autoconnect); // unlisted preference

    getBoolPref("ip_abort_no_service", &ip_abort_no_service, "Check cellular service before connecting");
    getBoolPref("ip_abort_no_signal", &ip_abort_no_signal, "Check cellular signal strength before connecting");
    getBoolPref("ip_enable_ssl", &ip_enable_ssl, "Use SSL for TCP connections");
    getBoolPref("ip_enable_gps", &ip_enable_gps, "Enable GPS receiver");
    if (ip_simultaneous_gps) {
      getBoolPref("ip_enable_gps_always", &ip_enable_gps_always, "Continually track GPS location");
    }
    else {
      getBoolPref("ip_initial_gps", &ip_initial_gps, "Delay IP connection until GPS location obtained");
    }
    getBoolPref("ip_enable_gps_only", &ip_enable_gps_only, "Use LTE modem for location only, not for comms");

    getBoolPref("ip_enable_rtc", &ip_enable_rtc, "Use clock in modem");
    getBoolPref("ip_enable_sms", &ip_enable_sms, "Process SMS via modem");
    getBoolPref("ip_modem_publish_gps_raw", &ip_modem_publish_gps_raw, "Publish the raw GPS readings");
    getBoolPref("ip_modem_publish_location_always", &ip_modem_publish_location_always, "Publish the location status when unchanged");
    getULongPref("ip_modem_gps_timestamp", &ip_modem_gps_timestamp, "Timestamp of most recent change to GPS position");
    getIntPref("ip_modem_gps_fix_timeout_sec", &ip_modem_gps_fix_timeout_sec, "Time in seconds to wait for GPS lock before giving up (0=forever)");
    getBoolPref("ip_modem_gps_autosave", &ip_modem_gps_autosave, "Save last GPS position to flash memory");
    ip_location_timestamp = getULongPref("ip_location_timestamp", (unsigned long)0, "Time of last GPS fix");

    getIntPref("ip_location_refresh_interval_sec", &ip_location_refresh_interval, "Periodically check location");
    getIntPref("ip_ftp_timeout_sec", &ip_ftp_timeout_sec, "Timeout (in seconds) for FTP operations");

    // if set ip_lte_ap* overrids ip_ap_* for LTE modem
    ip_ap_name = getPref("ip_lte_ap_name", ip_ap_name, "LTE Access point name");
    ip_ap_user = getPref("ip_lte_ap_user", ip_ap_user, "LTE Access point username");
    ip_ap_pass = getPref("ip_lte_ap_pass", ip_ap_pass, "LTE Access point password");
    ip_lte_sms_password = getPref("ip_lte_sms_password", ip_lte_sms_password, "LTE SMS password");

    getBoolPref("ip_lte_key_invert", &invert_key, "Invert the sense of the LTE soft-power pin");
    LEAF_WARN("LTE key invert state is %s", ABILITY(invert_key));

    getFloatPref("ip_modem_latitude", &latitude, "Recorded position latitude");
    getFloatPref("ip_modem_longitude", &longitude, "Recorded position latitude");
    getFloatPref("ip_modem_altitude", &altitude, "Recorded position altitude");

    

    LEAF_LEAVE;
  }

void AbstractIpLTELeaf::start(void) 
{
  AbstractIpModemLeaf::start();
  LEAF_ENTER(L_INFO);

  

  
  LEAF_VOID_RETURN;
}

void AbstractIpLTELeaf::onModemPresent() 
{
  AbstractIpModemLeaf::onModemPresent();
  LEAF_ENTER(L_INFO);

  // Modem was detected for the first time after poweroff or reboot
  if (modemIsPresent()) {
    ip_device_type = modemQuery("AT+CGMM","");
    ip_device_imei = modemQuery("AT+CGSN","");
    ip_device_iccid = modemQuery("AT+CCID","");
    if (ip_device_iccid == "") {
      LEAF_ALERT("SIM card ID not read");
      post_error(POST_ERROR_LTE_NOSIM, 0);
    }
    ip_device_version = modemQuery("AT+CGMR","Revision:");
    publish("_ip_modem", "1");
  }
  if (!ip_enable_gps && ipGPSPowerStatus()) {
    LEAF_NOTICE("Make sure GPS is off");
    ipDisableGPS();
  }
}


void AbstractIpLTELeaf::loop(void) 
{
  AbstractIpModemLeaf::loop();
  LEAF_ENTER(L_TRACE);
  
  if (!canRun() || !modemIsPresent()) {
    LEAF_NOTICE("Modem not ready");
    LEAF_VOID_RETURN;
  }

  // Check if it is time to (re-)enable GPS and look for a fix
  if (ip_enable_gps && gpsConnected() && !ip_gps_active) {
    ipCheckGPS();
  }

  unsigned long now = millis();
  
  if (ip_gps_active &&
      gps_fix &&
      (now >= (last_gps_check + gps_check_interval)) ) {
    // If GPS is already enabled, and has a fix, check for a change of location infrequently
    last_gps_check = now;
    ipPollGPS();
  }
  else if (ip_gps_active &&
	   !gps_fix &&
	   (now >= (last_gps_fix_check + ip_modem_gps_fix_check_interval)) ){
    // GPS is enabled but has no fix, check for a fix frequently (so that if
    // simultaneous GPS is not supported, we can resume IP)
    last_gps_fix_check = now;
    ipPollGPS();
    unsigned long elapsed_sec = (now - ip_gps_active_timestamp)/1000;
    if (!gps_fix && ip_modem_gps_fix_timeout_sec && (elapsed_sec >= ip_modem_gps_fix_timeout_sec)) {
      // Time to give up on GPS
      LEAF_WARN("GPS fix timeout (after %d sec)", elapsed_sec);
      ipDisableGPS(true);
    }
    else {
      LEAF_NOTICE("No GPS fix yet (after %d sec)", elapsed_sec);
    }
  }
  
  if (ip_enable_sms && (millis() >= (last_sms_check+sms_check_interval))) {
    last_sms_check=millis();
    ipProcessSMS();
  }
  LEAF_LEAVE;
}

bool AbstractIpLTELeaf::getNetStatus() {
  String numeric_status = modemQuery("AT+CGREG?", "+CGREG: ");
  if (!numeric_status) {
    LEAF_ALERT("Status query failed");
    return false;
  }
  int status = numeric_status.toInt();

  switch (status) {
  case 0:
    LEAF_NOTICE("getNetStatus: not registered");
    break;
  case 1:
    LEAF_NOTICE("getNetStatus: Registered (home)");
    break;
  case 2:
    LEAF_NOTICE("getNetStatus: Not registered (searching)");
    break;
  case 3:
    LEAF_NOTICE("getNetStatus: Denied");
    break;
  case 4:
    LEAF_NOTICE("getNetStatus: Unknown");
    break;
  case 5:
    LEAF_NOTICE("getNetStatus: Registered (roaming)");
    break;
  }
  
  return ((status == 1) || (status == 5));
}

bool AbstractIpLTELeaf::ipLinkStatus() {
  String status = modemQuery("AT+CNACT?", "+CNACT: ",10*modem_timeout_default, HERE);
  LEAF_NOTICE("Connection status %s", status.c_str());
  return (status.toInt()==1);
}

int AbstractIpLTELeaf::getRssi(void)
{
  LEAF_DEBUG("Check signal strength");
  int rssi = -99;
  
  if (modemSendExpectInt("AT+CSQ","+CSQ: ", &rssi, -1, HERE)) {
    rssi = 0 - rssi;
    LEAF_INFO("Got RSSI %d", rssi);
  }
  else {
    LEAF_ALERT("Modem CSQ (rssi) query failed");
  }
    
  return rssi;
}

void AbstractIpLTELeaf::ipOnConnect() 
{
  AbstractIpLeaf::ipOnConnect();
  LEAF_ENTER(L_INFO);
  
  time_t now;
  time(&now);

  if (now < 100000000) {
    // looks like the clock is in the "seconds since boot" epoch, not
    // "seconds since 1970".   Ask the network for an update
    LEAF_NOTICE("Clock appears stale, setting time from cellular network");
    ipPollNetworkTime();
  }
  LEAF_VOID_RETURN;
}

void AbstractIpLTELeaf::mqtt_do_subscribe() 
{
  AbstractIpModemLeaf::mqtt_do_subscribe();
  register_mqtt_cmd("ip_lte_status", "report the status of the LTE connection");
  register_mqtt_cmd("ip_lte_connect", "initiate an LTE connection");
  register_mqtt_cmd("ip_lte_disconnect", "terminate the LTE connection");
  register_mqtt_cmd("gps_store", "Store the current gps location to non-volatile memory");
  register_mqtt_cmd("gps_poll", "Poll the latest GPS status");
  register_mqtt_cmd("gps_enable", "Turn on the GPS receiver");
  register_mqtt_cmd("gps_disable", "Turn off the GPS receiver");
  register_mqtt_cmd("lte_time", "Set the clock from the LTE network");
  register_mqtt_cmd("ip_ping", "Send an ICMP echo (PING) packet train, for link testing");
  register_mqtt_cmd("ip_dns", "Peform a DNS lookup, for IP testing");
  register_mqtt_cmd("ip_tcp_connect", "Establish a TCP connection");
  register_mqtt_cmd("sms", "Send an SMS message (payload is number,msg)");
  register_mqtt_cmd("gps/config", "Report on configuration an status of gps");

}

bool AbstractIpLTELeaf::mqtt_receive(String type, String name, String topic, String payload)
{
  bool handled = AbstractIpModemLeaf::mqtt_receive(type, name, topic, payload);
  LEAF_INFO("AbstractIpLTELeaf mqtt_receive %s %s => %s", type.c_str(), name.c_str(), topic.c_str());

  WHEN("cmd/ip_lte_status",{
      ipStatus("ip_lte_status");
    })
  ELSEWHEN("cmd/ip_lte_connect",{
      ipConnect("cmd");
    })
  ELSEWHEN("cmd/ip_lte_configure",{
      ipModemConfigure();
    })
  ELSEWHEN("cmd/ip_lte_disconnect",{
      ipDisconnect();
    })
  ELSEWHEN("get/lte_time",{
    ipPublishTime(payload);
  })
  ELSEWHEN("cmd/gps_store",{
      setFloatPref("ip_modem_latitude", latitude);
      setFloatPref("ip_modem_longitude", longitude);
      setFloatPref("ip_modem_altitude", altitude);
  })
  ELSEWHEN("cmd/gps_enable",{
      ipEnableGPS();
    })
  ELSEWHEN("cmd/gps_disable",{
      ipDisableGPS(payload.toInt());
    })
  ELSEWHEN("cmd/gps_poll",{
      ipPollGPS(true);
    })
  ELSEWHEN("cmd/gps_config",{
      DynamicJsonDocument doc(512);
      JsonObject obj = doc.to<JsonObject>();
      obj["ip_simultaneous_gps"]=ip_simultaneous_gps;
      obj["ip_enable_gps"]=ip_enable_gps;
      obj["ip_enable_gps_always"]=ip_enable_gps_always;
      obj["ip_enable_gps_only"]=ip_enable_gps_only;
      obj["ip_initial_gps"]=ip_initial_gps;
      obj["ip_modem_probe_at_gps"]=ip_modem_probe_at_gps;
      obj["ip_modem_publish_gps_raw"]=ip_modem_publish_gps_raw;
      obj["ip_modem_publish_location_always"]=ip_modem_publish_location_always;
      obj["ip_modem_gps_autosave"]=ip_modem_gps_autosave;
      obj["ip_location_refresh_interval"]=ip_location_refresh_interval;
      char msg[512];
      serializeJson(doc, msg, sizeof(msg)-2);
      mqtt_publish(String("gps_config/")+leaf_name, msg, 0, false, L_NOTICE, HERE);
    })
  ELSEWHEN("cmd/gps_status",{
      DynamicJsonDocument doc(512);
      JsonObject obj = doc.to<JsonObject>();
      obj["ip_enable_gps"]=ip_enable_gps;
      obj["ip_gps_active"]=ip_gps_active;
      obj["gps_fix"]=gps_fix;
      obj["ip_gps_active_timestamp"]=ip_gps_active_timestamp;
      obj["ip_modem_gps_timestamp"]=ip_modem_gps_timestamp;
      obj["ip_gps_acquire_time"]=ip_gps_acquire_time;
      char msg[512];
      serializeJson(doc, msg, sizeof(msg)-2);
      mqtt_publish(String("gps_status/")+leaf_name, msg, 0, false, L_NOTICE, HERE);
    })
  ELSEWHEN("get/gps",{
      ipPollGPS(true);
    })
  ELSEWHEN("set/latitude",{
      latitude = payload.toFloat();
    })
  ELSEWHEN("set/longitude",{
      longitude = payload.toFloat();
    })
  ELSEWHEN("set/altitude",{
      altitude = payload.toFloat();
    })
  ELSEWHEN("cmd/lte_time",{
      ipPollNetworkTime();
    })
  ELSEWHEN("cmd/ip_ping",{
      if (payload == "") payload="8.8.8.8";
      ipPing(payload);
    })
  ELSEWHEN("cmd/ip_dns",{
      if (payload == "") payload="www.google.com";
      ipDnsQuery(payload,30000);
    })
  ELSEWHEN("cmd/ip_tcp_connect",{
      int space = payload.indexOf(' ');
      if (space < 0) {
	LEAF_ALERT("connect syntax is hostname SPACE portno");
      }
      else {
	String host = payload.substring(0, space);
	int port = payload.substring(space+1).toInt();
	int slot;
	IpClientLTE *client = (IpClientLTE *)tcpConnect(host, port, &slot);
	if (!client) {
	  LEAF_ALERT("Connect failed");
	}
	else {
	  LEAF_NOTICE("TCP connection in slot %d", slot);
	  publish("_tcp_connect", String(slot), L_NOTICE, HERE);
	}
      }
    })
  ELSEWHEN("set/ip_lte_ap_name",{
      ip_ap_name = payload;
      setPref("ip_lte_ap_name", ip_ap_name);
    })
  ELSEWHEN("get/ip_lte_ap_name",{
      mqtt_publish("status/ip_lte_ap_name", ip_ap_name);
    })
  ELSEWHEN("get/ip_lte_modem_info",{
      if (ip_device_type.length()) {
	mqtt_publish("status/ip_device_type", ip_device_type);
      }
      if (ip_device_imei.length()) {
	mqtt_publish("status/ip_device_imei", ip_device_imei);
      }
      if (ip_device_iccid.length()) {
	mqtt_publish("status/ip_device_iccid", ip_device_iccid);
      }
      if (ip_device_version.length()) {
	mqtt_publish("status/ip_device_version", ip_device_version);
      }
    })
  ELSEWHEN("get/ip_device_type",{
      if (ip_device_type.length()) {
	mqtt_publish("status/ip_device_type", ip_device_type);
      }
    })
  ELSEWHEN("get/ip_device_imei",{
      if (ip_device_imei.length()) {
	mqtt_publish("status/ip_device_imei", ip_device_imei);
      }
    })
  ELSEWHEN("get/ip_device_iccid",{
      if (ip_device_iccid.length()) {
	mqtt_publish("status/ip_device_iccid", ip_device_iccid);
      }
    })
  ELSEWHEN("get/ip_device_version",{
      if (ip_device_version.length()) {
	mqtt_publish("status/ip_device_version", ip_device_version);
      }
    })
  ELSEWHEN("get/ip_lte_signal",{
      //LEAF_INFO("Check signal strength");
      String rsp = modemQuery("AT+CSQ","+CSQ: ");
      mqtt_publish("status/ip_lte_signal", rsp);
    })
  ELSEWHEN("get/ip_lte_network",{
      //LEAF_INFO("Check network status");
      String rsp = modemQuery("AT+CPSI?","+CPSI: ");
      mqtt_publish("status/ip_lte_network", rsp);
    })
  ELSEWHEN("get/sms_count",{
      int count = getSMSCount();
      mqtt_publish("status/sms_count", String(count));
    })
  ELSEWHEN("get/sms",{
      LEAF_DEBUG("get/sms");
      int msg_index = payload.toInt();
      String msg = getSMSText(msg_index);
      if (msg) {
	String sender = getSMSSender(msg_index);
	char status_buf[256];
	if (sender) {
	  snprintf(status_buf, sizeof(status_buf), "{\"msgno\":%d, \"sender\",\"%s\", \"message\":\"%s\"}",
		   msg_index, sender.c_str(), msg.c_str());
	}
	else {
	  snprintf(status_buf, sizeof(status_buf), "{\"msgno\":%d \"sender\",\"unknown\", \"message\":\"%s\"}",
		   msg_index, msg.c_str());
	}
	mqtt_publish("status/sms", status_buf);
      }
      else {
	mqtt_publish("status/sms", "{\"error\":\"not found\"}");
      }
    })
  ELSEWHEN("cmd/sms",{
      LEAF_DEBUG("LTE cmd/sms");
      String number;
      String message;
      int pos = payload.indexOf(",");
      if (pos > 0) {
	number = payload.substring(0,pos);
	message = payload.substring(pos+1);
	LEAF_NOTICE("Send SMS number=%s msg=%s", number.c_str(), message.c_str());
	cmdSendSMS(number, message);
      }
    });
  
  return handled;
}
  

void AbstractIpLTELeaf::ipPublishTime(String fmt)
{
  AbstractIpLeaf::ipPublishTime(fmt);
  mqtt_publish("status/time_source", (ip_time_source==TIME_SOURCE_GPS)?"GPS":(ip_time_source==TIME_SOURCE_NETWORK)?"NETWORK":"SELF");
}

int AbstractIpLTELeaf::getSMSCount()
{
  LEAF_ENTER(L_NOTICE);
  if (!modemIsPresent()) {
    LEAF_NOTICE("Modem not present");
    LEAF_INT_RETURN(0);
  }
  
  
  if (!modemSendCmd(HERE, "AT+CMGF=1")) {
    // maybe the modem fell asleep
    if (modemProbe(HERE) && modemSendCmd(HERE, "AT+CMGF=1")) {
      LEAF_NOTICE("Successfully woke modem");
    }
    else {
      LEAF_ALERT("SMS format command not accepted");
      LEAF_INT_RETURN(0);
    }
  }
  String response = modemQuery("AT+CPMS?","+CPMS: ");
  int count = 0;
  int pos = response.indexOf(",");
  if (pos < 0) {
    LEAF_WARN("Did not understand response [%s]", response.c_str());
    LEAF_INT_RETURN(0);
  }
  //LEAF_NOTICE("Removing %d chars from [%s]", pos, response.c_str());
  response.remove(0, pos+1);
  count = response.toInt();

  LEAF_INT_RETURN(count);
}

String AbstractIpLTELeaf::getSMSText(int msg_index)
{
  LEAF_ENTER_INT(L_NOTICE, msg_index);
  String result = "";
  
  if (!modemIsPresent()) return "";

  int sms_len;
  snprintf(modem_command_buf, modem_command_max, "AT+CMGR=%d", msg_index);
  if (!modemSendExpectIntField(modem_command_buf, "+CMGR: ", 11, &sms_len, ',', -1, HERE)) {
    LEAF_ALERT("Error requesting message %d", msg_index);
    LEAF_STR_RETURN(result);
  }
  if (sms_len >= modem_response_max) {
    LEAF_ALERT("SMS message length (%d) too long", sms_len);
    LEAF_STR_RETURN(result);
  }
  if (!modemGetReplyOfSize(modem_response_buf, sms_len, modem_timeout_default*4)) {
    LEAF_ALERT("SMS message read failed");
    LEAF_STR_RETURN(result);
  }
  modem_response_buf[sms_len]='\0';
  LEAF_NOTICE("SMS message body: %d %s", msg_index, modem_response_buf);

  result = modem_response_buf;
  LEAF_STR_RETURN(result);
}

String AbstractIpLTELeaf::getSMSSender(int msg_index)
{
  if (!modemSendCmd(HERE, "AT+CMGF=1")) {
    LEAF_ALERT("SMS format command not accepted");
    return "";
  }
  if (!modemSendCmd(HERE, "AT+CSDH=1")) {
    LEAF_ALERT("SMS header format command not accepted");
    return "";
  }

  int sms_len;
  snprintf(modem_command_buf, modem_command_max, "AT+CMGR=%d", msg_index);
  String result = modemSendExpectQuotedField(modem_command_buf, "+CMGR: ", 2, ',', -1, HERE);
  modemFlushInput();
  return result;
}

bool AbstractIpLTELeaf::cmdSendSMS(String rcpt, String msg)
{
  if (!modemSendCmd(HERE, "AT+CMGF=1")) {
    LEAF_ALERT("SMS format command not accepted");
    return 0;
  }

  ACTION("SMS send");
  modemWaitBufferMutex();
  snprintf(modem_command_buf, modem_command_max, "AT+CMGS=\"%s\"", rcpt.c_str());
  if (!modemSendExpectPrompt(modem_command_buf, -1, HERE)) {
    LEAF_ALERT("SMS prompt not found");
    modemReleaseBufferMutex();
    return false;
  }
  modem_stream->print(msg);
  modem_stream->print((char)0x1A);

  modemGetReply(modem_response_buf, modem_response_max, 30000, 1, 0, HERE);
  if (strstr(modem_response_buf, "+CMGS")==NULL) {
    LEAF_ALERT("SMS send not confirmed");
    modemReleaseBufferMutex();
    return false;
  }
  modemFlushInput();
  
  modemReleaseBufferMutex();
  return true;
}

bool AbstractIpLTELeaf::cmdDeleteSMS(int msg_index) 
{
  if (!modemSendCmd(HERE, "AT+CMGF=1")) {
    LEAF_ALERT("SMS format command not accepted");
    return 0;
  }
  return modemSendCmd(HERE, "AT+CMGD=%03d", msg_index);
}


bool AbstractIpLTELeaf::ipProcessSMS(int msg_index)
{
  LEAF_ENTER_INT((msg_index<0)?L_INFO:L_NOTICE, msg_index);
  int first,last;

  if (ip_modem_probe_at_sms || !modemIsPresent()) {
    LEAF_NOTICE("Probing for modem");
    modemProbe(HERE,MODEM_PROBE_QUICK);
  }
  
  if (!modemIsPresent()) {
    LEAF_ALERT("Modem is not present");
    LEAF_BOOL_RETURN(false);
  }
  if (!modemWaitPortMutex(HERE)) {
    LEAF_ALERT("Cannot obtain modem mutex");
  }
  
  if (msg_index < 0) {
    // process all unread sms
    first = 0;
    last = getSMSCount();
    LEAF_INFO("Modem holds %d SMS messages", last);
    if (last > 0) {
      ACTION("SMS rcvd %d", last);
    }
  }
  else {
    first=msg_index;
    last=msg_index+1;
  }
  modemReleasePortMutex(HERE);

  for (msg_index = first; msg_index < last; msg_index++) {

    if (!modemWaitPortMutex(HERE)) {
      LEAF_ALERT("Cannot obtain modem mutex");
    }

    String msg = getSMSText(msg_index);
    if (!msg) continue;
    String sender = getSMSSender(msg_index);
    LEAF_NOTICE("SMS message %d is from %s", msg_index, sender.c_str());

    // Delete the SMS *BEFORE* processing, else we might end up in a
    // reboot loop forever.   DAMHIKT.
    cmdDeleteSMS(msg_index);
    modemReleasePortMutex(HERE);

    String reply = "";
    String command = "";
    String password = "";
    String topic;
    String payload;
    int sep;

    if (ip_lte_sms_password.length()) {
      if ((sep = msg.indexOf("\r\n")) >= 0) {
	password = msg.substring(0,sep);
	msg.remove(0,sep+2);
      }
      else if ((sep = msg.indexOf(" ")) >= 0) {
	password = msg.substring(0,sep);
	msg.remove(0,sep+1);
      }
      if (password != ip_lte_sms_password) {
	LEAF_ALERT("Invalid SMS password");
	LEAF_BOOL_RETURN(false);
      }
    }

    // Process each line of the SMS by treating it as MQTT input
    do {
      if ((sep = msg.indexOf("\r\n")) >= 0) {
	command = msg.substring(0,sep);
	msg.remove(0,sep+2);
      }
      else {
	command = msg;
	msg = "";
      }
      LEAF_NOTICE("Processing one line of SMS as a Bogo-MQTT: %s", command.c_str());
      if (!command.length()) continue;

      if ((sep = command.indexOf(' ')) >= 0) {
	topic = command.substring(0,sep);
	payload = command.substring(sep+1);
      }
      else {
	topic = command;
	payload = "1";
      }

      if (!pubsubLeaf) {
	continue;
      }

      if (pubsubLeaf->hasPriority()) {
	topic = pubsubLeaf->getPriority() + "/" + topic;
      }

      pubsubLeaf->_mqtt_receive(topic, payload, PUBSUB_SHELL|PUBSUB_LOOPBACK);
      reply += pubsubLeaf->getLoopbackBuffer() + "\r\n";
    } while (msg.length());

    // We have now accumulated results in reply
    if (sender.length()) {
      const int sms_max = 140;
      if (reply.length() < 140) {
	LEAF_NOTICE("Send SMS reply %s <= %s", sender.c_str(), reply.c_str());
	cmdSendSMS(sender, reply);
      }
      else {
	LEAF_NOTICE("Send Chunked SMS reply %s <= %s", sender.c_str(), reply.c_str());
	String chunk;
	while (reply.length()) {
	  if (reply.length() > 140) {
	    chunk = reply.substring(0,140);
	    reply = reply.substring(140);
	  }
	  else {
	    chunk = reply;
	    reply = "";
	  }
	  LEAF_INFO("Send SMS chunk %s", chunk);
	  cmdSendSMS(sender, chunk);
	}
      }
    }
  }

  LEAF_BOOL_RETURN(true);
}

bool AbstractIpLTELeaf::modemProcessURC(String Message)
{
  LEAF_ENTER(L_INFO);
  LEAF_INFO("Asynchronous Modem input: [%s]", Message.c_str());

  if (Message == "+PDP: DEACT") {
    LEAF_ALERT("Lost LTE connection");
    post_error(POST_ERROR_LTE, 3);
    ERROR("Lost LTE");
    post_error(POST_ERROR_LTE_LOST, 0);
    ipOnDisconnect();
    ipScheduleReconnect();

  }
  else if ((Message == "RDY") ||
	   (Message == "+CPIN: READY") ||
           (Message == "+CFUN: 1") ||
	   (Message == "SMS Ready")) {
    LEAF_NOTICE("Modem appears to be rebooting (%s)", Message.c_str());
    if (isConnected()) {
      ipOnDisconnect();
      ipScheduleReconnect();
    }
    if (Message=="+CFUN: 1") {
      ipModemRecordReboot();
      if ((ip_modem_last_reboot_cmd + 10000) > millis()) {
	// a reboot is not unexpected because we recently requested it
	LEAF_NOTICE("Modem appears to be rebooting (%s)", Message.c_str());
      }
      else {
	// no intentional reboot recently, this must be unexpected
	LEAF_ALERT("Modem rebooted unexpectedly (reboot_count=%d)", ipModemGetRebootCount());
      }
    }
  }
  else if (Message.startsWith("+PSUTTZ") || Message.startsWith("DST: ")) {
    /*
      [DST: 0
      *PSUTTZ: 21/01/24,23:10:29","+40",0]
     */
    LEAF_NOTICE("Got timestamp from network");
    parseNetworkTime(Message);

    if (!ip_connected && ip_autoconnect) {
      // Having received time means we are back in signal.  Reconnect.
      ipConnect("PSUTTZ");
    }
  }
  else if (Message.startsWith("+SAPBR 1: DEACT")) {
    LEAF_ALERT("Lost TCP connection");
  }
  else if (Message.startsWith("+APP PDP: DEACTIVE")) {
    LEAF_ALERT("Lost application layer connection");
    ipOnDisconnect();
  }
  else if (Message.startsWith("+CMTI: \"SM\",")) {
    LEAF_ALERT("Rceived SMS notification");
    int idx = Message.indexOf(',');
    if (idx > 0) {
      int msg_id = Message.substring(idx+1).toInt();
      ipProcessSMS(msg_id);
    }
  }
  else if (Message == "CONNECT OK") {
    LEAF_INFO("Ignore CONNECT OK");
  }
  else if (Message.startsWith("+RECEIVE,")) {
    int slot = Message.substring(9,10).toInt();
    int size = Message.substring(11).toInt();
    LEAF_NOTICE("Got TCP data for slot %d of %d bytes", slot, size);
    if (ip_clients[slot]) {
      ((IpClientLTE *)ip_clients[slot])->readToBuffer(size);
    }
  }
  else {
    bool result = AbstractIpModemLeaf::modemProcessURC(Message);
    if (!result) {
      // log the unhandled URC
      message("fs", "cmd/appendl/urc.txt", Message);
    }
    return result;
  }
  return true;
}

bool AbstractIpLTELeaf::ipPollGPS(bool force)
{
  LEAF_ENTER(L_NOTICE);

  if (ip_modem_probe_at_gps || !modemIsPresent()) modemProbe(HERE, MODEM_PROBE_QUICK);
  if (!modemIsPresent()) {
    LEAF_NOTICE("Modem is not detected");
    LEAF_BOOL_RETURN(false) ;
  }

  if (!ip_gps_active) {
    LEAF_NOTICE("GPS is not active");
    if (!force) {
      LEAF_NOTICE("Unable to poll GPS, not active");
      LEAF_BOOL_RETURN(false);
    }
    LEAF_NOTICE("Enabling GPS for poll");
    ipEnableGPS();
  }

  String loc = modemQuery("AT+CGNSINF", "+CGNSINF: ", 10000);
  if (loc.length()) {
    LEAF_BOOL_RETURN(parseGPS(loc));
  }
  LEAF_ALERT("Did not get GPS response");
  LEAF_BOOL_RETURN(false);
}

bool AbstractIpLTELeaf::ipPollNetworkTime()
{
  char date_buf[40];

  LEAF_ENTER(L_INFO);
  String datestr = modemSendExpectQuotedField("AT+CCLK?", "+CCLK: ", 1, ',', -1, HERE);
  if (datestr) {
    parseNetworkTime(datestr);
    LEAF_BOOL_RETURN(true);
  }
  LEAF_BOOL_RETURN(false);
}

bool AbstractIpLTELeaf::parseNetworkTime(String datestr)
{
  LEAF_ENTER(L_DEBUG);
  bool result = false;

  /*
   * eg       [DST: 0
   *          *PSUTTZ: 21/01/24,23:10:29","+40",0]
   *
   *
   */
  int index = -1;
  index = datestr.indexOf("DST: ");
  if (index >= 0) {
    if (index > 0) datestr.remove(0, index+5);
    char dstflag = datestr[0];
    LEAF_INFO("LTE network supplies DST flag %c", dstflag);
    ip_clock_dst = (dstflag=='1');
  }

  index = datestr.indexOf("*PSUTTZ: ");
  if (index >= 0) {
    datestr.remove(0, index+9);
  }
  else {
    index = datestr.indexOf("+CCLK: \"");
    if (index >= 0) {
      datestr.remove(0, index+8);
    }
  }

  LEAF_INFO("LTE network supplies TTZ record [%s]", datestr.c_str());
  // We should now be looking at a timestamp like 21/01/24,23:10:29
  //                                              01234567890123456
  //
  struct tm tm;
  struct timeval tv;
  struct timezone tz;
  time_t now;
  char ctimbuf[32];

  if ((datestr.length() > 16) &&
      (datestr[2]=='/') &&
      (datestr[5]=='/') &&
      (datestr[8]==',') &&
      (datestr[11]==':') &&
      (datestr[14]==':')) {
    tm.tm_year = datestr.substring(0,2).toInt() + 100;
    tm.tm_mon = datestr.substring(3,5).toInt()-1;
    tm.tm_mday = datestr.substring(6,8).toInt();
    tm.tm_hour = datestr.substring(9,11).toInt();
    tm.tm_min = datestr.substring(12,14).toInt();
    tm.tm_sec = datestr.substring(15,17).toInt();
    tz.tz_minuteswest = -datestr.substring(18,20).toInt()*15; // americans are nuts
    tz.tz_dsttime = 0;
    tv.tv_sec = mktime(&tm)+60*tz.tz_minuteswest;
    tv.tv_usec = 0;
    LEAF_NOTICE("Parsed time Y=%d M=%d D=%d h=%d m=%d s=%d z=%d",
		(int)tm.tm_year, (int)tm.tm_mon, (int)tm.tm_mday,
		(int)tm.tm_hour, (int)tm.tm_min, (int)tm.tm_sec, (int)tz.tz_minuteswest);
    if (year < 2022) {
      LEAF_WARN("LTE date string [%s] is bogus", datestr.c_str());
      LEAF_BOOL_RETURN(false);
    }
    time(&now);
    if (now != tv.tv_sec) {
      settimeofday(&tv, &tz);
      strftime(ctimbuf, sizeof(ctimbuf), "%FT%T", &tm);
      LEAF_NOTICE("Clock differs from LTE by %d sec, set time to %s.%06d+%02d%02d", (int)abs(now-tv.tv_sec), ctimbuf, tv.tv_usec, -tz.tz_minuteswest/60, abs(tz.tz_minuteswest)%60);
      time(&now);
      char timebuf[32];
      ctime_r(&now, timebuf);
      timebuf[strlen(timebuf)-1]='\0';
      LEAF_NOTICE("Unix time is now %lu (%s)", (unsigned long)now, timebuf);
      ip_time_source = TIME_SOURCE_NETWORK;
      publish("status/time", ctimbuf);
      ACTION("TIME %s", ctimbuf);
      ipCheckGPS();
    }
  }

  LEAF_BOOL_RETURN(result);

}


bool AbstractIpLTELeaf::parseGPS(String gps)
{
  LEAF_ENTER(L_INFO);
  bool result = false;
  bool partial = false;
  int fix = 0;

  if (gps.startsWith("1,1,") || gps.startsWith("1,,")) {
    if (gps.startsWith("1,,")) {
      partial=true;
    }
    LEAF_NOTICE("GPS %s fix %s", partial?"partial":"full", gps.c_str());
    if (ip_modem_publish_gps_raw) {
      mqtt_publish("status/gps", gps.c_str(), 0, false, L_NOTICE, HERE);
    }
    /*
     * eg 1,1,20201012004322.000,-27.565879,152.936990,16.700,0.00,103.8,1,,0.6,0.9,0.7,,25,9,3,,,34,,
     *
     *  1<GNSS run status>,
     *  2<Fix status>,
     *  3<UTC date & Time>,
     *  4<Latitude>,
     *  5<Longitude>,
     *  6<MSL Altitude>,
     *  7<Speed Over Ground>,
     *  8<Course Over Ground>,
     *  9<Fix Mode>
     * 10<Reserved1>
     * 11<HDOP>
     * 12<PDOP>
     * 13<VDOP>
     * 14<Reserved2>
     * 15<GNSS Satellites in View>
     * 16<GNSS Satellites Used>
     * 17<GLONASS Satellites Used>
     * 18<Reserved3>
     * 19<C/N0 max>
     * 20<HPA>
     * 21<VPA>
     */
    int wordno = 0;
    String word;
    float fv;
    bool locChanged = false;
    struct tm tm;
    struct timeval tv;
    struct timezone tz;
    time_t now;
    char ctimbuf[32];

    while (gps.length()) {
      //LEAF_DEBUG("Looking for next word in %s", gps.c_str());
      ++wordno;
      int pos = gps.indexOf(',');
      if (pos < 0) {
	word = gps;
	gps = "";
      }
      else {
	word = gps.substring(0,pos);
	gps.remove(0,pos+1);
      }

      //LEAF_DEBUG("GPS word %d is %s", wordno, word.c_str());
      if (word == "") {
	// this word is blank, skip it
	continue;
      }
      
      switch (wordno) {
      case 1: // run
	break;
      case 2: // fix
	fix = word.toInt();
	break;
      case 3: // date
	tm.tm_year = word.substring(0,4).toInt()-1900;
	tm.tm_mon = word.substring(4,6).toInt()-1;
	tm.tm_mday = word.substring(6,8).toInt();
	tm.tm_hour = word.substring(8,10).toInt();
	tm.tm_min = word.substring(10,12).toInt();
	tm.tm_sec = word.substring(12).toInt();
	tv.tv_sec = mktime(&tm);
	tv.tv_usec = word.substring(14).toInt()*1000;
	tz.tz_minuteswest = 0;
	tz.tz_dsttime = 0;
	time(&now);
	if (now != tv.tv_sec) {
	  settimeofday(&tv, &tz);
	  strftime(ctimbuf, sizeof(ctimbuf), "%FT%T", &tm);
	  LEAF_NOTICE("Clock differs from GPS by %d sec, set time to %s.%06d", (int)abs(now-tv.tv_sec), ctimbuf, tv.tv_usec);
	  ip_time_source = TIME_SOURCE_GPS;
	  publish("status/time", word);
	  ACTION("GPSTIME %s", word.c_str())
	  ipCheckGPS();
	}
	break;
      case 4: // lat
	if (!fix && word == "0.000000") break;
	fv = word.toFloat();
	if (fv != latitude) {
	  latitude = fv;
	  LEAF_DEBUG("Set latitude %f",latitude);
	  locChanged = true;
	}
	break;
      case 5: // lon
	if (!fix && word == "0.000000") break;
	fv = word.toFloat();
	if (fv != longitude) {
	  longitude = fv;
	  LEAF_DEBUG("Set longitude %f",longitude);
	  locChanged = true;
	}
	break;
      case 6: // alt
	fv = word.toFloat();
	if (fv != altitude) {
	  altitude = fv;
	  LEAF_DEBUG("Set altitude %f",altitude);
	}
	break;
      case 7: // speed
	fv = word.toFloat();
	if (fv != speed_kph) {
	  speed_kph = fv;
	  LEAF_DEBUG("Set speed %fkm/h",speed_kph);
	}
	break;
      case 8: // course
	fv = word.toFloat();
	if (fv != heading) {
	  heading = word.toFloat();
	  LEAF_DEBUG("Set heading %f",heading);
	}
	break;
      default: // dont care about rest
	break;
      }
    }

    if (locChanged || ip_modem_publish_location_always) {
      String msg("");

      msg += isnan(latitude)?"":String(latitude,6);
      msg += ",";
      msg += isnan(longitude)?"":String(longitude,6);
      msg += ",";
      msg += isnan(altitude)?"":String(altitude,3);
      msg += ",";
      msg += isnan(speed_kph)?"":String(speed_kph,3);
      msg += ",";
      msg += isnan(heading)?"":String(heading,1);
    
      publish("status/location", msg, L_INFO, HERE);
      ACTION("GPS %s", msg.c_str());
      if (locChanged && prefsLeaf) {
	time_t now = time(NULL);
	LEAF_INFO("Storing time of GPS fix %llu", (unsigned long long)now);
	ip_modem_gps_timestamp = now;
	setULongPref("ip_modem_gps_timestamp", ip_modem_gps_timestamp);
      }
    }

    if (!partial) {
      gps_fix = true;
      
      ip_gps_acquire_time = millis()-ip_gps_active_timestamp;
      if (!ip_enable_gps_always) {
	LEAF_NOTICE("Disable GPS after obtaining lock (acquisition took %dms)", ip_gps_acquire_time);
	ipDisableGPS(true);
      }
    }
  }
  else {
    LEAF_NOTICE("No GPS fix (%s)", gps.c_str());
    gps_fix = false;
  }

  LEAF_BOOL_RETURN(result);
}

bool AbstractIpLTELeaf::ipGPSPowerStatus()  
{
  int i;
  if (modemSendExpectInt("AT+CGNSPWR?","+CGNSPWR: ", &i, -1, HERE)) {
    return (i==1);
  }
  return false;
}

bool AbstractIpLTELeaf::ipEnableGPS() 
{
  LEAF_ENTER(L_NOTICE);
  // bounce the gps off 
  if (modemSendCmd(HERE, "AT+CGNSPWR=0")) {
    ip_gps_active=false;
  }
  delay(50);

  // kill LTE if needed
  if (isConnected() && !ip_simultaneous_gps) {
    // Turning on GPS will kick LTE offline
    LEAF_WARN("Drop IP connection while waiting for GPS lock (this modem is single-channel)");
    ipDisconnect();
    ipCommsState(TRY_GPS, HERE);
  }
  
  ACTION("GPS on");
  ip_gps_active = modemSendCmd(HERE, "AT+CGNSPWR=1");
  if (ip_gps_active) {
    ip_gps_active_timestamp = millis();
  }
  LEAF_BOOL_RETURN(ip_gps_active);
}

bool AbstractIpLTELeaf::ipDisableGPS(bool resumeIp)
{
  LEAF_ENTER_BOOL(L_NOTICE, resumeIp);

  ACTION("GPS off");
  if (modemSendCmd(HERE, "AT+CGNSPWR=0")) {
    ip_gps_active = false;

    if (resumeIp && !isConnected() && ip_autoconnect) {
      ipConnect("Resuming after GPS disabled");
    }
    else if (ip_simultaneous_gps) {
      ipCommsState(OFFLINE, HERE);
    }
    
    LEAF_BOOL_RETURN(true);
  }
  LEAF_BOOL_RETURN(false);
}
      
bool AbstractIpLTELeaf::ipCheckGPS() 
{
  LEAF_ENTER(L_DEBUG);
  
  unsigned long now = millis();
  time_t wall_time = time(NULL);

  if (ip_location_timestamp &&
      ip_location_refresh_interval &&
      (wall_time >= (ip_location_timestamp + ip_location_refresh_interval))
    ) {
    LEAF_NOTICE("GPS location is stale, seeking a new lock");
    gps_fix = false;
    ipEnableGPS();
    
    LEAF_BOOL_RETURN(true);
  }
  LEAF_BOOL_RETURN(false);
}

bool AbstractIpLTELeaf::ipConnect(String reason) 
{
  LEAF_ENTER(L_INFO);

  if (ip_enable_gps && (ip_initial_gps||ip_enable_gps_only) && !ip_simultaneous_gps && !ip_gps_active && !gps_fix && (reason=="initial")) {
    // this modem cannot run GPS and LTE simultaneously, and this is our initial IP connect,
    // so take a moment to grab a GPS fix before bringing up IP
    LEAF_WARN("Delaying IP connect to obtain an initial GPS fix");
    ipEnableGPS();
    LEAF_BOOL_RETURN(false);
  }

  if (!ip_simultaneous_gps && ip_gps_active && !gps_fix) {
    // this modem can't do simultaenous GPS and LTE, so hang back while GPS is busy
    LEAF_WARN("Suppress IP connect while waiting for GPS fix");
    LEAF_BOOL_RETURN(false);
  }
  if (ip_enable_gps_only) {
    // this modem is being used for location only, not for IP
    LEAF_WARN("Suppress IP connect, using modem only for GPS fix");
    LEAF_BOOL_RETURN(false);
  }

  String response = modemQuery("AT");
  if (response == "ATOK") {
    // modem is answering, but needs echo turned off
    modemSendCmd(HERE, "ATE0");
  }

  if (!AbstractIpModemLeaf::ipConnect(reason)) {
    // Superclass says no
    LEAF_BOOL_RETURN(false);
  }

  LEAF_BOOL_RETURN(true); // tell subclass to proceed
}

bool AbstractIpLTELeaf::ipDisconnect(bool retry)
{
  LEAF_ENTER(L_INFO);

  LEAF_NOTICE("Turn off LTE");
  if (!ipLinkDown()) {
    LEAF_ALERT("Disconnect command failed");
  }
  else {
    ipOnDisconnect();
  }

  LEAF_BOOL_RETURN(AbstractIpModemLeaf::ipDisconnect(retry));
}

Client *AbstractIpLTELeaf::newClient(int slot) {
  return (Client *)(new IpClientLTE(this, slot));
}

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
