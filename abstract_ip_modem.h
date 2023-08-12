#pragma once
#include "abstract_ip.h"
class AbstractIpModemLeaf;
#include "trait_modem.h"
//
//@************************* class AbstractIpModemLeaf ***************************
//
// This class encapsulates a TCP/IP interface via an AT-command modem
//

#define MODEM_PWR_PIN_NONE (-1)
#define MODEM_KEY_PIN_NONE (-1)
#define MODEM_SLP_PIN_NONE (-1)

#ifndef IP_MODEM_USE_POWEROFF
#define IP_MODEM_USE_POWEROFF false
#endif

#ifndef IP_MODEM_AUTOPROBE
#define IP_MODEM_AUTOPROBE true
#endif

#ifndef IP_MODEM_AUTOCONNECT
#define IP_MODEM_AUTOCONNECT true
#endif

#ifndef IP_MODEM_RECONNECT
#define IP_MODEM_RECONNECT true
#endif


#define NO_AUTOPROBE false

typedef std::function<bool(int,size_t,const uint8_t *,size_t)> IPModemHttpHeaderCallback;
typedef std::function<bool(const uint8_t *,size_t)> IPModemHttpDataCallback;

class AbstractIpModemLeaf : public AbstractIpLeaf, public TraitModem
{
public:
  using AbstractIpLeaf::getDebugLevel;
  using AbstractIpLeaf::getName;

  AbstractIpModemLeaf(String name, String target, int8_t uart,int rx, int tx, int baud=115200, uint32_t options=SERIAL_8N1,int8_t pwrpin=MODEM_PWR_PIN_NONE, int8_t keypin=MODEM_KEY_PIN_NONE, int8_t sleeppin=MODEM_SLP_PIN_NONE, bool run=LEAF_RUN, bool autoprobe=true)
    : AbstractIpLeaf(name, target, LEAF_PIN(rx)|LEAF_PIN(tx)|LEAF_PIN(pwrpin)|LEAF_PIN(keypin)|LEAF_PIN(sleeppin))
    , TraitModem(name, uart, rx, tx, baud, options, pwrpin, keypin, sleeppin)
    , Debuggable(name)
  {
    this->run = run;
    this->ipModemSetAutoprobe(autoprobe);
    this->modemSetParent(this);
  }

protected:
  int ip_modem_max_file_size = 10240;
  bool ip_modem_use_sleep = true;
  bool ip_modem_use_poweroff = IP_MODEM_USE_POWEROFF;
  bool ip_modem_use_urc = true;
  bool ip_modem_autoprobe = IP_MODEM_AUTOPROBE;
  bool ip_modem_autoconnect = IP_MODEM_AUTOCONNECT;
  bool ip_modem_reconnect = IP_MODEM_RECONNECT;
  bool ip_modem_probe_at_connect = false;
  bool ip_modem_test_after_connect = true;
  bool ip_modem_reuse_connection = true;
  bool ip_modem_needs_reboot = false;
  int ip_modem_reboot_wait_sec = 5;
  bool ip_modem_trace = false;
  int ip_modem_connect_attempt_count = 0;
  int ip_modem_reconnect_interval_sec = 0;
  int ip_modem_reboot_count = 0;
  unsigned long ip_modem_last_reboot = 0;
  unsigned long ip_modem_last_reboot_cmd = 0;
  int ip_modem_connectfail_threshold = 3;
  Ticker ip_modem_probe_timer;
  unsigned long ip_modem_probe_scheduled = 0;
  int ip_modem_probe_interval_sec = NETWORK_RECONNECT_SECONDS;
  bool ip_modem_probe_due = false;
  int ip_modem_urc_check_interval_msec = 100;
  unsigned long ip_modem_last_urc_check = 0;

public:
  virtual bool shouldConnect();
  virtual void setup(void);
  virtual void start(void);
  virtual void pre_sleep(int duration);
  virtual void stop(void);
  virtual void loop(void);

  virtual void readFile(const char *filename, char *buf, int buf_size, int partition=-1,int timeout=-1) {}
  virtual void writeFile(const char *filename, const char *contents, int size=-1, int partition=-1,int timeout=-1){}
  virtual bool writeFileVerify(const char *filename, const char *contents, int size=-1, int partition=-1,int timeout=-1) {return false;}

  virtual bool ipConnect(String reason);
  virtual bool isConnected(codepoint_t where = undisclosed_location)
  {
#if 0
    // too much spew
    if (ip_modem_probe_at_connect && !modemProbe(CODEPOINT(where), MODEM_PROBE_QUICK)) {
      return false;
    }
#endif
    return AbstractIpLeaf::isConnected(CODEPOINT(where));
  }
  virtual bool commandHandler(String type, String name, String topic, String payload);
  virtual bool valueChangeHandler(String topic, Value *v);

  virtual void ipModemSetNeedsReboot() {
    LEAF_NOTICE("modem reboot requested");
    ip_modem_needs_reboot = true;
  }
  virtual void ipModemSetAutoprobe (bool s) { ip_modem_autoprobe = s; }
  void ipModemSetProbeDue() {
    ip_modem_probe_scheduled = 0;
    ip_modem_probe_due=true;
  }
  virtual void ipScheduleProbe(int delay=-1);

  virtual bool ipModemNeedsReboot() { return ip_modem_needs_reboot; }
  virtual void ipModemRecordReboot()
  {
    ++ip_modem_reboot_count;
    fslog(HERE, IP_LOG_FILE, "modem reboot %d", ip_modem_reboot_count);
    setIntPref("ip_modem_reboots", ip_modem_reboot_count);
    ip_modem_last_reboot = millis();
  }
  virtual int ipModemGetRebootCount() { return ip_modem_reboot_count; }
  virtual int ipModemReboot(codepoint_t where=undisclosed_location) {
    LEAF_ENTER(L_NOTICE);

    ip_modem_last_reboot_cmd = millis();

    if (modemSendCmd(where, "AT+CFUN=1,1")) {
      ACTION("MODEM soft reboot");
      // modem responded to a software reboot request
      fslog(HERE, IP_LOG_FILE, "modem probe soft reboot");
      if (modemProbe(HERE, MODEM_PROBE_QUICK)) {
	LEAF_BOOL_RETURN(true);
      }
    }

    // Modem not responding to software reboot, use the hardware
    if (pin_power >= 0) {
      // We have direct power control, use that
      ACTION("MODEM hard poweroff");
      modemSetPower(false);
      delay(500);
      modemSetPower(true);
      ACTION("MODEM hard poweron");
      LEAF_BOOL_RETURN(true);
    }
    else if (pin_key >= 0) {
      ACTION("MODEM soft poweroff");
      modemPulseKey(false);
      delay(1000);
      modemPulseKey(true);
      ACTION("MODEM soft poweron");
      LEAF_BOOL_RETURN(true);
    }

    // was unable to act
    LEAF_ALERT("Could not reboot modem");
    LEAF_BOOL_RETURN(false);
  }
  virtual void ipModemPowerOff(codepoint_t where=undisclosed_location)
  {
    LEAF_WARN_AT(CODEPOINT(where),"Modem power soft-off (via AT+CPOWD)");
    ACTION("MODEM softoff");
    modemSendExpect("AT+CPOWD=0","NORMAL POWER DOWN",NULL,0,100,2,HERE);
  }

  virtual void ipOnConnect()
  {
    AbstractIpLeaf::ipOnConnect();
    ip_modem_connect_attempt_count = 0;
  }
  virtual void ipScheduleReconnect()
  {
    LEAF_ENTER(L_NOTICE);
    AbstractIpLeaf::ipScheduleReconnect();
    if ((ip_modem_connectfail_threshold > 0) &&
	(ip_modem_connect_attempt_count > 0) &&
	((ip_modem_connect_attempt_count % ip_modem_connectfail_threshold)==0)) {
      LEAF_WARN("Consecutive failed connection attempts (%d) exceeds threshold (%d).  Reinitialise modem",
		ip_modem_connect_attempt_count, ip_modem_connectfail_threshold);
      ipModemSetNeedsReboot();
    }
    else if (ip_modem_connectfail_threshold && ip_modem_connect_attempt_count>0) {
      LEAF_NOTICE("There have been %d/%d consecutive failed connection attempts",
		  ip_modem_connect_attempt_count, ip_modem_connectfail_threshold);
    }
    LEAF_LEAVE;
  }


  virtual int modemHttpGetWithCallback(const char *url,
			       IPModemHttpHeaderCallback header_callback,
			       IPModemHttpDataCallback chunk_callback,
			       int bearer=0,
			       int chunk_size=2048)
  {
    LEAF_ALERT("This function must be overridden in subclass");
    return false;
  };


};

void AbstractIpModemLeaf::setup(void) {
  AbstractIpLeaf::setup();
  LEAF_ENTER(L_INFO);
  if (canRun()) {
    modemSetup();
  }

  registerCommand(HERE,"modem_key", "trigger the modem power key (1=on, 0=off)");
  registerCommand(HERE,"modem_test", "connect the console directly to the modem (console cli only!)");
  registerCommand(HERE,"modem_reboot", "Reboot the modem (modem only, not ESP)");
  registerCommand(HERE,"modem_probe", "Initiate modem presence detection");
  registerCommand(HERE,"modem_off", "Turn the modem off");
  registerCommand(HERE,"modem_status", "Get the modem status");
  registerCommand(HERE,"modem_at", "Send a single AT command and report the response");
  registerCommand(HERE,"modem_http_get", "Fetch a file via http");

  registerBoolValue("ip_modem_trace", &ip_modem_trace, "print trace of modem exchanges to console");

#ifdef ESP32
  registerBoolValue("ip_modem_own_loop", &own_loop, "Use a separate thread for modem connection management");
#endif

  modemSetTrace(ip_modem_trace);
  registerBoolValue("ip_modem_use_sleep", &ip_modem_use_sleep, "Put modem to sleep if possible");
  registerBoolValue("ip_modem_use_poweroff", &ip_modem_use_poweroff, "Turn off modem power when possible");
  registerBoolValue("ip_modem_use_urc", &ip_modem_use_urc, "Periodically check for Unsolicted Result Codes (URC)");
  registerBoolValue("ip_modem_autoprobe", &ip_modem_autoprobe, "Probe for modem at startup");
  registerBoolValue("ip_modem_autoconnect", &ip_modem_autoconnect, "Auto connect via modem (overrides ip_autoconnect)");
  registerBoolValue("ip_modem_reconnect", &ip_modem_reconnect, "Automatically schedule an IP/modem reconnect after loss of IP");
  registerIntValue("ip_modem_reconnect_interval_sec", &ip_modem_reconnect_interval_sec, "IP/modem reconnect time in seconds (0=immediate)");

  registerBoolValue("ip_modem_probe_at_connect", &ip_modem_probe_at_connect, "Confirm the modem is answering before attempting to connect");
  registerBoolValue("ip_modem_probe_at_urc", &modem_probe_at_urc, "Confirm the modem is answering, as part of unsolicited result check");
  registerBoolValue("ip_modem_test_after_connect", &ip_modem_test_after_connect, "Make a DNS query after connect to confirm that IP is really working");
  registerBoolValue("ip_modem_reuse_connection", &ip_modem_reuse_connection, "If modem is already connected, reuse existing connection");
  registerIntValue("ip_modem_reboot_wait_sec", &ip_modem_reboot_wait_sec, "Time in seconds to wait for modem reboot");
  registerIntValue("ip_modem_max_file_size", &ip_modem_max_file_size, "Maximum file size for transfers");
  registerIntValue("ip_modem_chat_trace_level", &modem_chat_trace_level, "Log level for modem chat trace");
  registerIntValue("ip_modem_mutex_trace_level", &modem_mutex_trace_level, "Log level for modem mutex trace");
  registerIntValue("ip_modem_reboots", &ip_modem_reboot_count, "Number of unexpected modem reboots");
  registerIntValue("ip_modem_connectfail_threshold", &ip_modem_connectfail_threshold, "Reboot modem after N failed connect attempts");
  registerIntValue("ip_modem_urc_check_interval_msec", &ip_modem_urc_check_interval_msec, "Interval between modem URC checks");

  LEAF_LEAVE_SLOW(2000);
}

void AbstractIpModemLeaf::start(void)
{
  LEAF_ENTER(L_INFO);
  AbstractIpLeaf::start();

  if (!modemIsPresent() && ip_modem_autoprobe) {
    fslog(HERE, IP_LOG_FILE, "modem probe set_due");
    ipModemSetProbeDue();
  }
  LEAF_LEAVE;
}

void AbstractIpModemLeaf::stop(void)
{
  LEAF_ENTER(L_NOTICE);

  if (modemIsPresent()) {
    if (isConnected(HERE)) {
      if ((pubsubLeaf->getIpComms()==this) && pubsubLeaf->isConnected()) {
	// we are jerking the rug out from under the active pubsub, give it a heads-up
	pubsubLeaf->pubsubDisconnect();
      }
      // Lewis, Hang it up.
      ipDisconnect();
    }
    ipModemPowerOff(HERE);
  }

  AbstractIpLeaf::stop();
}

void AbstractIpModemLeaf::pre_sleep(int duration)
{
  AbstractIpLeaf::pre_sleep(duration);
  LEAF_ENTER(L_WARN);
  ipModemPowerOff(HERE);
  LEAF_LEAVE;
}

void ipModemProbeTimerCallback(AbstractIpModemLeaf *leaf) { leaf->ipModemSetProbeDue(); }

void AbstractIpModemLeaf::ipScheduleProbe(int interval)
{
  AbstractIpLeaf::ipScheduleProbe(interval);
  LEAF_ENTER(L_WARN);
  if (interval == -1) interval = ip_modem_probe_interval_sec;

  if (interval==0) {
    LEAF_WARN("Immediate modem re-probe");
    ipModemSetProbeDue();
  }
  else {
    unsigned long now_sec = time(NULL);
    unsigned long schedule_probe_at = now_sec + interval;
    if (ip_modem_probe_scheduled && (ip_modem_probe_scheduled < now_sec)) {
      fslog(HERE, IP_LOG_FILE, "modem probe overdue");
      LEAF_ALERT("Probe already overdue");
      ipModemSetProbeDue();
    }
    else if (ip_modem_probe_scheduled && (ip_modem_probe_scheduled < schedule_probe_at)) {
      fslog(HERE, IP_LOG_FILE, "modem probe already scheduled");
      LEAF_ALERT("Probe is already scheduled for an earlier time");
    }
    else {
      LEAF_WARN("Scheduling modem re-probe in %ds at %lu", interval, schedule_probe_at);
      ip_modem_probe_scheduled = schedule_probe_at;
      ip_modem_probe_timer.once(interval,
				&ipModemProbeTimerCallback,
				this);
    }
  }
  LEAF_LEAVE;
}

bool AbstractIpModemLeaf::shouldConnect()
{
  unsigned long uptime_sec = millis()/1000;
  return canRun() && modemIsPresent() && isAutoConnect() && !isConnected(HERE) && (uptime_sec >= ip_delay_connect);
}

bool AbstractIpModemLeaf::ipConnect(String reason)
{
  if (!AbstractIpLeaf::ipConnect(reason)) {
    // Superclass says no
    return false;
  }
  LEAF_ENTER_STR(L_NOTICE, reason);
  bool present = modemIsPresent();

  if (reason == "reconnect") {
    LEAF_NOTICE("Probing modem due to previous loss of connection");
    fslog(HERE, IP_LOG_FILE, "modem probe reconnect");
    present = modemProbe(HERE,MODEM_PROBE_QUICK);
  }
  else if (ip_modem_probe_at_connect) {
    LEAF_NOTICE("Probing modem due to probe_at_connect");
    fslog(HERE, IP_LOG_FILE, "modem probe ipConnect");
    present = modemProbe(HERE,MODEM_PROBE_QUICK);
  } else if (!present) {
    LEAF_NOTICE("Probing modem due to previous non-detection");
    fslog(HERE, IP_LOG_FILE, "lte modem ipConnect notpresent");
    present = modemProbe(HERE,MODEM_PROBE_QUICK);
  }

  if (!present) {
    LEAF_WARN("Cannot connect: Modem is not present");
    ipScheduleProbe();
  }
  else {
    ++ip_modem_connect_attempt_count;
    if (ip_modem_connect_attempt_count > 1) {
      LEAF_WARN("Connection attempt %d (modem reinit threshold is %d)", ip_modem_connect_attempt_count, ip_modem_connectfail_threshold);
    }
  }
  LEAF_BOOL_RETURN(present);
}

