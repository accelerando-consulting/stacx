#pragma once
#include "abstract_ip_simcom.h"

#include "ip_client_sim7080.h"


//
//@******************** class IpSimcomSim7080Leaf ************************
//
// This class encapsulates a simcom Sim7080 nbiot/catm1 modem
//

class IpSimcomSim7080Leaf : public AbstractIpSimcomLeaf
{
public:
  IpSimcomSim7080Leaf(
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
    : AbstractIpSimcomLeaf(name,target,uart,rxpin,txpin,baud,options,pwrpin,keypin,sleeppin,run,autoprobe)
    , Debuggable(name)
  {
    //ip_modem_probe_at_connect = true;
    //ip_modem_probe_at_sms = true;
    //ip_modem_probe_at_gps = true;
    ip_modem_reuse_connection=false;
    ip_simultaneous_gps = false; // the sim7080 has only single channel radio
    this->invert_key = invert_key; // true=pulse LOW to power on, false=pulse HIGH
    duration_key_on=250;
  }

  virtual void start()
  {
    AbstractIpSimcomLeaf::start();
    LEAF_ENTER(L_NOTICE);
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
    String response = modemQuery("AT+CNACT?","+CNACT: ", 10*modem_timeout_default,HERE);
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
    int result = modemGetReply(result_buf, sizeof(result_buf), 4000, 4, 0, HERE);
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
    String result = modemQuery("AT+CNACT=0,0","",2000,HERE);
    if ((result == "+APP PDP: 0,DEACTIVE") || (result=="OK")) {
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
    LEAF_ENTER_STR(L_INFO, host);
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

  virtual String getSMSText(int msg_index)
  {
    LEAF_ENTER_INT(L_NOTICE, msg_index);
    String result = "";
    
    if (!modemIsPresent()) {
      LEAF_NOTICE("Modem not present");
      LEAF_STR_RETURN(result);
    }

    if (!modemSendCmd(HERE, "AT+CMGF=1")) {
      LEAF_ALERT("SMS format command not accepted");
      LEAF_STR_RETURN(result);
    }
    if (!modemSendCmd(HERE, "AT+CSDH=1")) {
      LEAF_ALERT("SMS header format command not accepted");
      LEAF_STR_RETURN(result);
    }
    
    int sms_len;
    snprintf(modem_command_buf, modem_command_max, "AT+CMGR=%d", msg_index);
    /*
      TA returns SMS message with location value <index> from message storage <mem1> to the TE. If status of the message is 'received unread', status in the storage changes to 'received read'.
1) If text mode (+CMGF=1) and Command successful:
for SMS-DELIVER:
+CMGR: <stat>,<oa>[,<alpha>],<scts>[,<tooa>,<fo>,<pid>,<dcs>,<sca>,<t osca>,<length>]<CR><LF><data>
    */

    modemFlushInput();
    if (!modemSendExpectIntField(modem_command_buf, "+CMGR: ", 12, &sms_len, ',', -1, HERE,false)) {
      LEAF_ALERT("Error requesting message %d", msg_index);
      LEAF_STR_RETURN(result);
    }
    LEAF_NOTICE("SMS message %d length is %d", msg_index, sms_len);
    //LEAF_DEBUG("Remainder of modem_response_buf is [%s]", modem_response_buf);
    if (sms_len >= modem_response_max) {
      LEAF_ALERT("SMS message length (%d) too long", sms_len);
      LEAF_STR_RETURN(result);
    }
    if (!modemGetReplyOfSize(modem_response_buf, sms_len, modem_timeout_default*4, L_DEBUG)) {
      LEAF_ALERT("SMS message read failed");
      LEAF_STR_RETURN(result);
    }
    modem_response_buf[sms_len]='\0';
    LEAF_NOTICE("Got SMS message: %d %s", msg_index, modem_response_buf);
    result = modem_response_buf;
    
    LEAF_STR_RETURN(result);
}

  virtual String getSMSSender(int msg_index)
  {
    LEAF_ENTER_INT(L_INFO, msg_index);
    String result = "";
    
    if (!modemIsPresent()) {
      LEAF_NOTICE("Modem not present");
      LEAF_STR_RETURN(result);
    }
    
    if (!modemSendCmd(HERE, "AT+CMGF=1")) {
      LEAF_ALERT("SMS format command not accepted");
      LEAF_STR_RETURN(result);
    }
    if (!modemSendCmd(HERE, "AT+CSDH=1")) {
      LEAF_ALERT("SMS header format command not accepted");
      LEAF_STR_RETURN(result);
    }

    int sms_len;
    snprintf(modem_command_buf, modem_command_max, "AT+CMGR=%d", msg_index);
    /*
      TA returns SMS message with location value <index> from message storage <mem1> to the TE. If status of the message is 'received unread', status in the storage changes to 'received read'.
1) If text mode (+CMGF=1) and Command successful:
for SMS-DELIVER:
+CMGR: <stat>,<oa>[,<alpha>],<scts>[,<tooa>,<fo>,<pid>,<dcs>,<sca>,<t osca>,<length>]<CR><LF><data>
    */

    modemFlushInput();
    result = modemSendExpectQuotedField(modem_command_buf, "+CMGR: ", 2, ',', -1, HERE);
    if (result == "") {
      LEAF_ALERT("Error inspecting message %d", msg_index);
      LEAF_STR_RETURN(result);
    }
    modemFlushInput(HERE);
    
    LEAF_STR_RETURN(result);
  }

  
  virtual IpClientSim7080 *newClient(int port) 
  {
    return new IpClientSim7080(this, port);
  }

#ifdef NOTYET
  virtual int modemHttpGetWithCallback(
    const char *url,
    IPModemHttpHeaderCallback header_callback,
    IPModemHttpDataCallback chunk_callback,
    int bearer,
    int chunk_size)
  {
    char cmd[255];
    char exp[40];
    int err, state,size,hdr_size;
    size_t file_size =0;
    size_t got_size = 0;
    int chunk;
    const int chunk_size_max = 1024;
    char chunk_buf[chunk_size_max];

    if (chunk_size > chunk_size_max) {
      chunk_size = chunk_size_max;
    }
    
    LEAF_NOTICE("httpGetWithCallback url=%s", url);

    if (!modemSendCmd(HERE, "AT+SHCONF=\"URL\":\"%s\"", url)) {
      LEAF_ALERT("HTTP url rejected");
      return -1;
    }
    
    
    // 
    // The basic HTTP get example from the simcom app note only works for
    // files up to about 40k.   Above this it returns 602 NO MEMORY
    //
    // We work around this by doing partial-range queries for small
    // chunks of the file
    // 

    do {

      if (!modemSendCmd(HERE, "AT+HTTPPARA=BREAK,%lu", (unsigned long)got_size)) {
	LEAF_ALERT("HTTP BREAK set failed");
	modemHttpEnd(bearer);
	return -1;
      }

      if (!modemSendCmd(HERE, "AT+HTTPPARA=BREAKEND,%lu", (unsigned long)got_size+chunk_size-1)) {
	LEAF_ALERT("HTTP BREAKEND set failed");
	modemHttpEnd(bearer);
	return -1;
      }

      // Send a HTTP get.   Because we are using partial range we expect 206,1024
      // i.e. size here is the size of the first chunk only
      if (!modemSendExpectIntPair("AT+HTTPACTION=0", "+HTTPACTION: 0,", &err,&size,75000,2, HERE)) {
	LEAF_ALERT("HTTP partial-get initiate failed");
	modemHttpEnd(bearer);
	return -1;
      }

      if (err != 206) { // partial content
	LEAF_ALERT("HTTP error code %d", err);
	modemHttpEnd(bearer);
	return -1;
      }
	
      if (got_size == 0) {
	// This is the first chunk, get the total size from the HTTP header,
	// and pass it to the callback
	if (!modemSendExpectInt("AT+HTTPHEAD", "+HTTPHEAD: ",&hdr_size, 75000,HERE)) {
	  LEAF_ALERT("HTTP header fetch failed");
	  modemHttpEnd(bearer);
	  return -1;
	}
	if (hdr_size > chunk_size_max) hdr_size=chunk_size_max;
	if (hdr_size > 0) {
	  LEAF_NOTICE("Reading header chunk of %d", hdr_size);
	  if (!modemGetReplyOfSize(chunk_buf, hdr_size, 10000)) {
	    LEAF_ALERT("Chunk Read failed");
	    modemHttpEnd(bearer);
	    return -1;
	  }
	}
	else {
	  chunk_buf[0]='\0';
	}

	char *range_hdr = strstr(chunk_buf, "\r\ncontent-range: bytes ");
	if (!range_hdr) {
	  LEAF_ALERT("Did not find range header");
	  modemHttpEnd(bearer);
	  return -1;
	}
	char *slash = strchr(range_hdr, '/');
	if (!slash) {
	  LEAF_ALERT("Did not find file-size marker in range header");
	  modemHttpEnd(bearer);
	  return -1;
	}
	file_size = strtoul(slash+1, NULL, 10);
	if (file_size == 0) {
	  LEAF_ALERT("Did not find file-size marker in range header");
	  modemHttpEnd(bearer);
	  return -1;
	}

	if (!header_callback(err, file_size, (const uint8_t *)chunk_buf)) {
	  LEAF_ALERT("Header callback indicated abort");
	  modemHttpEnd(bearer);
	  return -1;
	}
      }

      // For first and all subsequent chunks, read the chunk data
      if (!modemSendExpectInt("AT+HTTPREAD", "+HTTPREAD: ",&chunk, 75000,HERE)) {
	LEAF_ALERT("HTTP data fetch failed");
	modemHttpEnd(bearer);
	return -1;
      }
      if (chunk > chunk_size) {
	// can't happen.  yeah right.
	LEAF_ALERT("HTTP chunk of size %d exceeds configured buffer size %d", chunk, chunk_size);
	modemHttpEnd(bearer);
	return -1;
      }
	
      if (chunk > 0) {
	LEAF_NOTICE("Reading chunk of %d", chunk);
	if (!modemGetReplyOfSize((char *)chunk_buf, chunk, 30000)) {
	  LEAF_ALERT("Chunk Read failed");
	  modemHttpEnd(bearer);
	  return -1;
	}
      }
      LEAF_NOTICE("Got a chunk of %d", chunk);

      if (!chunk_callback((const uint8_t *)chunk_buf, chunk)) {
	LEAF_ALERT("Chunk callback indicated to abort");
	modemHttpEnd(bearer);
	return -1;
      }
      size -= chunk;
      got_size += chunk;

    } while (got_size < file_size);

    modemHttpEnd(bearer);

    LEAF_NOTICE("HTTP get complete");
    return got_size;
  }
#endif


  bool modemProcessURC(String Message) 
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
    else if (Message.startsWith("+SNPING4")) {
      if (Message.endsWith(",60000")) {
	LEAF_ALERT("PING TIMEOUT: %s", Message.c_str());
      }
      else {
	LEAF_WARN("PING RESPONSE: %s", Message.c_str());
      }
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
	if (!modemWaitPortMutex(HERE)) {
	  LEAF_ALERT("Cannot obtain modem port mutex to process [%s]", Message.c_str());
	}
	else {
	  ((IpClientSim7080 *)ip_clients[slot])->dataIndication(0);
	  modemReleasePortMutex(HERE); 
	}
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
};

// Local Variables:
// mode: C++
// c-basic-offset: 2
// End:
