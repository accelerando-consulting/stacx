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
  IpSimcomSim7080Leaf(String name, String target, int uart,int rxpin, int txpin,  int baud=115200, uint32_t options=SERIAL_8N1, int8_t pwrpin=-1, int8_t keypin=-1, int8_t sleeppin=-1, bool run = true) : AbstractIpSimcomLeaf(name,target,uart,rxpin,txpin,baud,options,pwrpin,keypin,sleeppin,run) {
    ip_modem_probe_at_connect = true;
    ip_modem_probe_at_sms = true;
    ip_modem_probe_at_gps = true;
  }

  virtual bool ipSetApName(String apn) { return modemSendCmd("AT+CGDCONT=1,\"IP\",\"%s\"", apn.c_str()); }
  virtual bool ipGetAddress() {
    String response = modemQuery("AT+CNACT?","+CNACT: ", 10*modem_timeout_default);
    if (response && response.startsWith("0,1,")) {
      strlcpy(ip_addr_str, response.substring(5,response.length()-1).c_str(), sizeof(ip_addr_str));
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

  virtual bool ipPing(String host) 
  {
    modemSendCmd("AT+SNPING4,%s,10,64,1000");
    return true;
  }
  virtual bool modemProcessURC(String Message);


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
