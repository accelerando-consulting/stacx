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
  AbstractIpModemLeaf(String name, String target, int8_t uart,int rx, int tx, int baud=115200, uint32_t options=SERIAL_8N1,int8_t pwrpin=MODEM_PWR_PIN_NONE, int8_t keypin=MODEM_KEY_PIN_NONE, int8_t sleeppin=MODEM_SLP_PIN_NONE, bool run=LEAF_RUN, bool autoprobe=true) :
    AbstractIpLeaf(name, target, LEAF_PIN(rx)|LEAF_PIN(tx)|LEAF_PIN(pwrpin)|LEAF_PIN(keypin)|LEAF_PIN(sleeppin)),
    TraitModem(uart, rx, tx, baud, options, pwrpin, keypin, sleeppin)
  {
    this->run = run;
    this->ipModemSetAutoprobe(autoprobe);
  }

  virtual void setup(void);
  virtual void start(void);
  virtual void loop(void);
  virtual const char *get_name_str() { return leaf_name.c_str(); }

  virtual void readFile(const char *filename, char *buf, int buf_size, int partition=-1,int timeout=-1) {}
  virtual void writeFile(const char *filename, const char *contents, int size=-1, int partition=-1,int timeout=-1){}
  virtual bool writeFileVerify(const char *filename, const char *contents, int size=-1, int partition=-1,int timeout=-1) {return false;}

  virtual bool mqtt_receive(String type, String name, String topic, String payload);
  virtual bool ipConnect(String reason);
  virtual void ipModemSetNeedsReboot() {
    LEAF_WARN("modem reboot requested");
    ip_modem_needs_reboot = true;
  }
  virtual void ipModemSetAutoprobe(bool s) { ip_modem_autoprobe = s; }
  virtual bool ipModemNeedsReboot() { return ip_modem_needs_reboot; }
  virtual void ipModemRecordReboot() 
  {
    ++ip_modem_reboot_count;
    setIntPref("ip_modem_reboots", ip_modem_reboot_count);
    ip_modem_last_reboot = millis();
  }
  virtual int ipModemGetRebootCount() { return ip_modem_reboot_count; }
  virtual void ipOnConnect() 
  {
    AbstractIpLeaf::ipOnConnect();
    ip_modem_connect_attempt_count = 0;
  }
  virtual void ipScheduleReconnect() 
  {
    AbstractIpLeaf::ipScheduleReconnect();
    if ((ip_modem_connectfail_threshold > 0) &&
	(ip_modem_connect_attempt_count >= ip_modem_connectfail_threshold)) {
      LEAF_WARN("Consecutive failed connection attempts exceeds threshold (%d).  Reinitialise modem",
		ip_modem_connect_attempt_count);
      ipModemSetNeedsReboot();
    }
  }
protected:
  virtual bool shouldConnect();

  int ip_modem_max_file_size = 10240;
  bool ip_modem_use_sleep = true;
  bool ip_modem_use_poweroff = false;
  bool ip_modem_autoprobe = true;
  bool ip_modem_probe_at_connect = false;
  bool ip_modem_needs_reboot = false;
  bool ip_modem_trace = false;
  int ip_modem_connect_attempt_count = 0;
  int ip_modem_reboot_count = 0;
  unsigned long ip_modem_last_reboot = 0;
  int ip_modem_connectfail_threshold = 3;
  

};

void AbstractIpModemLeaf::setup(void) {
  AbstractIpLeaf::setup();
  LEAF_ENTER(L_NOTICE); 
  if (canRun()) {
    modemSetup();
  }
 
  getBoolPref("ip_modem_enable", &run, "Enable the IP modem module");
  getBoolPref("ip_modem_trace", &ip_modem_trace, "print trace of modem exchanges");
  modemSetTrace(ip_modem_trace);
  getBoolPref("ip_modem_use_sleep", &ip_modem_use_sleep, "Put modem to sleep if possible");
  getBoolPref("ip_modem_use_poweroff", &ip_modem_use_poweroff, "Turn off modem power when possible");
  getBoolPref("ip_modem_autoprobe", &ip_modem_autoprobe, "Probe for modem at startup");
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
  LEAF_ENTER_STR(L_INFO, reason);
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
      LEAF_NOTICE("Connection attempt %d (modem reinit threshold is %d)", ip_modem_connect_attempt_count, ip_modem_connectfail_threshold);
    }
  }
  LEAF_BOOL_RETURN(present);
}

void AbstractIpModemLeaf::loop(void) 
{
  static bool first = true;
  
  AbstractIpLeaf::loop();

  if (ipModemNeedsReboot()) {
    LEAF_ALERT("Attempting to reboot modem");
    if (modemProbe(HERE,MODEM_PROBE_QUICK)) {
      ACTION("MODEM reboot");
      modemSendCmd(HERE, "AT+CFUN=1,1");
      ip_modem_connect_attempt_count = 0;      
      ip_modem_needs_reboot=false;
    }
    else {
      LEAF_ALERT("Modem not ready for reboot");
    }
  }
    
  if (first && shouldConnect()) {
    ipConnect("autoconnect");
    first = false;
  }

  if (canRun() && modemIsPresent()) {
    modemCheckURC();
  }
}

bool AbstractIpModemLeaf::mqtt_receive(String type, String name, String topic, String payload)
{
  LEAF_ENTER(L_DEBUG);
  bool handled = false;
  LEAF_INFO("AbstractIpModem mqtt_receive %s %s => %s [%s]", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());
  WHEN("set/modem_sleep",{
      modemSetSleep(parsePayloadBool(payload));
    })
    ELSEWHEN("set/modem_key",{
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
    ELSEWHEN("cmd/modem_probe",{
	modemProbe(HERE,(payload=="quick"));
	mqtt_publish("status/modem", TRUTH_lc(modemIsPresent()));
      })
    ELSEWHEN("cmd/modem_off",{
	modemSendCmd(HERE, "AT+CPOWD=1");
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