void AbstractIpModemLeaf::loop()
{
  static bool first = true;
  LEAF_ENTER(L_TRACE);
  unsigned long uptime_sec = millis()/1000;

  if (ip_modem_autoprobe && ip_modem_probe_due && (uptime_sec >= ip_delay_connect)) {
    ip_modem_probe_due = false;
    LEAF_NOTICE("Attempting to auto-probe IP modem");
    fslog(HERE, IP_LOG_FILE, "modem probe scheduled");
    modemProbe(HERE);
    if (!modemIsPresent()) {
      ipScheduleProbe();
    }
  }

  if (ipModemNeedsReboot()) {
    LEAF_ALERT("Attempting to reboot modem");
    fslog(HERE, IP_LOG_FILE, "modem probe pre_reboot");
    if (modemProbe(HERE,MODEM_PROBE_QUICK)) {
      ip_modem_needs_reboot = false;
      ipModemReboot(HERE);
      //ip_modem_connect_attempt_count = 0;
      ip_reconnect_due = false;
      ipReconnectTimer.once(ip_modem_reboot_wait_sec,
			    &ipReconnectTimerCallback,
			    (AbstractIpLeaf *)this);
    }
    else {
      LEAF_ALERT("Modem not ready for reboot");
      comms_state(WAIT_MODEM, HERE);
      ipScheduleProbe();
    }
  }

  AbstractIpLeaf::loop();

  if (first && shouldConnect()) {
    first = false;
    LEAF_NOTICE("Initial IP autoconnect");
    ipConnect("initial");
  }

  if (ip_modem_use_urc && modemIsPresent()) {
    unsigned long now = millis();
    if (now >= (ip_modem_last_urc_check + ip_modem_urc_check_interval_msec)) {
      ip_modem_last_urc_check = now;
      modemCheckURC();
    }
  }
  LEAF_LEAVE;
}


bool AbstractIpModemLeaf::valueChangeHandler(String topic, Value *v) {
  LEAF_HANDLER(L_INFO);

  WHEN("modem_sleep",{
      modemSetSleep(VALUE_AS_BOOL(v));
    })
  ELSEWHEN("ip_modem_autoconnect", ip_autoconnect = VALUE_AS_BOOL(v))
  ELSEWHEN("ip_modem_reconnect", ip_reconnect = VALUE_AS_BOOL(v))
  ELSEWHEN("ip_modem_reconnect_interval_sec", ip_reconnect_interval_sec = VALUE_AS_INT(v))
  else {
    AbstractIpLeaf::valueChangeHandler(topic, v);
  }

  LEAF_HANDLER_END;
}

bool AbstractIpModemLeaf::commandHandler(String type, String name, String topic, String payload) {
  LEAF_HANDLER(L_INFO);

  WHEN("modem_key",{
    int d = payload.toInt();
    if (d > 1) {
      // integer-like argument
      modemPulseKey(d);
    }
    else {
      // boolean-like argument
      modemPulseKey(parsePayloadBool(payload, true));
    }
  })
  ELSEWHEN("modem_test",{
    modemChat(&Serial, parsePayloadBool(payload,true));
  })
  ELSEWHEN("modem_reboot",{
    ipModemReboot(HERE);
  })
  ELSEWHEN("modem_probe",{
    if (payload == "no_presence") {
      LEAF_NOTICE("Doing modem probe without updating presence");
      fslog(HERE, IP_LOG_FILE, "modem probe command no_presence");
      modemProbe(HERE, false, true);
    }
    else {
      fslog(HERE, IP_LOG_FILE, "modem probe command quick");
      modemProbe(HERE,(payload=="quick"));
      mqtt_publish("status/modem", TRUTH_lc(modemIsPresent()));
    }
  })
  ELSEWHEN("modem_off",{
    ipModemPowerOff(HERE);
    modemSetPresent(false);
  })
  ELSEWHEN("modem_http_get",{
    modemHttpGetWithCallback(
      payload.c_str(),
      [](int status, size_t len, const uint8_t *hdr, size_t hdr_len) -> bool {
	INFO("HTTP header code=%d len=%lu", status, (unsigned long)len);
	//mqtt_publish("status/modem_http_get/code", status);
	//mqtt_publish("status/modem_http_get/size", len);
	return true;
      },
      [](const uint8_t *buf, size_t len) -> bool {
	INFO("HTTP chunk callback len=%lu", (unsigned long)len);
	//mqtt_publish(String("status/modem_http_get/chunk/")+String(chunk_num), len);
	//NOTICE("%s", (char *)buf);
	return true;
      });
  })
  ELSEWHEN("modem_status",{
    mqtt_publish("status/modem", TRUTH_lc(modemIsPresent()));
  })
  ELSEWHEN("modem_at",{
    if (!modemWaitPortMutex()) {
      LEAF_ALERT("Cannot take modem mutex");
    }
    else {
      //LEAF_INFO("Send AT command %s", payload.c_str());
      String result = modemQuery(payload,5000,HERE);
      modemReleasePortMutex();
      mqtt_publish("status/at", result);
    }
  })
  else {
    AbstractIpLeaf::commandHandler(type, name, topic, payload);
  }

  LEAF_HANDLER_END;
}





// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
