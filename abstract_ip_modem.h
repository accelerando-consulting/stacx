#pragma once
#include "abstract_ip.h"
#include "trait_modem.h"
//
//@************************* class AbstractIpModemLeaf ***************************
//
// This class encapsulates a TCP/IP interface via an AT-command modem
//

#define MODEM_PWR_PIN_NONE (-1)
#define MODEM_KEY_PIN_NONE (-1)
#define MODEM_SLP_PIN_NONE (-1)

#define NO_AUTOPROBE false

typedef std::function<bool(int,size_t,const uint8_t *)> IPModemHttpHeaderCallback;
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

  virtual void setup(void);
  virtual void start(void);
  virtual void pre_sleep(int duration);
  virtual void stop(void);
  virtual void loop(void);

  virtual void readFile(const char *filename, char *buf, int buf_size, int partition=-1,int timeout=-1) {}
  virtual void writeFile(const char *filename, const char *contents, int size=-1, int partition=-1,int timeout=-1){}
  virtual bool writeFileVerify(const char *filename, const char *contents, int size=-1, int partition=-1,int timeout=-1) {return false;}

  virtual bool mqtt_receive(String type, String name, String topic, String payload, bool direct=false);
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

  virtual void ipModemSetNeedsReboot() {
    LEAF_WARN("modem reboot requested");
    ip_modem_needs_reboot = true;
  }
  virtual void ipModemSetAutoprobe(bool s) { ip_modem_autoprobe = s; }
  void ipModemSetProbeDue() {ip_modem_probe_due=true;}
  virtual void ipModemScheduleProbe(int delay=-1);

  virtual bool ipModemNeedsReboot() { return ip_modem_needs_reboot; }
  virtual void ipModemRecordReboot() 
  {
    ++ip_modem_reboot_count;
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
    LEAF_WARN_AT(CODEPOINT(where),"Modem power off");
    ACTION("MODEM off");
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
	(ip_modem_connect_attempt_count >= ip_modem_connectfail_threshold)) {
      LEAF_WARN("Consecutive failed connection attempts (%d) exceeds threshold (%d).  Reinitialise modem",
		ip_modem_connect_attempt_count, ip_modem_connectfail_threshold);
      ipModemSetNeedsReboot();
    }
    else if (ip_modem_connectfail_threshold && ip_modem_connect_attempt_count>1) {
      LEAF_NOTICE("There have been %d consecutive failed connection attempts",
		  ip_modem_connect_attempt_count);
    }
    LEAF_LEAVE;
  }
protected:
  virtual bool shouldConnect();

  int ip_modem_max_file_size = 10240;
  bool ip_modem_use_sleep = true;
  bool ip_modem_use_poweroff = false;
  bool ip_modem_use_urc = true;
  bool ip_modem_autoprobe = true;
  bool ip_modem_probe_at_connect = false;
  bool ip_modem_test_after_connect = true;
  bool ip_modem_reuse_connection = true;
  bool ip_modem_needs_reboot = false;
  int ip_modem_reboot_wait_sec = 5;
  bool ip_modem_trace = false;
  int ip_modem_connect_attempt_count = 0;
  int ip_modem_reboot_count = 0;
  unsigned long ip_modem_last_reboot = 0;
  unsigned long ip_modem_last_reboot_cmd = 0;
  int ip_modem_connectfail_threshold = 3;
  Ticker ip_modem_probe_timer;
  int ip_modem_probe_interval_sec = NETWORK_RECONNECT_SECONDS;
  bool ip_modem_probe_due = false;
  int ip_modem_urc_check_interval_msec = 100;
  unsigned long ip_modem_last_urc_check = 0;

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
 
  registerBoolValue("ip_modem_trace", &ip_modem_trace, "print trace of modem exchanges to console");

#ifdef ESP32
  registerBoolValue("ip_modem_own_loop", &own_loop, "Use a separate thread for modem connection management");
#endif
  
  modemSetTrace(ip_modem_trace);
  registerBoolValue("ip_modem_use_sleep", &ip_modem_use_sleep, "Put modem to sleep if possible");
  registerBoolValue("ip_modem_use_poweroff", &ip_modem_use_poweroff, "Turn off modem power when possible");
  registerBoolValue("ip_modem_use_urc", &ip_modem_use_urc, "Periodically check for Unsolicted Result Codes (URC)");
  registerBoolValue("ip_modem_autoprobe", &ip_modem_autoprobe, "Probe for modem at startup");
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
  LEAF_ENTER(L_NOTICE);
  ipModemPowerOff(HERE);
}

void ipModemProbeTimerCallback(AbstractIpModemLeaf *leaf) { leaf->ipModemSetProbeDue(); }

void AbstractIpModemLeaf::ipModemScheduleProbe(int interval) 
{
  LEAF_ENTER(L_NOTICE);
  if (interval == -1) interval = ip_modem_probe_interval_sec;
  
  if (interval==0) {
    LEAF_NOTICE("Immediate modem re-probe");
    ipModemSetProbeDue();
  }
  else {
    LEAF_NOTICE("Scheduling modem re-probe in %ds", ip_modem_probe_interval_sec);
    ip_modem_probe_timer.once(interval,
			      &ipModemProbeTimerCallback,
			      this);
  }
  LEAF_LEAVE;
}

bool AbstractIpModemLeaf::shouldConnect() 
{
  return canRun() && modemIsPresent() && isAutoConnect() && !isConnected(HERE);
}

bool AbstractIpModemLeaf::ipConnect(String reason) 
{
  if (!AbstractIpLeaf::ipConnect(reason)) {
    // Superclass says no
    return false;
  }
  LEAF_ENTER_STR(L_NOTICE, reason);
  bool present = modemIsPresent();
  
  if (ip_modem_probe_at_connect) {
    LEAF_NOTICE("Probing modem due to probe_at_connect");
    present = modemProbe(HERE,MODEM_PROBE_QUICK);
  } else if (!present) {
    LEAF_NOTICE("Probing modem due to previous non-detection");
    present = modemProbe(HERE,MODEM_PROBE_QUICK);
  }

  if (!present) {
    LEAF_WARN("Cannot connect: Modem is not present");
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

  if (ip_modem_autoprobe && ip_modem_probe_due) {
    ip_modem_probe_due = false;
    LEAF_NOTICE("Attempting to auto-probe IP modem");
    modemProbe(HERE);
    if (!modemIsPresent()) {
      ipModemScheduleProbe();
    }
  }
  
  if (ipModemNeedsReboot()) {
    LEAF_ALERT("Attempting to reboot modem");
    ip_modem_needs_reboot = false;
    if (modemProbe(HERE,MODEM_PROBE_QUICK)) {
      ipModemReboot(HERE);
      ip_modem_connect_attempt_count = 0;      
      ip_reconnect_due = false;
      ipReconnectTimer.once(ip_modem_reboot_wait_sec,
			    &ipReconnectTimerCallback,
			    (AbstractIpLeaf *)this);
    }
    else {
      LEAF_ALERT("Modem not ready for reboot");
      comms_state(WAIT_MODEM, HERE);
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


bool AbstractIpModemLeaf::mqtt_receive(String type, String name, String topic, String payload, bool direct)
{
  LEAF_ENTER(L_DEBUG);
  bool handled = false;
  LEAF_INFO("AbstractIpModem mqtt_receive %s %s => %s [%s]", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());
  WHEN("set/modem_sleep",{
      modemSetSleep(parsePayloadBool(payload));
    })
    ELSEWHEN("cmd/modem_key",{
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
    ELSEWHEN("set/ip_modem_chat_trace_level",{
	modem_chat_trace_level = payload.toInt();
      })
    ELSEWHEN("set/ip_modem_mutex_trace_level",{
	modem_mutex_trace_level = payload.toInt();
      })
    ELSEWHEN("set/ip_modem_trace",{
	ip_modem_trace = (bool)payload.toInt();
      })
    ELSEWHEN("get/modem_reboot_count",{
	mqtt_publish("status/modem_reboot_count", String(ip_modem_reboot_count));
	mqtt_publish("status/modem_uptime_sec", String((int)((millis()-ip_modem_last_reboot)/1000)));
      })
    ELSEWHEN("cmd/modem_test",{
	modemChat(&Serial, parsePayloadBool(payload,true));
      })
    ELSEWHEN("cmd/modem_reboot",{
	ipModemReboot(HERE);
      })
    ELSEWHEN("cmd/modem_probe",{
	if (payload == "no_presence") {
	  LEAF_WARN("Doing modem probe without updating presence");
	  modemProbe(HERE, false, true);
	}
	else {
	  modemProbe(HERE,(payload=="quick"));
	  mqtt_publish("status/modem", TRUTH_lc(modemIsPresent()));
	}
      })
    ELSEWHEN("cmd/modem_off",{
	ipModemPowerOff(HERE);
	modemSetPresent(false);
      })
    ELSEWHEN("cmd/modem_status",{
	mqtt_publish("status/modem", TRUTH_lc(modemIsPresent()));
      })
    ELSEWHEN("cmd/at",{
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
    handled = AbstractIpLeaf::mqtt_receive(type, name, topic, payload, direct);
  }
  
  LEAF_BOOL_RETURN(handled);
}




// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
