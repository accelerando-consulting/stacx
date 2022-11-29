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
    , TraitModem(uart, rx, tx, baud, options, pwrpin, keypin, sleeppin)
    , TraitDebuggable(name)
  {
    this->run = run;
    this->ipModemSetAutoprobe(autoprobe);
  }

  virtual void setup(void);
  virtual void start(void);
  virtual void stop(void);
  virtual void loop(void);

  virtual void readFile(const char *filename, char *buf, int buf_size, int partition=-1,int timeout=-1) {}
  virtual void writeFile(const char *filename, const char *contents, int size=-1, int partition=-1,int timeout=-1){}
  virtual bool writeFileVerify(const char *filename, const char *contents, int size=-1, int partition=-1,int timeout=-1) {return false;}

  virtual void mqtt_do_subscribe(void);
  virtual bool mqtt_receive(String type, String name, String topic, String payload);
  virtual bool ipConnect(String reason);
  virtual void ipModemSetNeedsReboot() {
    LEAF_WARN("modem reboot requested");
    ip_modem_needs_reboot = true;
  }
  virtual void ipModemSetAutoprobe(bool s) { ip_modem_autoprobe = s; }
  void ipModemSetProbeDue() {ip_modem_probe_due=true;}
  virtual void ipModemScheduleProbe();

  virtual bool ipModemNeedsReboot() { return ip_modem_needs_reboot; }
  virtual void ipModemRecordReboot() 
  {
    ++ip_modem_reboot_count;
    setIntPref("ip_modem_reboots", ip_modem_reboot_count);
    ip_modem_last_reboot = millis();
  }
  virtual int ipModemGetRebootCount() { return ip_modem_reboot_count; }
  virtual int ipModemReboot(codepoint_t where=undisclosed_location) {
    LEAF_ENTER(L_INFO);
    
    ip_modem_last_reboot_cmd = millis();

    if (modemSendCmd(where, "AT+CFUN=1,1")) {
      ACTION("MODEM reboot");
      // modem responded to a software reboot request
      LEAF_BOOL_RETURN(true);
    }

    // Modem not responding to software reboot, use the hardware
    if (pin_power >= 0) {
      // We have direct power control, use that
      ACTION("MODEM poweroff");
      modemSetPower(false);
      delay(500);
      modemSetPower(true);
      ACTION("MODEM poweron");
      LEAF_BOOL_RETURN(true);
    }
    else if (pin_key >= 0) {
      ACTION("MODEM poweroff");
      modemPulseKey(false);
      delay(1000);
      modemPulseKey(true);
      ACTION("MODEM poweron");
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
    modemSendCmd(HERE, "AT+CPOWD=1");
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

};

void AbstractIpModemLeaf::setup(void) {
  AbstractIpLeaf::setup();
  LEAF_ENTER(L_NOTICE); 
  if (canRun()) {
    modemSetup();
  }
 
  getBoolPref("ip_modem_enable", &run, "Enable the IP modem module");
  getBoolPref("ip_modem_trace", &ip_modem_trace, "print trace of modem exchanges");

#ifdef ESP32
  getBoolPref("ip_modem_own_loop", &own_loop, "Use a separate thread for modem connection management");
#endif
  
  modemSetTrace(ip_modem_trace);
  getBoolPref("ip_modem_use_sleep", &ip_modem_use_sleep, "Put modem to sleep if possible");
  getBoolPref("ip_modem_use_poweroff", &ip_modem_use_poweroff, "Turn off modem power when possible");
  getBoolPref("ip_modem_autoprobe", &ip_modem_autoprobe, "Probe for modem at startup");
  getBoolPref("ip_modem_probe_at_connect", &ip_modem_probe_at_connect, "Confirm the modem is answering before attempting to connect");
  getBoolPref("ip_modem_test_after_connect", &ip_modem_test_after_connect, "Make a DNS query after connect to confirm that IP is really working");
  getBoolPref("ip_modem_reuse_connection", &ip_modem_reuse_connection, "If modem is already connected, reuse existing connection");
  getIntPref("ip_modem_reboot_wait_sec", &ip_modem_reboot_wait_sec, "Time in seconds to wait for modem reboot");
  getIntPref("ip_modem_max_file_size", &ip_modem_max_file_size, "Maximum file size for transfers");
  getIntPref("ip_modem_chat_trace_level", &modem_chat_trace_level, "Log level for modem chat trace");
  getIntPref("ip_modem_mutex_trace_level", &modem_mutex_trace_level, "Log level for modem mutex trace");
  getIntPref("ip_modem_reboots", &ip_modem_reboot_count, "Number of unexpected modem reboots");
  getIntPref("ip_modem_connectfail_threshold", &ip_modem_connectfail_threshold, "Reboot modem after N failed connect attempts");
  
  LEAF_LEAVE_SLOW(2000);
}

void AbstractIpModemLeaf::start(void) 
{
  LEAF_ENTER(L_NOTICE);
  AbstractIpLeaf::start();

  if (!modemIsPresent() && ip_modem_autoprobe) {
    modemProbe(HERE);
    if (!modemIsPresent()) {
      ipModemScheduleProbe();
    }
  }
  LEAF_LEAVE;
}

void AbstractIpModemLeaf::stop(void) 
{
  LEAF_ENTER(L_NOTICE);

  if (modemIsPresent()) {
    if (isConnected()) {
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

void ipModemProbeTimerCallback(AbstractIpModemLeaf *leaf) { leaf->ipModemSetProbeDue(); }

void AbstractIpModemLeaf::ipModemScheduleProbe() 
{
  LEAF_ENTER(L_NOTICE);
  if (ip_modem_probe_interval_sec == 0) {
    ipModemSetProbeDue();
  }
  else {
    LEAF_NOTICE("Will attempt modem probe in %ds", ip_modem_probe_interval_sec);
    ip_modem_probe_timer.once(ip_modem_probe_interval_sec,
			      &ipModemProbeTimerCallback,
			      this);
  }
  LEAF_LEAVE;
}

bool AbstractIpModemLeaf::shouldConnect() 
{
  return canRun() && modemIsPresent() && !isConnected() && isAutoConnect();
}

bool AbstractIpModemLeaf::ipConnect(String reason) 
{
  if (!AbstractIpLeaf::ipConnect(reason)) {
    // Superclass said no can do
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

void AbstractIpModemLeaf::loop(void) 
{
  static bool first = true;

  if (ip_modem_probe_due) {
    ip_modem_probe_due = false;
    LEAF_NOTICE("Attempting to auto-probe IP modem");
    modemProbe(HERE);
    if (!modemIsPresent()) {
      ipModemScheduleProbe();
    }
  }
  
  if (ipModemNeedsReboot()) {
    LEAF_ALERT("Attempting to reboot modem");
    if (modemProbe(HERE,MODEM_PROBE_QUICK)) {
      ipModemReboot(HERE);
      ip_modem_connect_attempt_count = 0;      
      ip_modem_needs_reboot=false;
      ip_reconnect_due = false;
      ipReconnectTimer.once(ip_modem_reboot_wait_sec,
			    &ipReconnectTimerCallback,
			    (AbstractIpLeaf *)this);
    }
    else {
      LEAF_ALERT("Modem not ready for reboot");
    }
  }
    
  AbstractIpLeaf::loop();

  if (first && shouldConnect()) {
    ipConnect("autoconnect");
    first = false;
  }

  if (canRun() && modemIsPresent()) {
    modemCheckURC();
  }
}

void AbstractIpModemLeaf::mqtt_do_subscribe(void) 
{
  AbstractIpLeaf::mqtt_do_subscribe();
  register_mqtt_cmd("modem_key", "trigger the modem power key (1=on, 0=off)");
  register_mqtt_cmd("modem_test", "connect the console directly to the modem (console cli only!)");
  register_mqtt_cmd("modem_reboot", "Reboot the modem (modem only, not ESP)");
  register_mqtt_cmd("modem_probe", "Initiate modem presence detection");
  register_mqtt_cmd("modem_off", "Turn the modem off");
  register_mqtt_cmd("modem_status", "Get the modem status");
  register_mqtt_cmd("modem_at", "Send a single AT command and report the response");
}



bool AbstractIpModemLeaf::mqtt_receive(String type, String name, String topic, String payload)
{
  LEAF_ENTER(L_DEBUG);
  bool handled = false;
  LEAF_INFO("AbstractIpModem mqtt_receive %s %s => %s [%s]", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());
  WHEN("set/modem_sleep",{
      modemSetSleep(parsePayloadBool(payload));
    })
    ELSEWHEN("cmd/modem_key",{
	modemPulseKey(parsePayloadBool(payload, true));
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
	modemProbe(HERE,(payload=="quick"));
	mqtt_publish("status/modem", TRUTH_lc(modemIsPresent()));
      })
    ELSEWHEN("cmd/modem_off",{
	ipModemPowerOff(HERE);
      })
    ELSEWHEN("cmd/modem_status",{
	mqtt_publish("status/modem", TRUTH_lc(modemIsPresent()));
      })
    ELSEWHEN("cmd/at",{
	if (!modemWaitBufferMutex()) {
	  LEAF_ALERT("Cannot take modem mutex");
	}
	else {
	  LEAF_INFO("Send AT command %s", payload.c_str());
	  String result = modemQuery(payload,5000);
	  modemReleaseBufferMutex();
	  mqtt_publish("status/at", result);
	}
      })
  else {
    handled = AbstractIpLeaf::mqtt_receive(type, name, topic, payload);
  }
  
  LEAF_BOOL_RETURN(handled);
}




// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
