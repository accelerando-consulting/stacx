#pragma once
#include "abstract_ip_simcom.h"

//
//@*********************** class IpSimcomM2MLeaf *************************
//
// This class encapsulates a simcom nbiot/catm1 modem
//

class IpSimcomSim7080Leaf : public AbstractIpSimcomLeaf
{
public:
  IpSimcomSim7080Leaf(String name, String target, int uart,int rxpin, int txpin,  int baud=115200, uint32_t options=SERIAL_8N1, int8_t pwrpin=MODEM_PWR_PIN_NONE, int8_t keypin=MODEM_KEY_PIN_NONE, int8_t sleeppin=MODEM_SLP_PIN_NONE, bool run = LEAF_RUN,bool autoprobe=true) : AbstractIpSimcomLeaf(name,target,uart,rxpin,txpin,baud,options,pwrpin,keypin,sleeppin,run,autoprobe) {
    //ip_modem_probe_at_connect = true;
    ip_modem_probe_at_sms = true;
    ip_modem_probe_at_gps = true;
    invert_key = true; // pulse LOW to power on
  }

  virtual void setup() 
  {
    AbstractIpSimcomLeaf::setup();
  }
    

  virtual bool ipSetApName(String apn) { return modemSendCmd(HERE, "AT+CGDCONT=1,\"IP\",\"%s\"", apn.c_str()); }
  virtual bool ipGetAddress() {
    String response = modemQuery("AT+CNACT?","+CNACT: ", 10*modem_timeout_default);
    if (response && response.startsWith("0,1,")) {
      ip_addr_str = response.substring(5,response.length()-1);
      return true;
    }
    return false;
  }

  virtual bool ipLinkUp() {
    LEAF_ENTER(L_NOTICE);
    String result = modemQuery("AT+CNACT=0,1");
    if ((result == "+APP PDP: 0,ACTIVE") || (result=="OK")) {
      LEAF_BOOL_RETURN(true);
    }
    LEAF_BOOL_RETURN(false);
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
    modemSendCmd(HERE, "AT+SNPING4,%s,10,64,1000");
    return true;
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
  
    
  


protected:

};

bool IpSimcomSim7080Leaf::modemProcessURC(String Message) 
{
  if (!canRun()) {
    return(false);
  }
  LEAF_ENTER(L_NOTICE);
  LEAF_NOTICE("modemProcessURC: [%s]", Message.c_str());
  bool result = false;

  if (Message == "+APP PDP: 0,ACTIVE") {
    if (ipGetAddress()) {
      LEAF_ALERT("IP came back online");
      ip_connected = true;
      result = true;
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
