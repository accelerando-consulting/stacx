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

protected:
  virtual bool shouldConnect();

  int ip_modem_max_file_size = 10240;
  bool ip_modem_use_sleep = true;
  bool ip_modem_auto_probe = true;
  
};

void AbstractIpModemLeaf::setup(void) {
  LEAF_ENTER(L_NOTICE);
  AbstractIpLeaf::setup();
  modemSetup();
  
  run = getBoolPref("mdm_enable", run);
  ip_modem_use_sleep = getBoolPref("mdm_use_slp", ip_modem_use_sleep);
  ip_modem_auto_probe = getBoolPref("mdm_autoprobe", ip_modem_auto_probe);
  ip_modem_max_file_size = getIntPref("mdm_maxfile", ip_modem_max_file_size);
  
  if (ip_modem_auto_probe) {
    modem_present = modemProbe();
  }
  LEAF_LEAVE;
}

void AbstractIpModemLeaf::start(void) 
{
  LEAF_ENTER(L_NOTICE);
  AbstractIpLeaf::start();
  modem_present |= modemProbe();
  LEAF_LEAVE;
}

bool AbstractIpModemLeaf::shouldConnect() 
{
  return canRun() && modemPresent() && !isConnected() && isAutoConnect();
}

void AbstractIpModemLeaf::loop(void) 
{
  static bool first = true;
  
  AbstractIpLeaf::loop();
  if (first && shouldConnect()) {
    ipConnect("autoconnect");
    first = false;
  }

  if (canRun() && modemPresent()) {
    modemCheckURC();
  }
}

bool AbstractIpModemLeaf::mqtt_receive(String type, String name, String topic, String payload)
{
  LEAF_ENTER(L_INFO);
  bool handled = false;
  LEAF_INFO("AbstractIpModem mqtt_receive %s %s => %s", type.c_str(), name.c_str(), topic.c_str());
  WHEN("set/modem_sleep",{
      modemSetSleep(parsePayloadBool(payload));
    })
    ELSEWHEN("set/modem_key",{
	modemSetKey(parsePayloadBool(payload));
      })
    ELSEWHEN("cmd/modem_test",{
	modemChat(&Serial, parsePayloadBool(payload));
      })
    ELSEWHEN("cmd/at",{
	char sendbuffer[80];
	char replybuffer[80];
	if (!modemWaitBufferMutex()) {
	  LEAF_ALERT("Cannot take modem mutex");
	}
	else {
	  strlcpy(modem_command_buf, payload.c_str(), sizeof(modem_command_buf));
	  LEAF_DEBUG("Send AT command %s", modem_command_buf);
	  String result = modemQuery(modem_command_buf, "",5000);
	  modemReleaseBufferMutex();
	  mqtt_publish("status/at", result);
	}
      })
  else {
    handled = AbstractIpLeaf::mqtt_receive(type, name, topic, payload);
  }
  
  LEAF_RETURN(handled);
}




// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
