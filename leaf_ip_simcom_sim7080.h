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

  virtual bool ipLinkStatus(bool force_correction=false) {
    LEAF_ENTER(L_DEBUG);

    // don't call the superclass because in this instance it is wrong

    String status_str = modemQuery("AT+CNACT?", "+CNACT: ",10*modem_timeout_default, HERE);

    LEAF_NOTICE("Connection status %s", status_str.c_str());

    // response is <pdpidx>,<statusx>,<addressx>, and we want <statusx>
    int comma = status_str.indexOf(',');
    if (comma >= 0) {
      status_str.remove(0, comma+1);
    }
    int statusx = status_str.toInt();
    bool status = statusx;

    if (force_correction) {
      if (force_correction) {
	LEAF_INFO("Sim7080 class says %s, checking for state change", HEIGHT(status));
      }
      if (isConnected() && !status) {
	LEAF_ALERT("IP Link lost");
	fslog(HERE, IP_LOG_FILE, "lte status link lost");
	ipOnDisconnect();
	ipScheduleReconnect();
      }
      else if (status && !isConnected()) {
	LEAF_ALERT("IP Link found");
	fslog(HERE, IP_LOG_FILE, "lte status link found");
	ipOnConnect();
      }
    }

    LEAF_BOOL_RETURN(status);
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


  int modemHttpGetWithCallback(
    const char *url,
    IPModemHttpHeaderCallback header_callback,
    IPModemHttpDataCallback chunk_callback,
    int bearer = 0,
    int chunk_size = 2048)
  {
    LEAF_NOTICE("httpGetWithCallback url=%s", url);
    const int chunk_size_max = 2048;
    char chunk_buf[chunk_size_max];
    char host_part[65];
    char path_part[255];
    char cmd[255];
    char exp[40];
    int err, state,size,hdr_size,conn_state;
    size_t file_size =0;
    size_t got_size = 0;
    int chunk_count = 0;
    int chunks_rcvd = 0;
    int chunk;
    char ua[32]="";
    char ref[16]="";

#ifdef BUILD_NUMBER
    snprintf(ua, sizeof(ua), "%s/%d (%s)", DEVICE_ID, BUILD_NUMBER, device_id);
#else
    snprintf(ua, sizeof(ua), "%s/1.0 (%s)", DEVICE_ID, device_id);
#endif

    stacx_heap_check(HERE, L_WARN);
    if (chunk_size > chunk_size_max) chunk_size = chunk_size_max;

    if (strncmp(url, "http://", 7)!=0) {
      LEAF_ALERT("Unsupported scheme in URL");
      return -1;
    }
    // TODO: https support

    const char *host_end = strchr(url+7, '/');
    if (host_end) {
      snprintf(path_part, sizeof(path_part), "%s", host_end);
      int host_len = host_end - url;
      memcpy(host_part, url, host_len);
      host_part[host_len] = '\0';
    }
    else {
      snprintf(host_part, sizeof(host_part), "%s", url);
      snprintf(path_part, sizeof(path_part), "/");
    }

    if (!modemSendExpectInt("AT+SHSTATE?", "+SHSTATE: ", &conn_state)) {
      LEAF_ALERT("SHSTATE? failed");
      return -1;
    }
    if (conn_state) {
      LEAF_WARN("Closing dangling connection");
      modemSendCmd(HERE, "AT+SHDISC");
    }

    //
    // The basic HTTP get example from the simcom app note only works for
    // files up to about 40k.   Above this it returns 602 NO MEMORY
    //
    // We work around this by doing partial-range queries for small
    // chunks of the file
    //

    if (!modemSendCmd(HERE, "AT+SHCONF=\"URL\",\"%s\"", host_part)) {
      LEAF_ALERT("URL rejected");
      return -1;
    }
    if (!modemSendCmd(HERE, "AT+SHCONF=\"BODYLEN\",1024")) {
      LEAF_ALERT("BODYLEN rejected");
      return -1;
    }
    if (!modemSendCmd(HERE, "AT+SHCONF=\"HEADERLEN\",350")) {
      LEAF_ALERT("HEADERLEN rejected");
      return -1;
    }
    if (!modemSendCmd(10000, HERE, "AT+SHCONN")) {
      LEAF_ALERT("SHCONN failed");
      return -1;
    }
    if (!modemSendExpectInt("AT+SHSTATE?", "+SHSTATE: ", &conn_state)) {
      LEAF_ALERT("SHSTATE? failed");
      return -1;
    }
    if (!conn_state) {
      LEAF_ALERT("Connection not established");
      return -1;
    }

    if (!modemSendCmd(HERE, "AT+SHCHEAD")) {
      LEAF_ALERT("SCHEAD failed");
      return -1;
    }
    if (!modemSendCmd(HERE, "AT+SHAHEAD=\"User-Agent\",\"%s\"", ua)) {
      LEAF_ALERT("User-Agent: header rejected");
      return -1;
    }
    if (!modemSendCmd(HERE, "AT+SHAHEAD=\"Referer\",\"%s\"", ref)) {
      LEAF_ALERT("Referer: header rejected");
      return -1;
    }
    if (!modemSendCmd(HERE, "AT+SHAHEAD=\"Connection\",\"keep-alive\"")) {
      LEAF_ALERT("Connection: header rejected");
      return -1;
    }
    if (!modemSendCmd(HERE, "AT+SHAHEAD=\"Accept\",\"*/*\"")) {
      LEAF_ALERT("Accept: header rejected");
      return -1;
    }

    // HEAD request, to get size
    snprintf(cmd, sizeof(cmd), "AT+SHREQ=\"%s\",5", path_part);
    int resp_code;
    int resp_len;

    if (!modemSendExpectIntPair(cmd, "+SHREQ: \"HEAD\",", &resp_code, &resp_len, 10000, 4, HERE)) {
      LEAF_ALERT("HEAD operation failed");
      modemSendCmd(HERE, "AT+SHDISC");
      return -1;
    }
    LEAF_NOTICE("HEAD response %d %d", resp_code, resp_len);
    if (resp_len) {
      snprintf(cmd, sizeof(cmd), "AT+SHREAD=0,%d", resp_len);
      if (!modemSendExpectInt(cmd, "+SHREAD: ", &hdr_size, 5000, HERE, 4, false)) {
	LEAF_ALERT("HEAD read failed");
	modemSendCmd(HERE, "AT+SHDISC");
	return -1;
      }
      if (hdr_size != resp_len) {
	LEAF_ALERT("Length mismatch want %d != got %d", resp_len, hdr_size);
	modemSendCmd(HERE, "AT+SHDISC");
	return -1;
      }
      LEAF_INFO("Reading header chunk of %d", hdr_size);
      if (!modemGetReplyOfSize(chunk_buf, hdr_size, 10000, -1, HERE)) {
	LEAF_ALERT("Header text failed");
	modemSendCmd(HERE, "AT+SHDISC");
	return -1;
      }

      //
      // Parse the header to find the content length
      //
      char *end_pos = chunk_buf+hdr_size;
      char *pos = chunk_buf;
      char *line_end ;
      while (pos < end_pos) {
	line_end = strchr(pos, '\n');
	if (!line_end) break;
	*line_end = '\0';
	if (strstr(pos, "Content-Length: ")==pos) {
	  file_size = atoi(pos + strlen("Content-Length: "));
	  LEAF_NOTICE("Content-Length for %s is %d", path_part, file_size);
	  break;
	}
	pos = line_end+1;
      }
    }

    if (header_callback) {
      if (!header_callback(resp_code, file_size, (const uint8_t *)chunk_buf, (size_t)hdr_size)) {
	LEAF_WARN("Header callback says abort");
	modemSendCmd(HERE, "AT+SHDISC");
	return -1;
      }
    }

    if (file_size == 0) {
      LEAF_ALERT("Did not get file size");
      modemSendCmd(HERE, "AT+SHDISC");
      return -1;
    }

    chunk_count = (int)(file_size/chunk_size);
    if (file_size % chunk_size) ++chunk_count;
    LEAF_NOTICE("Fetching %s will require %d chunks (each %d)", path_part, chunk_count, chunk_size);
    chunks_rcvd = 0;
    do {
      Leaf::wdtReset(HERE);
      stacx_heap_check(HERE, L_WARN);

      // Send a HTTP get.   Because we are using partial range we expect 206,1024
      // i.e. size here is the size of the first chunk only
      size = (file_size - got_size);
      if (size > chunk_size_max) size = chunk_size_max;

      if (!modemSendCmd(HERE, "AT+SHCHEAD")) {
	LEAF_ALERT("Header reset (SCHEAD) failed");
	return -1;
      }
      if (!modemSendCmd(HERE, "AT+SHAHEAD=\"User-Agent\",\"%s\"", ua)) {
	LEAF_ALERT("User-Agent: header rejected");
	return -1;
      }
      snprintf(ref, sizeof(ref), "chunk %d/%d", chunks_rcvd+1, chunk_count);
      if (!modemSendCmd(HERE, "AT+SHAHEAD=\"Referer\",\"%s\"", ref)) {
	LEAF_ALERT("Referer: header rejected");
	return -1;
      }
      if (!modemSendCmd(HERE, "AT+SHAHEAD=\"Connection\",\"keep-alive\"")) {
	LEAF_ALERT("Connection: header rejected");
	return -1;
      }
      if (!modemSendCmd(HERE, "AT+SHAHEAD=\"Accept\",\"*/*\"")) {
	LEAF_ALERT("Accept: header rejected");
	return -1;
      }

      if (!modemSendCmd(HERE, "AT+SHAHEAD=\"Range\",\"bytes=%d-%d\"", got_size, got_size+size-1)) {
	LEAF_ALERT("Range: header rejected");
	return -1;
      }

      Leaf::wdtReset(HERE);

      snprintf(cmd, sizeof(cmd), "AT+SHREQ=\"%s\",1", path_part);
      if (!modemSendExpectIntPair(cmd, "+SHREQ: \"GET\",", &resp_code, &resp_len, 10000, 4, HERE)) {
	LEAF_ALERT("GET operation failed");
	modemSendCmd(HERE, "AT+SHDISC");
	return -1;
      }
      if (!resp_len) {
	continue;
      }
      if (resp_code != 206) { // partial content
	LEAF_ALERT("HTTP error code %d", resp_code);
	modemSendCmd(HERE, "AT+SHDISC");
	return -1;
      }

      Leaf::wdtReset(HERE);

      snprintf(cmd, sizeof(cmd), "AT+SHREAD=0,%d", resp_len);
      int read_len;
      if (!modemSendExpectInt(cmd, "+SHREAD: ", &chunk, 5000, HERE, 4, false)) {
	LEAF_ALERT("HEAD read failed");
	modemSendCmd(HERE, "AT+SHDISC");
	return -1;
      }
      if (chunk != resp_len) {
	LEAF_ALERT("Length mismatch want %d != got %d", resp_len, chunk);
	modemSendCmd(HERE, "AT+SHDISC");
	return -1;
      }
      LEAF_INFO("Reading body chunk of %d", chunk);
      if (!modemGetReplyOfSize(chunk_buf, chunk, 10000, -1, HERE)) {
	LEAF_ALERT("Body read failed");
	modemSendCmd(HERE, "AT+SHDISC");
	return -1;
      }

      Leaf::wdtReset(HERE);

      ++chunks_rcvd;
      LEAF_NOTICE("Got chunk %d/%d, size=%d", chunks_rcvd, chunk_count, chunk);
      if (chunk_callback && !chunk_callback((const uint8_t *)chunk_buf, chunk)) {
	LEAF_ALERT("Chunk callback indicated to abort");
	modemSendCmd(HERE, "AT+SHDISC");
	return -1;
      }
      size -= chunk;
      got_size += chunk;

#if 0
      if (ip_modem_use_urc) {
	// Check for other incoming notifications during loong download
	modemCheckURC();
      }
#endif

    } while (got_size < file_size);

    stacx_heap_check(HERE, L_WARN);
    LEAF_NOTICE("HTTP get complete");
    modemSendCmd(HERE, "AT+SHDISC");
    return got_size;
  }

};

// Local Variables:
// mode: C++
// c-basic-offset: 2
// End:
