#pragma once
#include "abstract_ip_simcom.h"

//
//@*********************** class IpSimcomSim7000 *************************
//
// This class encapsulates a simcom nbiot/catm1 modem
//

class IpSimcomSim7000Leaf : public AbstractIpSimcomLeaf
{
public:
  IpSimcomSim7000Leaf(
    String name,
    String target,
    int uart,
    int rxpin,
    int txpin,
    int baud=115200,
    uint32_t options=SERIAL_8N1,
    int8_t pwrpin=MODEM_PWR_PIN_NONE,
    int8_t keypin=MODEM_KEY_PIN_NONE,
    int8_t sleeppin=MODEM_SLP_PIN_NONE,
    bool run = LEAF_RUN,
    bool autoprobe=true,
    bool invert_key=false
    )
    : Debuggable(name)
    , AbstractIpSimcomLeaf(name,target,uart,rxpin,txpin,baud,options,pwrpin,keypin,sleeppin,run,autoprobe)

  {
    this->invert_key = invert_key; // true=pulse LOW to power on, false=pulse HIGH
    duration_key_on=250;
  }

  virtual bool ipSetApName(String apn) {
    bool result = true;
    if (!modemSendCmd(HERE, "AT+CGDCONT=1,\"IP\",\"%s\"", apn.c_str())) {
      result = false;
    }
    if (!modemSendCmd(HERE, "AT+CSTT=\"%s\"", apn.c_str())) {
      result = false;
    }
    return result;
  }

  bool ipTestLink() 
  {
    LEAF_ENTER(L_INFO);
    // Don't trust this shitty modem.  Make a DNS query to ensure it's REALLY up
    String dnsresult = ipDnsQuery("www.google.com", 10000);
    if (dnsresult.startsWith("DNSFAIL")) {
      LEAF_WARN("DNS is not working, modem is probably confused about being online");
      ipModemSetNeedsReboot();
      LEAF_BOOL_RETURN(false);
    }
    LEAF_BOOL_RETURN(true);
  }

  
  virtual bool ipGetAddress(bool link_test=true) {
    String response = modemQuery("AT+CNACT?","+CNACT: ", 10*modem_timeout_default);
    if (response && response.startsWith("1,")) {
      ip_addr_str = response.substring(3,response.length()-1);
      if (ip_modem_test_after_connect && link_test && !ipTestLink()) {
	return false;
      }
      return true;
    }
    return false;
  }

  virtual bool ipLinkUp() {
    LEAF_ENTER(L_NOTICE);
    char result_buf[256];
    modemSendExpect("AT+CNACT=1", "", NULL, 0, -1, 1, HERE);
    int result = modemGetReply(result_buf, sizeof(result_buf), 4000, 4, 0, HERE);
    if (result && (strstr(result_buf,"+APP PDP: ACTIVE") || strstr(result_buf, "OK"))) {
      modemSendCmd("AT+CIICR");
      if (ipGetAddress(false)) {
	if (ip_modem_test_after_connect && !ipTestLink()) {
	  LEAF_BOOL_RETURN_SLOW(5000, false);
	}
	LEAF_BOOL_RETURN_SLOW(5000, true);
      }
    }
    LEAF_BOOL_RETURN_SLOW(5000, false);
  }

  virtual bool ipLinkDown() {
    LEAF_ENTER(L_NOTICE);
    String result = modemQuery("AT+CNACT=0","",2000);
    if ((result == "+APP PDP: DEACTIVE") || (result=="OK")) {
      LEAF_BOOL_RETURN(true);
    }
    else {
      LEAF_WARN("Unexpected disconnect result: %s", result.c_str());
    }
    LEAF_BOOL_RETURN(false);
  }

  virtual bool ipPing(String host) 
  {
    LEAF_ENTER_STR(L_NOTICE, host);
    modemSendCmd(HERE, "AT+CIPPING=\"%s\"", host.c_str());
    LEAF_BOOL_RETURN(true);
  }

  virtual String ipDnsQuery(String host, int timeout=-1) {
    LEAF_ENTER_STR(L_INFO, host);
    char dns_cmd[80];
    char dns_buf[128];
    snprintf(dns_cmd, sizeof(dns_cmd), "AT+CDNSGIP=\"%s\"", host.c_str());
    modemSendExpect(dns_cmd, "+CDNSGIP: ", dns_buf, sizeof(dns_buf), timeout, 4, HERE);
    String result;
    if (strstr(dns_buf,"1,")==dns_buf) {
      LEAF_STR_RETURN(String(dns_buf+2));
    }
    LEAF_STR_RETURN(String("DNSFAIL ")+String(dns_buf));
  }


  bool modemProcessURC(String Message) 
  {
    if (!canRun()) {
      return(false);
    }
    LEAF_ENTER(L_INFO);
    LEAF_NOTICE("modemProcessURC: [%s]", Message.c_str());
    bool result = false;

    if (Message == "+APP PDP: ACTIVE") {
      if (ipGetAddress()) {
	LEAF_ALERT("IP came back online");
	ip_connected = true;
	result = true;
      }
    }
    else if (Message == "+APP PDP: DEACTIVE") {
      LEAF_ALERT("IP link lost");
      ipOnDisconnect();
      result = true;
    }
    else if (Message.startsWith("+CIPPING")) {
      if (Message.endsWith(",60000,255")) {
	LEAF_ALERT("PING TIMEOUT: %s", Message.c_str());
      }
      else {
	LEAF_WARN("PING RESPONSE: %s", Message.c_str());
      }
    }
    else {
      result = AbstractIpSimcomLeaf::modemProcessURC(Message);
    }
  
    LEAF_BOOL_RETURN(result);
  }

};



// Local Variables:
// mode: C++
// c-basic-offset: 2
// End:
