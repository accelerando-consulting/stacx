#pragma once
#include "abstract_ip_simcom.h"

#include "ip_client_sim7080.h"


//
//@*********************** class IpSimcomM2MLeaf *************************
//
// This class encapsulates a simcom nbiot/catm1 modem
//

class IpSimcomSim7080Leaf : public AbstractIpSimcomLeaf
{
public:
  IpSimcomSim7080Leaf(String name, String target, int uart,int rxpin, int txpin,  int baud=115200, uint32_t options=SERIAL_8N1, int8_t pwrpin=MODEM_PWR_PIN_NONE, int8_t keypin=MODEM_KEY_PIN_NONE, int8_t sleeppin=MODEM_SLP_PIN_NONE, bool run = LEAF_RUN,bool autoprobe=true,bool invert_key=true)
    : TraitDebuggable(name)
    , AbstractIpSimcomLeaf(name,target,uart,rxpin,txpin,baud,options,pwrpin,keypin,sleeppin,run,autoprobe)
  {
    //ip_modem_probe_at_connect = true;
    //ip_modem_probe_at_sms = true;
    //ip_modem_probe_at_gps = true;
    ip_modem_reuse_connection=false;
    ip_simultaneous_gps = false; // the sim7080 has only single channel radio
    this->invert_key = invert_key; // true=pulse LOW to power on, false=pulse HIGH
  }

  virtual void setup() 
  {
    AbstractIpSimcomLeaf::setup();
  }

  virtual void start()
  {
    AbstractIpSimcomLeaf::start();
    started=true;
  }

  virtual bool ipSetApName(String apn) { return modemSendCmd(HERE, "AT+CGDCONT=1,\"IP\",\"%s\"", apn.c_str()); }

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
    if (response && response.startsWith("0,1,")) {
      ip_addr_str = response.substring(5,response.length()-1);
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
    modemSendExpect("AT+CNACT=0,1", "", NULL, 0, -1, 1, HERE);
    int result = modemGetReply(result_buf, sizeof(result_buf), 2000, 4, 0, HERE);
    if (result && (strstr(result_buf,"+APP PDP: 0,ACTIVE") || strstr(result_buf, "OK"))) {
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
    String result = modemQuery("AT+CNACT=0,0");
    if ((result == "+APP PDP: 0,DEACTIVE") || (result=="OK")) {
      LEAF_BOOL_RETURN(true);
    }
    LEAF_BOOL_RETURN(false);
  }

  virtual bool ipPing(String host) 
  {
    LEAF_ENTER_STR(L_NOTICE, host);
    modemSendCmd(HERE, "AT+SNPING4=\"%s\",10,64,1000", host.c_str());
#if 0
    // commented out because URC handler processes replies
    unsigned long until = millis()+10000;
    do {
      int count = modemGetReply(modem_response_buf, modem_response_max, 250, 2, 0, HERE);
      if (count) {
	LEAF_NOTICE("ECHO REPLY %s", modem_response_buf);
      }
    } while (millis() < until);
#endif
    LEAF_BOOL_RETURN(true);
  }
  virtual String ipDnsQuery(String host, int timeout=-1) {
    LEAF_ENTER_STR(L_NOTICE, host);
    char dns_cmd[80];
    char dns_buf[128];
    snprintf(dns_cmd, sizeof(dns_cmd), "AT+CDNSGIP=\"%s\",2,2000", host.c_str());
    modemSendExpect(dns_cmd, "+CDNSGIP: ", dns_buf, sizeof(dns_buf), timeout, 4, HERE);
    String result;
    if (strstr(dns_buf,"1,")==dns_buf) {
      LEAF_STR_RETURN(String(dns_buf+2));
    }
    LEAF_STR_RETURN(String("DNSFAIL ")+String(dns_buf));
  }

  virtual bool modemProcessURC(String Message);
  virtual bool modemBearerBegin(int bearer) 
  {
    return true;
  }
  virtual bool modemBearerEnd(int bearer) 
  {
    return true;
  }
  virtual bool ftpPut(String host, String user, String pass, String path, const char *buf, int buf_len)
  {
    LEAF_ENTER(L_NOTICE);
    LEAF_NOTICE("Uploading %s of size %d", path.c_str(), buf_len);
    bool result = modemFtpPut(host.c_str(), user.c_str(), pass.c_str(), path.c_str(), buf, buf_len, /*bearer*/0);
    LEAF_RETURN(result);
  }
  virtual bool modemFtpEnd(int bearer = 1) 
  {
    LEAF_ENTER(L_NOTICE);
    bool result = true;
    if (!modemSendExpect("AT+FTPQUIT","+FTPPUT: 1,80", NULL, 0, 5000, 4, HERE)) {
      LEAF_NOTICE("Quit command rejected");
      result = false;
    }
    if (!modemBearerEnd(bearer)) {
      LEAF_NOTICE("Bearer termination failed");
      result = false;
    }
    LEAF_BOOL_RETURN(result);
  }
  
  virtual IpClientSim7080 *newClient(int port) 
  {
    return new IpClientSim7080(this, port);
  }
  


protected:

};

bool IpSimcomSim7080Leaf::modemProcessURC(String Message) 
{
  if (!canRun()) {
    return(false);
  }
  LEAF_ENTER(L_INFO);
  LEAF_NOTICE("modemProcessURC: [%s]", Message.c_str());
  bool result = false;

  if (Message == "+APP PDP: 0,ACTIVE") {
    if (ipGetAddress()) {
      LEAF_ALERT("IP came back online");
      ip_connected = true;
      result = true;
    }
  }
  else if (Message == "+APP PDP: 0,DEACTIVE") {
    LEAF_ALERT("IP link lost");
    ipOnDisconnect();
    result = true;
  }
  else if (Message.startsWith("+CADATAIND: ")) {
    int pos = Message.indexOf(" ");
    int slot = Message.substring(pos+1).toInt();
    if (ip_clients[slot]==NULL) {
      LEAF_ALERT("Data received for invalid connection slot %d", slot);
      result=true;
    }
    else {
      LEAF_NOTICE("Data received connection slot %d", slot);
      // tell the socket it has data (we don't know how much)
      ((IpClientSim7080 *)ip_clients[slot])->dataIndication(0);
    }
  }
  else if (Message.startsWith("+CASTATE: ")) {
    int pos = Message.indexOf(" ");
    int slot=-1;
    int state=-1;

    if (pos >= 0) {
      slot = Message.substring(pos+1).toInt();
      pos = Message.indexOf(",");
      if (pos >= 0) {
	state = Message.substring(pos+1).toInt();
      }
    }
    if ((slot < 0) || (state < 0)) {
      LEAF_ALERT("Failed to understand CASTATE message [%s]", Message.c_str());
    }
    else {
      if (state == 0) {
	if (ip_clients[slot]==NULL) {
	  LEAF_ALERT("Data received for invalid connection slot %d", slot);
	  result=true;
	}
	else {
	  LEAF_WARN("Received socket disconnect alert for slot %d", slot);
	  // tell the socket it has data (we don't know how much)
	  ((IpClientSim7080 *)ip_clients[slot])->disconnectIndication();
	  result=true;
	}
      }
    }
  }
  else {
    result = AbstractIpSimcomLeaf::modemProcessURC(Message);
  }
  LEAF_BOOL_RETURN(result);
}

// Local Variables:
// mode: C++
// c-basic-offset: 2
// End:
