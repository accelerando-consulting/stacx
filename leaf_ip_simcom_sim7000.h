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
    : AbstractIpSimcomLeaf(name,target,uart,rxpin,txpin,baud,options,pwrpin,keypin,sleeppin,run,autoprobe)
    , Debuggable(name)
  {
    this->invert_key = true; // invert_key; // true=pulse LOW to power on, false=pulse HIGH
    duration_key_on=250;
    ip_reuse_connection = false;
  }

  virtual bool ipSetApName(String apn) {
    bool result = true;
    if (!modemSendCmd(HERE, "AT+CGDCONT=1,\"IP\",\"%s\"", apn.c_str())) {
      result = false;
    }
    String tt_ap = modemQuery("AT+CSTT?", "+CSTT: ", -1, HERE);
    if (tt_ap.startsWith("\"") &&
	tt_ap.substring(1, apn.length())==apn &&
	(tt_ap.substring(apn.length()+1,1)=="\"")) {
      // the TT value is already as desired.
      // Don't try to set it again because the modem will give an error.
    }
    else {
      if (!modemSendCmd(HERE, "AT+CSTT=\"%s\"", apn.c_str())) {
	result = false;
      }
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

  bool modemHttpBegin(const char *url, int bearer)
  {
    if (!modemBearerBegin(bearer)) {
      LEAF_ALERT("modemHttpBegin: could not get TCP bearer");
      return false;
    }

    if (!modemSendCmd(HERE, "AT+HTTPINIT")) {
      LEAF_ALERT("HTTP session initiation failed");
      modemBearerEnd(bearer);
      return false;
    }

    if (!modemSendCmd(HERE, "AT+HTTPPARA=CID,%d", bearer)) {
      LEAF_ALERT("HTTP bearer profile set rejected");
      modemBearerEnd(bearer);
      return false;
    }

    if (!modemSendCmd(HERE, "AT+HTTPPARA=URL,\"%s\"", url)) {
      LEAF_ALERT("HTTP URL set failed");
      modemBearerEnd(bearer);
      return false;
    }
    return true;
  }

  bool modemHttpEnd(int bearer)
  {
    modemSendCmd(HERE, "AT+HTTPTERM");
    modemBearerEnd(bearer);
    return true;
  }


  virtual int modemHttpGetWithCallback(const char *url,
				       IPModemHttpHeaderCallback header_callback,
				       IPModemHttpDataCallback chunk_callback,
				       int bearer=0,
				       int chunk_size=2048)
  {
    char cmd[255];
    char exp[40];
    int err, state,size,hdr_size;
    size_t file_size =0;
    size_t got_size = 0;
    int chunk;
    const int chunk_size_max = 1024;
    char chunk_buf[chunk_size_max];

    if (chunk_size > chunk_size_max) chunk_size = chunk_size_max;

    LEAF_NOTICE("httpGetWithCallback url=%s bearer=%d", url, bearer);
    if (!modemHttpBegin(url, bearer)) {
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

	if (!header_callback(err, file_size, (const uint8_t *)chunk_buf), hdr_size) {
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


  virtual bool ipGetAddress(bool link_test=true) {
    String response = modemQuery("AT+CNACT?","+CNACT: ", 10*modem_timeout_default,HERE);
    if (response && response.startsWith("1,")) {
      ip_addr_str = response.substring(3,response.length()-1);

      //response = modemQuery("AT+CIFSR", "+CIFSR: ", -1, HERE);

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

    String tt_ap = modemQuery("AT+CSTT?", "+CSTT: ", -1, HERE);
    if (!tt_ap.startsWith("\""+ip_ap_name+"\"")) {
      LEAF_WARN("Sim7000 lost CSTT setting");
      if (!modemSendCmd(HERE, "AT+CSTT=\"%s\"", ip_ap_name.c_str())) {
	return false;
      }
    }

    int was = getDebugLevel();
    bool twas = modemDoTrace();
    setDebugLevel(L_DEBUG);
    modemSetTrace();

    bool result = modemSendExpect("AT+CNACT=1", "+APP PDP: ", NULL, 0, -1, 3, HERE);
    //int result = modemGetReply(result_buf, sizeof(result_buf), 4000, 4, 0, HERE);
    LEAF_NOTICE("result=%s result_buf=%s", TRUTH(result), result_buf);
    setDebugLevel(was);
    modemSetTrace(twas);

    if (result && !strstr(result_buf,"DEACTIVE")) {
      LEAF_NOTICE("CNACT returned ok");

      modemSendCmd(HERE, "AT+CIICR"); // is this necessary
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
    String result = modemQuery("AT+CNACT=0","",2000,HERE);
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
