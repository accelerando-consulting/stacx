#pragma once
#include "abstract_ip.h"
#include "trait_modem.h"
//
//@************************* class AbstractIpModemLeaf ***************************
//
// This class encapsulates a TCP/IP interface via an AT-command modem
//

typedef std::function<bool(int,size_t,const uint8_t *)> IPModemHttpHeaderCallback;
typedef std::function<bool(const uint8_t *,size_t)> IPModemHttpDataCallback;

class AbstractIpModemLeaf : public AbstractIpLeaf, public TraitModem
{
public:
  AbstractIpModemLeaf(String name, String target, int8_t uart,int rx, int tx, int baud=115200, uint32_t options=SERIAL_8N1,int8_t pwrpin=-1,int8_t keypin=-1,int8_t sleeppin=-1,bool run =true) :
    AbstractIpLeaf(name, target, LEAF_PIN(rx)|LEAF_PIN(tx)|LEAF_PIN(pwrpin)|LEAF_PIN(keypin)|LEAF_PIN(sleeppin)),
    TraitModem(uart, rx, tx, baud, options, pwrpin, keypin, sleeppin)
  {}

  virtual void setup(void);
  virtual void start(void);
  virtual void loop(void);
  virtual const char *get_name_str() { return leaf_name.c_str(); }

  virtual void readFile(const char *filename, char *buf, int buf_size, int partition=-1,int timeout=-1) {}
  virtual void writeFile(const char *filename, const char *contents, int size=-1, int partition=-1,int timeout=-1){}
  virtual bool writeFileVerify(const char *filename, const char *contents, int size=-1, int partition=-1,int timeout=-1) {return false;}

  virtual bool mqtt_receive(String type, String name, String topic, String payload);
  virtual bool ipConnect(String reason);

protected:
  virtual bool shouldConnect();

  int ip_modem_max_file_size = 10240;
  bool ip_modem_use_sleep = true;
  bool ip_modem_autoprobe = true;
  bool ip_modem_probe_at_connect = false;
  
};

void AbstractIpModemLeaf::setup(void) {
  LEAF_ENTER(L_NOTICE);
  AbstractIpLeaf::setup();
  if (canRun()) {
    modemSetup();
  }
  
  getBoolPref("ip_modem_enable", &run, "Enable the IP modmem module");
  getBoolPref("ip_modem_use_sleep", &ip_modem_use_sleep, "Put modem to sleep if possible");
  getBoolPref("ip_modem_autoprobe", &ip_modem_autoprobe, "Probe for modem at startup");
  getIntPref("ip_modem_max_file_size", &ip_modem_max_file_size, "Maximum file size for transfers");
  
  if (canRun() && ip_modem_autoprobe) {
    modemProbe();
  }
  LEAF_LEAVE;
}

void AbstractIpModemLeaf::start(void) 
{
  LEAF_ENTER(L_NOTICE);
  AbstractIpLeaf::start();
  if (!modemIsPresent() && ip_modem_autoprobe) modemProbe();
  LEAF_LEAVE;
}

bool AbstractIpModemLeaf::shouldConnect() 
{
  return canRun() && modemIsPresent() && !isConnected() && isAutoConnect();
}

bool AbstractIpModemLeaf::ipConnect(String reason) 
{
  LEAF_ENTER(L_INFO);
  
  if (ip_modem_probe_at_connect || !modemIsPresent()) modemProbe();
  LEAF_BOOL_RETURN(modemIsPresent());
}

void AbstractIpModemLeaf::loop(void) 
{
  static bool first = true;
  
  AbstractIpLeaf::loop();
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
	modemSetKey(parsePayloadBool(payload, true));
      })
    ELSEWHEN("cmd/modem_test",{
	modemChat(&Serial, parsePayloadBool(payload,true));
      })
    ELSEWHEN("cmd/modem_probe",{
	modemProbe();
	mqtt_publish("status/modem", truth(modemIsPresent()));
      })
    ELSEWHEN("cmd/modem_status",{
	mqtt_publish("status/modem", truth(modemIsPresent()));
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
