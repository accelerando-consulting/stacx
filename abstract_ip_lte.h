#pragma once
#include "abstract_ip_modem.h"

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
  {
    ip_ap_name = "telstra.m2m";
  }
  
  virtual void setup(void);
  virtual void start(void);
  virtual void loop(void);
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

  virtual bool ipDisconnect(bool retry=false);
  virtual bool ipLinkUp() { return modemSendCmd(HERE, "AT+CNACT=1"); }
  virtual bool ipLinkDown() { return modemSendCmd(HERE, "AT+CNACT=0"); }
  virtual bool ipLinkStatus();

protected:
  virtual void ipOnConnect();
  bool ipProcessSMS(int index=-1);
  bool ipProcessNetworkTime(String Time);
  bool ipCheckTime();
  bool ipCheckGPS();
  bool ipEnableGPS();
  bool ipGPSPowerStatus();
  bool ipDisableGPS();
  bool ipPollGPS();
  bool ipPollNetworkTime();
  virtual void ipPublishTime(void);
  virtual bool parseNetworkTime(String datestr);
  virtual bool parseGPS(String gps);
  virtual bool modemProcessURC(String Message);
  
  bool ip_enable_gps = true;
  bool ip_enable_gps_always = false;
  bool ip_enable_rtc = true;
  bool ip_enable_sms = true;
  bool ip_clock_dst = false;
  bool ip_modem_probe_at_sms = false;
  bool ip_modem_probe_at_gps = false;
  int ip_time_source = 0;
  int ip_location_refresh_interval = 86400;
  time_t ip_location_timestamp = 0;
  bool ip_gps_active = false;
  int ip_ftp_timeout_sec = 30;
  
  bool ip_abort_no_service = false;
  bool ip_abort_no_signal = true;

  String ip_device_type="";
  String ip_device_imei="";
  String ip_device_iccid="";
  String ip_device_version="";

  int8_t battery_percent;
  float latitude, longitude, speed_kph, heading, altitude, second;
  time_t location_timestamp = 0;
  time_t location_refresh_interval = 86400;
  uint16_t year;
  uint8_t month, day, hour, minute;
  unsigned long gps_check_interval = 600*1000;
  unsigned long sms_check_interval = 300*1000;
  unsigned long last_gps_check = 0;
  unsigned long last_sms_check = 0;
  bool gps_fix = false;
};

void AbstractIpLTELeaf::setup(void) {
    AbstractIpModemLeaf::setup();
    LEAF_ENTER(L_NOTICE);
    getBoolPref("ip_abort_no_service", &ip_abort_no_service, "Check cellular service before connecting");
    getBoolPref("ip_abort_no_signal", &ip_abort_no_signal, "Check cellular signal strength before connecting");
    getBoolPref("ip_enable_ssl", &ip_enable_ssl, "Use SSL for TCP connections");
    getBoolPref("ip_enable_gps", &ip_enable_gps, "Enable GPS receiver");
    getBoolPref("ip_enable_gps_always", &ip_enable_gps_always, "Continually track GPS location");
    getBoolPref("ip_enable_rtc", &ip_enable_rtc, "Use clock in modem");
    getBoolPref("ip_enable_sms", &ip_enable_sms, "Process SMS via modem");
    getIntPref("ip_location_refresh_interval_sec", &ip_location_refresh_interval, "Periodically check location");
    getIntPref("ip_ftp_timeout_sec", &ip_ftp_timeout_sec, "Timeout (in seconds) for FTP operations");

    // if set ip_lte_ap* overrids ip_ap_* for LTE modem
    ip_ap_name = getPref("ip_lte_ap_name", ip_ap_name, "LTE Access point name");
    ip_ap_user = getPref("ip_lte_ap_user", ip_ap_user, "LTE Access point username");
    ip_ap_pass = getPref("ip_lte_ap_pass", ip_ap_pass, "LTE Access point password");

    LEAF_LEAVE;
  }

void AbstractIpLTELeaf::start(void) 
{
  AbstractIpModemLeaf::start();
  LEAF_ENTER(L_NOTICE);

  
  LEAF_VOID_RETURN;
}

void AbstractIpLTELeaf::onModemPresent() 
{
  AbstractIpModemLeaf::onModemPresent();
  LEAF_ENTER(L_NOTICE);

  // Modem was detected for the first time after poweroff or reboot
  if (modemIsPresent()) {
    ip_device_type = modemQuery("AT+CGMM","");
    ip_device_imei = modemQuery("AT+CGSN","");
    ip_device_iccid = modemQuery("AT+CCID","");
    ip_device_version = modemQuery("AT+CGMR","Revision:");
  }
  if (!ip_enable_gps) {
    ipDisableGPS();
  }
}


void AbstractIpLTELeaf::loop(void) 
{
  AbstractIpModemLeaf::loop();
  if (!canRun() || !modemIsPresent()) return;
  
  if (ip_enable_gps && gpsConnected() && !ip_gps_active) ipCheckGPS();
  
  unsigned long interval = gpsConnected()?gps_check_interval:60*1000;
  if (isConnected() && ip_gps_active && ((last_gps_check + interval) <= millis())) {
    ipPollGPS();
    last_gps_check = millis();
  }
  
  if (ip_enable_sms && (millis() >= (last_sms_check+sms_check_interval))) {
    last_sms_check=millis();
    ipProcessSMS();
  }
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
  String status = modemQuery("AT+CNACT?", "+CNACT: ");
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
  LEAF_ENTER(L_NOTICE);
  
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

bool AbstractIpLTELeaf::mqtt_receive(String type, String name, String topic, String payload)
{
  bool handled = AbstractIpModemLeaf::mqtt_receive(type, name, topic, payload);
  LEAF_INFO("AbstractIpLTELeaf mqtt_receive %s %s => %s", type.c_str(), name.c_str(), topic.c_str());
  WHEN("get/lte_time",{
      ipPublishTime();
    })
    ELSEWHEN("get/gps",{
	ipPollGPS();
      })
    ELSEWHEN("cmd/lte_time",{
	ipPollNetworkTime();
      })
    ELSEWHEN("cmd/ip_ping",{
	ipPing(payload);
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
    ELSEWHEN("get/ip_lte_signal",{
	//LEAF_INFO("Check signal strength");
	String rsp = modemQuery("AT+CSQ","+CSQ: ");
	mqtt_publish("status/lte_signal", rsp);
      })
    ELSEWHEN("get/ip_lte_network",{
	//LEAF_INFO("Check network status");
	String rsp = modemQuery("AT+CPSI?","+CPSI: ");
	mqtt_publish("status/lte_network", rsp);
      })
    ELSEWHEN("get/sms_count",{
	int count = getSMSCount();
	mqtt_publish("status/smscount", String(count));
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
	LEAF_DEBUG("sim7000 cmd/sms");
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
  

void AbstractIpLTELeaf::ipPublishTime(void)
{
    time_t now;
    struct tm localtm;
    char ctimbuf[32];
    time(&now);
    localtime_r(&now, &localtm);
    strftime(ctimbuf, sizeof(ctimbuf), "%FT%T", &localtm);
    mqtt_publish("status/time", ctimbuf);
    mqtt_publish("status/time_source", (ip_time_source==TIME_SOURCE_GPS)?"GPS":(ip_time_source==TIME_SOURCE_NETWORK)?"NETWORK":"SELF");
}

int AbstractIpLTELeaf::getSMSCount()
{
  if (!modemIsPresent()) return 0;
  
  if (!modemSendCmd(HERE, "AT+CMGF=1")) {
    // maybe the modem fell asleep
    if (modemProbe(HERE,MODEM_PROBE_QUICK) && modemSendCmd(HERE, "AT+CMGF=1")) {
      LEAF_NOTICE("Successfully woke modem");
    }
    else {
      LEAF_ALERT("SMS format command not accepted");
      return 0;
    }
  }
  String response = modemQuery("AT+CPMS?","+CPMS: ");
  int count = 0;
  

  // FIXME NORELEASE: implement
  return count;
}

String AbstractIpLTELeaf::getSMSText(int msg_index)
{
  if (!modemIsPresent()) return "";
  if (!modemSendCmd(HERE, "AT+CMGF=1")) {
    LEAF_ALERT("SMS format command not accepted");
    return "";
  }

  int sms_len;
  snprintf(modem_command_buf, modem_command_max, "AT+CMGR=%d", msg_index);
  if (!modemSendExpectIntField(modem_command_buf, "+CMGR: ", 11, &sms_len, ',', -1, HERE)) {
    LEAF_ALERT("Error requesting message %d", msg_index);
    return "";
  }
  if (sms_len >= modem_response_max) {
    LEAF_ALERT("SMS message length (%d) too long", sms_len);
    return "";
  }
  if (!modemGetReplyOfSize(modem_response_buf, sms_len, modem_timeout_default*4)) {
    LEAF_ALERT("SMS message read failed");
    return "";
  }
  modem_response_buf[sms_len]='\0';
  LEAF_NOTICE("Got SMS message: %d %s", msg_index, modem_response_buf);

  return String(modem_response_buf);
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
  LEAF_ENTER_INT(L_NOTICE, msg_index);
  int first,last;

  if (ip_modem_probe_at_sms || !modemIsPresent()) modemProbe(HERE,MODEM_PROBE_QUICK);
  if (!modemIsPresent()) {
    LEAF_BOOL_RETURN(false);
  }
  if (!modemWaitPortMutex(HERE)) {
    LEAF_ALERT("Cannot obtain modem mutex");
  }
  
  if (!modemSendCmd(HERE, "AT+CMGF=1")) {
    LEAF_ALERT("SMS text format command not accepted");
    modemReleasePortMutex(HERE);
    LEAF_BOOL_RETURN(false);
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
    String topic;
    String payload;
    int sep;

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
      LEAF_INFO("Processing one line of SMS as a Bogo-MQTT: %s", command.c_str());
      if (!command.length()) continue;

      if ((sep = command.indexOf(' ')) >= 0) {
	topic = command.substring(0,sep);
	payload = command.substring(sep+1);
      }
      else {
	topic = command;
	payload = "1";
      }

      pubsubLeaf->_mqtt_receive(topic, payload, PUBSUB_LOOPBACK);
      reply += pubsubLeaf->getLoopbackBuffer() + "\r\n";
    } while (msg.length());

    // We have now accumulated results in reply
    if (sender) {
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
  LEAF_NOTICE("Asynchronous Modem input: [%s]", Message.c_str());

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
      LEAF_ALERT("Modem rebooted unexpectedly (reboot_count=%d)", ipModemGetRebootCount());
    }
  }
  else if (Message.startsWith("+PSUTTZ") || Message.startsWith("DST: ")) {
    /*
      [DST: 0
      *PSUTTZ: 21/01/24,23:10:29","+40",0]
     */
    LEAF_INFO("Got timestamp from network");
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
  }
  else if (Message.startsWith("+CMTI: \"SM\",")) {
    LEAF_ALERT("Got SMS");
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
#if 0
    if (clients[slot]) {
      clients[slot]->readToBuffer(size);
    }
#endif
  }
  else {
    bool result = AbstractIpModemLeaf::modemProcessURC(Message);
    if (!result) {
      // log the unhandled URC
      message("fs", "cmd/append/urc.txt", Message);
    }
    return result;
  }
  return true;
}

bool AbstractIpLTELeaf::ipPollGPS()
{
  LEAF_ENTER(L_NOTICE);
  if (!ip_gps_active) {
    LEAF_BOOL_RETURN(false);
  }

  if (ip_modem_probe_at_gps || !modemIsPresent()) modemProbe(HERE, MODEM_PROBE_QUICK);
  if (!modemIsPresent()) {
    LEAF_BOOL_RETURN(false) ;
  }
  
  String loc = modemQuery("AT+CGNSINF", "+CGNSINF: ", 10000);
  if (loc) {
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
    LEAF_DEBUG("Parsed time Y=%d M=%d D=%d h=%d m=%d s=%d z=%d",
		(int)tm.tm_year, (int)tm.tm_mon, (int)tm.tm_mday,
		(int)tm.tm_hour, (int)tm.tm_min, (int)tm.tm_sec, (int)tz.tz_minuteswest);
    time(&now);
    if (now != tv.tv_sec) {
      settimeofday(&tv, &tz);
      strftime(ctimbuf, sizeof(ctimbuf), "%FT%T", &tm);
      LEAF_INFO("Clock differs from LTE by %d sec, set time to %s.%06d+%02d%02d", (int)abs(now-tv.tv_sec), ctimbuf, tv.tv_usec, -tz.tz_minuteswest/60, abs(tz.tz_minuteswest)%60);
      time(&now);
      char timebuf[32];
      ctime_r(&now, timebuf);
      timebuf[strlen(timebuf)-1]='\0';
      LEAF_NOTICE("Unix time is now %llu (%s)", (unsigned long long)now, timebuf);
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

  if (gps.startsWith("1,1,")) {
    LEAF_NOTICE("GPS fix %s", gps.c_str());
    mqtt_publish("status/gps", gps.c_str());
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
      switch (wordno) {
      case 1: // run
      case 2: // fix
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
	fv = word.toFloat();
	if (fv != latitude) {
	  latitude = fv;
	  LEAF_DEBUG("Set latitude %f",latitude);
	  locChanged = true;
	}
	break;
      case 5: // lon
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

    if (locChanged) {
      publish("status/location", String(latitude,6)+","+String(longitude,6));
      ACTION("GPS %.6f,%.6f", latitude, longitude);
      if (prefsLeaf) {
	time_t now = time(NULL);
	LEAF_INFO("Storing time of GPS fix %llu", (unsigned long long)now);
	prefsLeaf->put("lte_loc_ts", String(now));
      }
    }
    gps_fix = true;
    if (!ip_enable_gps_always) {
      LEAF_NOTICE("Disable GPS after obtaining lock");
      ipDisableGPS();
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
  ACTION("GPS on");
  ip_gps_active = modemSendCmd(HERE, "AT+CGNSPWR=1");
  return ip_gps_active;
}

bool AbstractIpLTELeaf::ipDisableGPS() 
{
  ACTION("GPS off");
  if (modemSendCmd(HERE, "AT+CGNSPWR=0")) {
    ip_gps_active = false;
    return true;
  }
  return false;
}
      
bool AbstractIpLTELeaf::ipCheckGPS() 
{
  if (location_timestamp &&
      location_refresh_interval &&
      (time(NULL) >= (location_timestamp + location_refresh_interval))
    ) {
    LEAF_NOTICE("GPS location is stale, seeking a new lock");
    ip_gps_active = true;
    ipEnableGPS();
    
    return true;
  }
  return false;
}

bool AbstractIpLTELeaf::ipDisconnect(bool retry)
{
  LEAF_ENTER(L_NOTICE);

  LEAF_NOTICE("Turn off GPS");
  if (ip_gps_active) {
    if (!ipDisableGPS()) {
      LEAF_ALERT("GPS disable failed");
    }
  }
  
  LEAF_NOTICE("Turn off LTE");
  if (!ipLinkDown()) {
    LEAF_ALERT("Disconnect command failed");
  }
  else {
    ipOnDisconnect();
  }

  LEAF_BOOL_RETURN(AbstractIpModemLeaf::ipDisconnect(retry));
}

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
