#pragma once
#include "abstract_ip_lte.h"

//
//@************************* class AbstractIpSimcomLeaf ***************************
//
// This class encapsulates a TCP/IP interface via a Simcom modem
//

class AbstractIpSimcomLeaf : public AbstractIpLTELeaf
{
public:

  AbstractIpSimcomLeaf(String name, String target, int uart, int rxpin, int txpin, int baud=115200, uint32_t options=SERIAL_8N1, int8_t pwrpin=MODEM_PWR_PIN_NONE, int8_t keypin=MODEM_KEY_PIN_NONE, int8_t sleeppin=MODEM_SLP_PIN_NONE, bool run = LEAF_RUN,bool autoprobe=true) :
    AbstractIpLTELeaf(name,target,uart,rxpin,txpin,baud,options,pwrpin,keypin,sleeppin,run,autoprobe)
    , Debuggable(name)
  {
    ip_enable_ssl = false;
  }

  virtual bool modemProbe(codepoint_t where=undisclosed_location, bool quick=false);
  virtual bool ipModemConfigure();
  virtual bool ipConnect(String reason);
  virtual bool ipConnectFast();
  virtual bool ipConnectCautious();
    


  virtual bool ipSetApName(String apn) { return modemSendCmd(HERE, "AT+CNACT=1,\"%s\"", apn.c_str()); }
  virtual bool ipGetAddress(bool link_test=true) {
    String response = modemQuery("AT+CNACT?","+CNACT: ", 10*modem_timeout_default,HERE);
    if (response && response.startsWith("1,")) {
      ip_addr_str = response.substring(1,response.length()-1);
      return true;
    }
    return false;
  }
  virtual bool modemCarrierStatus() {
    LEAF_ENTER(L_DEBUG);
    if (!modemSendExpect("AT+CPSI?","+CPSI: ", modem_response_buf, modem_response_max, -1, -1, HERE) ||
	strstr(modem_response_buf, "NO SERVICE")) {
      LEAF_WARN("modemCarrierStatus is bad [%s]", modem_response_buf);
      LEAF_BOOL_RETURN(false);
    }
    LEAF_BOOL_RETURN(true);
  }
  
  virtual bool modemSignalStatus() {
    int i;
    LEAF_ENTER(L_DEBUG);
    if (!modemSendExpectInt("AT+CSQ","+CSQ: ", &i, -1, HERE)) {
      return false;
    }
    ip_rssi = 0-i;
    if (i == 99) {
      LEAF_BOOL_RETURN(false);
    }
    LEAF_BOOL_RETURN(true);
  }
 
  virtual void ipPullUpdate(String url);
  virtual void ipRollbackUpdate(String url);

  virtual float modemReadVcc();
  virtual void pre_sleep(int duration=0);
  virtual bool modemProcessURC(String Message);

  const char *ftpErrorString(int code);
  virtual bool modemReadFile(const char *filename, char *buf, int buf_size, int partition=-1,int timeout=-1);
  virtual bool modemWriteFile(const char *filename, const char *contents, int size=-1, int partition=-1,int timeout=-1);
  virtual bool modemWriteFileVerify(const char *filename, const char *contents, int size=-1, int partition=-1,int timeout=-1);
  virtual bool modemInstallCert();



  virtual bool modemFtpPut(const char *host, const char *user, const char *pass, const char *path, const char *buf, int buf_len, int bearer=1);
  virtual int modemFtpGet(const char *host, const char *user, const char *pass, const char *path, char *buf, int buf_max, int bearer=1);
  virtual int modemHttpGetWithCallback(const char *url,
				  IPModemHttpHeaderCallback header_callback,
				  IPModemHttpDataCallback chunk_callback,
				  int bearer=1,
				  int chunk_size=1024);

  virtual bool ftpPut(String host, String user, String pass, String path, const char *buf, int buf_len)
  {
    LEAF_ENTER(L_NOTICE);
    LEAF_NOTICE("Uploading %s of size %d", path.c_str(), buf_len);
    bool result = modemFtpPut(host.c_str(), user.c_str(), pass.c_str(), path.c_str(), buf, buf_len);
    LEAF_RETURN(result);
  }
#if 0
  virtual bool httpPost(char *url, const uint8_t *data, int len, const char *type="application/octet-stream");
#endif



protected:
  virtual bool modemFsBegin();
  virtual bool modemFsEnd();
  virtual bool modemBearerBegin(int bearer=1) ;
  virtual bool modemBearerEnd(int bearer=1);
  virtual bool modemFtpBegin(const char *host, const char *user, const char *pass, int bearer=1);

  virtual bool modemFtpEnd(int bearer=1);
  virtual bool modemHttpBegin(const char *url, int bearer);
  virtual bool modemHttpEnd(int bearer=1);

};

const char *AbstractIpSimcomLeaf::ftpErrorString(int code)
{
  switch(code) {
  case 1:
    return "Success";
  case 61:
    return "Network error";
  case 62:
    return "DNS error";
  case 63:
    return "Connect failed";
  case 64:
    return "Timeout error";
  case 65:
    return "Server error";
  case 66:
    return "Operation not allowed";
  case 70:
    return "Replay error";
  case 71:
    return "User error";
  case 72:
    return "Password error";
  case 73:
    return "Type error";
  case 74:
    return "Rest error";
  case 75:
    return "Passive error";
  case 76:
    return "Active error";
  case 77:
    return "Operation error";
  case 78:
    return "Upload error";
  case 79:
    return "Download error";
  case 80:
    return "Manual quit";
  default:
    return "Unknown";
  }
}

bool AbstractIpSimcomLeaf::modemProbe(codepoint_t where, bool quick) 
{
  if (!AbstractIpLTELeaf::modemProbe(where, quick)) {
    LEAF_NOTICE_AT(where, "modemProbe superclass probe failed");
    return false;
  }

  LEAF_ENTER_BOOL(L_NOTICE,quick);
  if (quick) {
    LEAF_BOOL_RETURN(true);
  }
  
  if (ip_enable_ssl) {
    LEAF_INFO("Checking SSL certificate status");
    char certbuf[2048];

    if (!modemReadFile("cacert.pem", certbuf, sizeof(certbuf))) {
      LEAF_NOTICE("No CA cert present, loading.");
      if (!modemInstallCert()) {
	LEAF_ALERT("Failed to load CA cert.");
	ip_enable_ssl = false;
      }
    }
  }

  LEAF_BOOL_RETURN(true);
}


bool AbstractIpSimcomLeaf::modemFsBegin() 
{
  LEAF_ENTER(L_NOTICE);
  if (!modemWaitPortMutex(HERE)) {
    LEAF_ALERT("Could not acquire modem port mutex");
    LEAF_BOOL_RETURN(false);
  }
  if (modemSendCmd(HERE, "AT+CFSINIT")) {
    LEAF_BOOL_RETURN(true);
  }
  
  LEAF_ALERT("FS prepare command not accepted");
  if (!modemSendCmd(HERE, "AT+CFSTERM")) {
    LEAF_ALERT("FS close command not accepted");
    modemReleasePortMutex(HERE);
    LEAF_BOOL_RETURN(false);
  }

  LEAF_NOTICE("Closed out a dangling FS session, retry FS prepare");
  if (!modemSendCmd(HERE, "AT+CFSINIT")) {
    LEAF_ALERT("FS prepare command still not accepted");
    modemReleasePortMutex(HERE);
    LEAF_BOOL_RETURN(false);
  }
  LEAF_BOOL_RETURN(true);
}

bool AbstractIpSimcomLeaf::modemFsEnd() 
{
  LEAF_ENTER(L_NOTICE);
  if (!modemSendCmd(5000, HERE, "AT+CFSTERM")) {
    LEAF_ALERT("Session commit failed");
    modemReleasePortMutex(HERE);
    LEAF_BOOL_RETURN(false);
  }

  modemReleasePortMutex(HERE);
  LEAF_BOOL_RETURN(true);
}

bool AbstractIpSimcomLeaf::modemReadFile(const char *filename, char *buf, int buf_size, int partition, int timeout) {
  LEAF_ENTER(L_INFO);
  if (partition<0) partition=3;
  LEAF_NOTICE("modemReadFile %s (partition %d, buf_size %d)", filename, partition, buf_size);
  int size = 0;
  char cmd[80]="";

  if (!modemFsBegin()) {
    LEAF_BOOL_RETURN(false);
  }
  
  snprintf(cmd, sizeof(cmd), "AT+CFSGFIS=%d,\"%s\"", partition, filename);
  if (!modemSendExpectInt(cmd, "+CFSGFIS: ", &size, timeout, HERE)) {
    LEAF_ALERT("File not found");
    LEAF_BOOL_RETURN(false);
  }
  if (size >= buf_size) {
    // insist on room for a null terminator so need n+1 buf space to read n bytes
    LEAF_ALERT("File too large");
    LEAF_BOOL_RETURN(false);
  }

  snprintf(cmd, sizeof(cmd), "AT+CFSRFILE=3,\"%s\",0,%d,0", filename, size);
  if (modemSendExpectInt(cmd, "+CFSRFILE: ", &size, timeout, HERE)) {
    if (size >= buf_size) {
      // wtf, size changed
      LEAF_ALERT("File too large");
      LEAF_BOOL_RETURN(false);
    }
    if (!modemGetReplyOfSize(buf, size)) {
      LEAF_ALERT("Read failed");
      LEAF_BOOL_RETURN(false);
    }
    if (size < buf_size) buf[size]='\0'; // null terminate
  }

  if (!modemSendExpectOk("", HERE)) {
    LEAF_ALERT("Read failed");
    LEAF_BOOL_RETURN(false);
  }

  if (!modemSendCmd(timeout, HERE, "AT+CFSTERM")) {
    LEAF_ALERT("Read cleanup failed");
    LEAF_BOOL_RETURN(false);
  }

  LEAF_BOOL_RETURN(true);
}



bool AbstractIpSimcomLeaf::modemWriteFile(const char *filename, const char *contents, int size, int partition,int timeout) {
  LEAF_ENTER(L_INFO);
  
  int free = 0;
  int reads = 0;
  if (size < 0) size = strlen(contents);
  if (partition < 0) partition = 3;

  LEAF_NOTICE("modemWrite %s (partition %d, size %d)", filename, partition, size);
  if (size > ip_modem_max_file_size) {
    LEAF_ALERT("File too large (%d bytes max)", ip_modem_max_file_size);
    LEAF_BOOL_RETURN(false);
  }
  if (!modemSendExpectInt("AT+CFSGFRS?","+CFSGFRS: ", &free, -1, HERE)) {
    LEAF_ALERT("Cannot get filesystem size");
    LEAF_BOOL_RETURN(false);
  }
  if (free < size) {
    LEAF_ALERT("Insufficient free space");
    LEAF_BOOL_RETURN(false);
  }

  if (!modemFsBegin()) {
    LEAF_ALERT("Could not begin modem FS session");
    LEAF_BOOL_RETURN(false);
  }

  snprintf(modem_command_buf, modem_command_max, "AT+CFSWFILE=%d,\"%s\",0,%d,10000", partition, filename, size);
  //LEAF_INFO("Write to flash filesystem: %s", modem_command_buf);
  if (!modemSendExpect(modem_command_buf, "DOWNLOAD",NULL,0,timeout,1,HERE)) {
    LEAF_ALERT("writeFile: Write command not accepted");
    modemFsEnd();
    LEAF_BOOL_RETURN(false);
  }
  if (!modemSendExpect(contents, "OK",NULL,0,timeout,1,HERE)) {
    LEAF_ALERT("writeFile: Download failed");
    modemFsEnd();
    LEAF_BOOL_RETURN(false);
  }

  LEAF_BOOL_RETURN(modemFsEnd());
}

bool AbstractIpSimcomLeaf::modemWriteFileVerify(const char *filename, const char *contents, int size, int partition, int timeout) {
  LEAF_ENTER(L_NOTICE);
  
  if (size < 0) size = strlen(contents);

  if (modemWriteFile(filename, contents,size,partition)) {
    char buf[4096];
    LEAF_NOTICE("Wrote file, reading back to compare");

    if (modemReadFile(filename, buf, sizeof(buf), partition, timeout)) {
      //LEAF_INFO("Readback complete");

      if (strcmp(contents, buf) == 0) {
	//LEAF_INFO("Compare matched");
	LEAF_BOOL_RETURN(true);
      }
      else {
	LEAF_ALERT("writeFileVerify: Compare MISMATCH");
	for (int n=0; n<strlen(contents);n++) {
	  if (!strncmp(contents, buf, n)) {
	    LEAF_NOTICE("mismatch starts at position %d\n",n);
	    break;
	  }
	}
      }
    }
    else {
      LEAF_ALERT("writeFileVerify: Readback failed");
    }
  }
  LEAF_BOOL_RETURN(false);
}

bool AbstractIpSimcomLeaf::modemBearerBegin(int bearer) 
{
  char cmd[40];
  char exp[40];

  if (!modemWaitPortMutex(HERE)) {
    LEAF_ALERT("Could not acquire modem port mutex");
    return false;
  }
  int retry = 0;
  int state = 3;
  
  while (retry<2) {
    // Get bearer status
    snprintf(cmd, sizeof(cmd), "AT+SAPBR=2,%d", bearer);
    snprintf(exp, sizeof(exp), "+SAPBR: %d,", bearer);
    if (!modemSendExpectInt(cmd, exp, &state, -1, HERE)) {
      LEAF_ALERT("Bearer query failed");
      modemReleasePortMutex(HERE);
      return false;
    }

    if (state != 1) {
      LEAF_NOTICE("Bearer state is %d, need to reopen (0=connecting, 1=connected, 2=closing, 3=closed)\n", state);
      // Configure bearer profile
      if (!modemSendCmd(HERE, "AT+SAPBR=3,%d,\"APN\",\"telstra.m2m\"", bearer)) {
	LEAF_ALERT("Bearer configure failed");
	modemReleasePortMutex(HERE);
	return false;
      }

      if (modemSendCmd(HERE, "AT+SAPBR=1,%d", bearer)) {
	// initate suceeded, return success holding mutex
	return true;
      }

      if (!retry) {
	LEAF_NOTICE("Bearer not available, try reopen");
	if (!modemSendCmd(HERE, "AT+SAPBR=0,%d", bearer)) {
	  LEAF_ALERT("Bearer close failed");
	  modemReleasePortMutex(HERE);
	  return false;
	}
	//LEAF_INFO("Closed bearer, will retry open");
	++retry;
      }
    }
  }
  
  LEAF_ALERT("could not get modem bearer");
  modemReleasePortMutex(HERE);
  return false;
}

bool AbstractIpSimcomLeaf::modemBearerEnd(int bearer)
{
  LEAF_ENTER_INT(L_NOTICE, bearer);
  
  if (!modemSendCmd(HERE, "AT+SAPBR=0,%d", bearer)) {
    LEAF_ALERT("Bearer close failed (non fatal)");
    // this seems to be non-fatal
    //return false;
  }
  modemReleasePortMutex(HERE);
  LEAF_BOOL_RETURN(true);
}

bool AbstractIpSimcomLeaf::modemFtpBegin(const char *host, const char *user, const char *pass, int bearer) 
{
  LEAF_ENTER(L_NOTICE);
  ipCommsState(TRANSACTION, HERE);
  if (!modemBearerBegin(bearer)) {
    LEAF_ALERT("ftpBegin: could not get TCP bearer");
    LEAF_BOOL_RETURN(false);
  }

  int max_cid = 3;
retry_bearer:
  if (!modemSendCmd(HERE, "AT+FTPCID=%d", bearer)) {
    LEAF_ALERT("ftpBegin: FTP channel initiation failed");
    if (bearer < max_cid) {
      ++bearer;
      LEAF_WARN("Retry FTPCID with bearer %d", bearer);
      goto retry_bearer;
    }
    modemBearerEnd(bearer);
    LEAF_BOOL_RETURN(false);
  }

  //LEAF_INFO("We seem to have an FTP session now");
  if (!modemSendCmd(HERE, "AT+FTPMODE=1")) {
    LEAF_ALERT("ftpBegin: FTP passive mode set failed");
    modemBearerEnd();
    LEAF_BOOL_RETURN(false);
  }

  if (!modemSendCmd(HERE, "AT+FTPSERV=\"%s\"", host)) {
    LEAF_ALERT("ftpBegin: FTP server failed");
    LEAF_BOOL_RETURN(false);
  }

  if (!modemSendCmd(HERE, "AT+FTPUN=\"%s\"", user)) {
    LEAF_ALERT("ftpBegin: FTP user failed");
    LEAF_BOOL_RETURN(false);
  }

  if (!modemSendCmd(HERE, "AT+FTPPW=\"%s\"", pass)) {
    LEAF_ALERT("ftpBegin: FTP pass failed");
    LEAF_BOOL_RETURN(false);
  }
  LEAF_BOOL_RETURN(true);
}

bool AbstractIpSimcomLeaf::modemFtpEnd(int bearer) 
{
  LEAF_ENTER_INT(L_NOTICE, bearer);
  modemBearerEnd(bearer);
  if (pubsubLeaf && pubsubLeaf->isConnected()) {
    if (stacx_comms_state==TRANSACTION) {
      ipCommsState(REVERT, HERE);
    }
    else {
      ipCommsState(ONLINE,HERE);
    }
  }
  else if (isConnected(HERE)) {
    ipCommsState(WAIT_PUBSUB, HERE);
  }
  else {
    ipCommsState(WAIT_IP, HERE);
  }
  LEAF_BOOL_RETURN(true);
}

bool AbstractIpSimcomLeaf::modemFtpPut(const char *host, const char *user, const char *pass, const char *path, const char *buf, int buf_len, int bearer)
{
  int state,err;
  int retry = 0;
  LEAF_ENTER(L_NOTICE);
  LEAF_NOTICE("modemFtpPut path=[%s]", path);

  if (!modemFtpBegin(host, user, pass, bearer)) {
    LEAF_ALERT("FTP session failed");
    LEAF_BOOL_RETURN(false);
  }

  char dir[128];
  char name[128];
  int dir_len = strcspn(path, "/");
  if (dir_len == strlen(path)) {
    // Path contains no slash, presume root
    LEAF_NOTICE("Storing to rood direcotry");
    snprintf(name, sizeof(name), "%s", path);
    snprintf(dir, sizeof(dir), "/");
  }
  else {
    memcpy(dir, path, dir_len);
    dir[dir_len]='/';
    dir[dir_len+1]='\0';
    strncpy(name, path+dir_len+1, sizeof(name));
    LEAF_NOTICE("Split upload path into '%s' and '%s'", dir, name);
  }
  if (!modemSendCmd(HERE, "AT+FTPPUTPATH=\"%s\"", dir)) {
    LEAF_ALERT("FTP path failed");
    modemFtpEnd(bearer);
    LEAF_BOOL_RETURN(false);
  }

  if (!modemSendCmd(HERE, "AT+FTPPUTNAME=\"%s\"", name)) {
    LEAF_ALERT("FTP name failed");
    modemFtpEnd(bearer);
    LEAF_BOOL_RETURN(false);
  }

  int chunk = 200;
  char cmd[32];
  err = 0;
  snprintf(cmd, sizeof(cmd), "AT+FTPPUT=1");
  int dns_retry = 3;
ftp_dns_retry:  
  if (!modemSendExpectIntPair("AT+FTPPUT=1", "+FTPPUT: 1,",&err,&chunk,ip_ftp_timeout_sec*1000,4, HERE)) {
    if ((err == 62) && (dns_retry>0)) {
      LEAF_WARN("FTP reported DNS error, retry count %d", dns_retry);
      --dns_retry;
      goto ftp_dns_retry;
    }
    LEAF_ALERT("FTP put initiate failed (%d)", err);
    modemFtpEnd(bearer);
    LEAF_BOOL_RETURN(false);
  }
  if (err != 1) {
    LEAF_ALERT("FTP state machine reports error %d, %s", err, ftpErrorString(err));
    modemFtpEnd(bearer);
    LEAF_BOOL_RETURN(false);
  }

  int sent = 0;

  while (sent < buf_len) {
    int remain = buf_len - sent;
    int thischunk = (remain > chunk) ? chunk : remain;

    LEAF_NOTICE("Sending chunk of %d bytes from remaining size %d", thischunk, remain);
    snprintf(modem_command_buf, modem_command_max, "AT+FTPPUT=2,%d", thischunk);
    if (!modemSendExpectIntPair(modem_command_buf, "+FTPPUT: ",&err,&thischunk,ip_ftp_timeout_sec*1000, 2, HERE)) {
      LEAF_ALERT("FTP data put failed");
      modemFtpEnd(bearer);
      LEAF_BOOL_RETURN(false);
    }
    if (err != 2) {
      LEAF_ALERT("FTP data put rejected");
      modemFtpEnd(bearer);
      LEAF_BOOL_RETURN(false);
    }
    if (thischunk == 0) {
      LEAF_NOTICE("Modem not ready, wait");
      delay(500);
      continue;
    }
    modem_stream->write((const uint8_t *)buf+sent, thischunk);
    sent += thischunk;
    LEAF_NOTICE("Wrote %d bytes, total so far %d of %d", thischunk, sent, buf_len);

    if (!modemSendExpectIntPair(NULL, "+FTPPUT: 1,",&err,&chunk,ip_ftp_timeout_sec*1000,4,HERE)) {
      LEAF_NOTICE("FTP continuation message not parsed");
      modemFtpEnd(bearer);
      LEAF_BOOL_RETURN(false);
    }
    if (err != 1) {
      LEAF_ALERT("FTP state machine did not invite continuation");
      modemFtpEnd(bearer);
      LEAF_BOOL_RETURN(false);
    }
    LEAF_NOTICE("FTP invites us to continue, next chunk size is %d",chunk);
  }

  // give tcp a little time to drain
  delay(500);

  // close the control connection
  if (!modemSendCmd(ip_ftp_timeout_sec*1000, HERE, "AT+FTPPUT=2,0")) {
    LEAF_ALERT("FTP put close failed");
    modemFtpEnd(bearer);
    LEAF_BOOL_RETURN(false);
  }

  modemFtpEnd(bearer);
  LEAF_BOOL_RETURN(true);
}

int AbstractIpSimcomLeaf::modemFtpGet(const char *host, const char *user, const char *pass, const char *path, char *buf, int buf_max, int bearer)
{
  int state,count,err;
  int size = 0;

  if (!modemFtpBegin(host, user, pass, bearer)) {
    return false;
  }

  char dir[128];
  char name[128];
  int dir_len = strcspn(path, "/");
  if (dir_len == strlen(path)) {
    // Path contains no slash, presume root
    snprintf(name, sizeof(name), "%s", path);
    snprintf(dir, sizeof(dir), "/");
  }
  else {
    memcpy(dir, path, dir_len);
    dir[dir_len]='\0';
    strncpy(name, path+dir_len+1, sizeof(name));
    LEAF_NOTICE("Split server path into '%s' and '%s'", path, name);
  }

  if (!modemSendCmd(HERE, "AT+FTPGETNAME=\"%s\"", name)) {
    LEAF_ALERT("FTP name failed");
    modemFtpEnd(bearer);
    return -1;
  }

  if (!modemSendCmd(HERE, "AT+FTPGETPATH=\"%s\"", dir)) {
    LEAF_ALERT("FTP path failed");
    modemFtpEnd(bearer);
    return -1;
  }

  if (!modemSendExpectIntPair("AT+FTPGET=1", "+FTPGET: ",&state,&err,ip_ftp_timeout_sec*1000,2,HERE)) {
    LEAF_ALERT("FTP get initiate failed");
    modemFtpEnd(bearer);
    return -1;
  }
  if (err != 1) {
    LEAF_ALERT("FTP get error %s", ftpErrorString(err));
    modemFtpEnd(bearer);
    return -1;
  }

  int chunk_max = 200;
  snprintf(modem_command_buf, modem_command_max, "AT+FTPGET=2,%d", chunk_max);
  state = 2;
  while (state == 2) {
    if (!modemSendExpectIntPair(modem_command_buf, "+FTPGET: ",&state, &count, ip_ftp_timeout_sec*1000, 2, HERE)) {
      LEAF_ALERT("FTP data fetch failed");
      return -1;
    }
    if (state == 1) {
      // connection is over
      LEAF_NOTICE("FTP data connection complete");
      break;
    }
    if ((state==2) && (count == 0)) {
      // connection still open but data not ready yet
      delay(100);
    }
    if (count > 0) {
      if (count > buf_max) {
	LEAF_ALERT("Reply buffer overflow");
	modemFtpEnd(bearer);
	return -1;
      }
      //LEAF_INFO("Reading chunk of %d", count);
      if (!modemGetReplyOfSize(buf, count, ip_ftp_timeout_sec*1000, L_INFO)) {
	LEAF_ALERT("Read failed");
	modemFtpEnd(bearer);
	return -1;
      }
      buf += count;
      size += count;
      buf_max -=count;
    }

    if (modemSendExpectIntPair(NULL, "+FTPGET: ",&state, &count, ip_ftp_timeout_sec*1000, 2, HERE)) {
      if (state == 1) {
	if (count == 0) {
	  LEAF_NOTICE("Connection complete");
	  // Connection complete
	  break;
	}
	else if (count == 1) {
	  LEAF_NOTICE("Read again");
	  state=2;
	  // There is more data, but we don't know how mouch
	  continue;
	}
	else {
	  LEAF_ALERT("Unexpected status");
	}
      }
      else if (state == 2) {
	LEAF_NOTICE("Bytes remain %d", count);
      }
    }
  }

  modemFtpEnd(bearer);
  LEAF_NOTICE("ftpGet complete");
  return size;
}

bool AbstractIpSimcomLeaf::modemHttpBegin(const char *url, int bearer) 
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

bool AbstractIpSimcomLeaf::modemHttpEnd(int bearer) 
{
  modemSendCmd(HERE, "AT+HTTPTERM");
  modemBearerEnd(bearer);
  return true;
}

int AbstractIpSimcomLeaf::modemHttpGetWithCallback(const char *url,
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

void AbstractIpSimcomLeaf::ipPullUpdate(String url)
{
  MD5Builder checksum;
  checksum.begin();

  const bool test = true;

  modemHttpGetWithCallback(
    url.c_str(),
    [test](int status, size_t len, const uint8_t *hdr) -> bool
    {
      NOTICE("HTTP header code=%d len=%lu hdr=%s", status, (unsigned long)len, (char *)hdr);
      if (!test) Update.begin(len, U_FLASH);
      return true;
    },
    [&checksum,test](const uint8_t *buf, size_t len) -> bool
    {
      checksum.add((uint8_t *)buf, len);
      NOTICE("HTTP chunk callback len=%lu", (unsigned long)len);
      size_t wrote;
      if (test) {
	wrote = len;
      }
      else {
	wrote = Update.write((uint8_t *)buf, len);
      }
      return (wrote == len);
    }
    );

  unsigned char digest[16];
  checksum.calculate();
  LEAF_NOTICE("HTTP file digest [%s]", checksum.toString().c_str())
  if (!test) Update.end();
}

void AbstractIpSimcomLeaf::ipRollbackUpdate(String url)
{
  if (Update.canRollBack()) {
    LEAF_ALERT("Rolling back to previous version");
    if (Update.rollBack()) {
      LEAF_NOTICE("Rollback succeeded.  Rebooting.");
      delay(1000);
      Leaf::reboot("rollback");
    }
    else {
      LEAF_ALERT("Rollback failed");
    }
  }
  else {
    LEAF_ALERT("Rollback is not possible");
  }
}

// Read the module's power supply voltage
float AbstractIpSimcomLeaf::modemReadVcc() {

  String volts = modemQuery("AT+CBC", "+CBC: ",-1,HERE);
  return volts.toFloat();
}

#if 0
// fixme implement
bool AbstractIpSimcomLeaf::httpPostFile(char *url, const uint8_t *data, int len, const char *type="application/octet-stream") {
  uint16_t status, resplen;

  HTTP_POST_start(url, F("application/jpeg"), data, len, &status, &resplen);
  LEAF_NOTICE("POST result %d len %d", (int)status, (int)resplen);
  HTTP_POST_end();
  return (status==200);
}
#endif

bool AbstractIpSimcomLeaf::ipConnect(String reason)
{
  if (!AbstractIpLTELeaf::ipConnect(reason)) {
    // Superclass says no
    return(false);
  }
  LEAF_ENTER_STR(L_INFO, reason);

  LEAF_NOTICE("ipConnect (%s) (ap=%s)", reason.c_str(), ip_ap_name.c_str());

  if (ipConnectFast() || ipConnectCautious()) {
    ipOnConnect();
  }
  else {
    if (ip_reconnect) ipScheduleReconnect();
  }
  
  LEAF_BOOL_RETURN(ip_connected);
}


bool AbstractIpSimcomLeaf::ipConnectFast()
{
  int i;
  LEAF_ENTER(L_NOTICE);

  if (!modemWaitPortMutex(HERE)) {
    LEAF_ALERT("Could not acquire modem mutex");
    LEAF_BOOL_RETURN(false);
  }
  ip_connected = false;

  String response = modemQuery("AT",-1,HERE);
  if (response == "AT") {
    // modem is answering, but needs echo turned off
    modemSendCmd(HERE, "ATE0");
  }

  if (ip_abort_no_service) {
    //LEAF_INFO("Check Carrier status");
    if (!modemCarrierStatus()) {
      ACTION("LTE NO CARRIER");
      post_error(POST_ERROR_LTE_NOSERV, 0);
      modemReleasePortMutex(HERE);
      LEAF_BOOL_RETURN(false);
    }
  }
  
  if (ip_abort_no_signal) {
    //LEAF_INFO("Check signal strength");
    if (!modemSignalStatus()) {
      ACTION("LTE NO SIGNAL");
      post_error(POST_ERROR_LTE_NOSIG, 0);
      modemReleasePortMutex(HERE);
      LEAF_BOOL_RETURN(false);
    }
  }

  if (ip_enable_gps && ip_simultaneous_gps) {
    //LEAF_INFO("Check GPS state");
    if (!ipGPSPowerStatus()) {
      LEAF_NOTICE("GPS needs to be powered on");
      ipEnableGPS();
    }
  }
  
  if (ip_enable_sms) {
    modemSendCmd(HERE, "AT+CNMI=2,1");
  }

  //LEAF_INFO("Check for existing connection");
  bool has_connection = ipGetAddress();
  
  if (has_connection && ip_modem_reuse_connection) {
    LEAF_NOTICE("Got IP addr %s", ip_addr_str.c_str());
    ip_connected = true;
  }
  else {
    if (ipModemNeedsReboot()) {
      LEAF_WARN("IP Aborting connect until modem reboot completes");
      modemReleasePortMutex(HERE);
      LEAF_BOOL_RETURN(false);
    }

    if (has_connection && !ip_reuse_connection) {
      // modem was already connected, thought we thought it was not.
      // Disconnect it and reconnect to clear any weird app state
      LEAF_WARN("Modem was unexpectedly found connected.  Bounce session");
      ipLinkDown();
    }
    else {
      //LEAF_INFO("IP not up, try activating");
    }

    if (!ipLinkUp()) {
      LEAF_WARN("IP not ready (fast-mode, will fall back to cautious-mode)");
      modemReleasePortMutex(HERE);
      LEAF_BOOL_RETURN(false);
    }

    unsigned long timebox = millis()+20*modem_timeout_default;
    do {
      if (ipGetAddress()) {
	LEAF_INFO("Got IP addr %s", ip_addr_str.c_str());
	ip_connected = true;
      }
      else {
	if (ipModemNeedsReboot()) {
	  LEAF_WARN("IP Aborting connect until modem reboot completes");
	  modemReleasePortMutex(HERE);
	  LEAF_BOOL_RETURN(false);
	}

	delay(500);
      }
    } while (!ip_connected && (millis()<=timebox));

    if (!ip_connected) {
      LEAF_ALERT("Did not get IP");
      post_error(POST_ERROR_LTE_NOCONN,0);
      modemReleasePortMutex(HERE);
      LEAF_BOOL_RETURN(false);
    }
  }

  LEAF_NOTICE("Connection succeded (IP=%s)", ip_addr_str.c_str());
  modemReleasePortMutex(HERE);
  LEAF_BOOL_RETURN_SLOW(5000, true);
}

bool AbstractIpSimcomLeaf::ipModemConfigure() 
{
  if (!AbstractIpLTELeaf::ipModemConfigure()) {
    // parent says no
    return false;
  }

  LEAF_ENTER(L_NOTICE);
  //LEAF_INFO("Check functionality");
  if (!modemWaitPortMutex(HERE)) {
    LEAF_ALERT("Could not acquire modem mutex");
    LEAF_BOOL_RETURN(false);
  }

  int i;
  if (!modemSendExpectInt("AT+CFUN?","+CFUN: ", &i, modem_timeout_default*10,HERE)) {
    LEAF_ALERT("Modem is not answering commands");
    modemReleasePortMutex(HERE);
    if (!modemProbe(HERE, MODEM_PROBE_QUICK)) {
      LEAF_ALERT("Aborting configuration, modem non responsive");
      ipModemSetNeedsReboot();
      LEAF_BOOL_RETURN(false);
    }
    if (!modemWaitPortMutex(HERE)) {
      LEAF_ALERT("Could not reqacquire modem mutex");
      LEAF_BOOL_RETURN(false);
    }
  }

  //LEAF_INFO("Set functionality mode 1 (full)");
  if (!modemSendCmd(HERE, "AT+CFUN=1")) {
    LEAF_ALERT("Modem would not activate");
    modemReleasePortMutex(HERE);
    LEAF_BOOL_RETURN(false);
  }


  //
  // Set the value of a bunch of setttings
  //
  static const char *cmds[][2] = {
    {"E0", "command echo suppressed"},
    {"+CMEE=2", "verbose errors enabled"},
    {"+CMNB=1", "LTE category M1"},
    {"+CBANDCFG=\"CAT-M\",28", "LTE M1 band 28"},
    {"+CNMP=2", "LTE mode 2, GPRS/LTE auto"},
    {"+CLTS=1", "Real time clock enabled"},
    //{"+CIPMUX=1", "Multi-stream IP mode"},
    //{"+CIPQSEND=1", "Quick-send IP mode"},
    {"+CMGF=1", "SMS Text-mode"},
    {"+CSDF=1", "SMS verbose headers"},
    {"+CSCLK=0", "disable slow clock (sleep) mode"},
    {NULL, NULL}
  };

  for (i=0; cmds[i][0] != NULL; i++) {
    //LEAF_INFO("Set %s using AT%s", cmds[i][1], cmds[i][0]);
    if (!modemSendCmd(HERE, "AT%s", cmds[i][0]))  {
      LEAF_ALERT("Modem did not respond to command %s (%s)", cmds[i][0], cmds[i][1]);
    }
  }

  //LEAF_INFO("Set network APN to [%s]", ip_ap_name);
  if (!ipSetApName(ip_ap_name)) {
    LEAF_ALERT("Modem did not accept AP name [%s]", ip_ap_name);
  }
  
  //
  // Confirm the expected value of a bunch of setttings
  //
  static const char *queries[][2] = {
    //{"CMEE?", "errors reporting mode"},
    //{"CGMM", "Module name"},
    {"CGMR", "Firmware version"},
    //{"CGSN", "IMEI serial number"},
    //{"CNUM", "Phone number"},
    //{"CLTS?", "Clock mode"},
    {"CCLK?", "Clock"},
    //{"CSCLK?", "Slow Clock (sleep) mode status"},
    //{"COPS?", "Operator status"},
    {"CSQ", "Signal strength"},
    {"CPSI?", "Signal info"},
    //{"CBAND?", "Radio band"},
    {"CMNB?", "LTE Mode"},
    //{"CREG?", "Registration status"},
    //{"CGREG?", "GSM Registration status"},
    //{"CGATT?", "Network attach status"},
    //{"CGACT?", "PDP context state"},
    //{"CGPADDR", "PDP address"},
    //{"CGDCONT?", "Network settings"},
    //{"CGNAPN", "NB-iot status"},
    //{"CGNSINF", "GPS fix status"},
    //{"CIPSTART?", "Available IP slots"},
    //{"CIPSTATUS=0", "IP slot 0 status"},
    //{"CIPSTATUS=1", "IP slot 1 status"},
    //{"CIPSTATUS=2", "IP slot 2 status"},
    //{"CIPSTATUS=3", "IP slot 3 status"},
    // {"COPS=?", "Available cell operators"}, // takes approx 2 mins
    {NULL,NULL}
  };

  
  String result;
  char cmd[32];
  
  for (i=0; queries[i][0] != NULL; i++) {
    snprintf(cmd, sizeof(cmd), "AT+%s", queries[i][0]);
    result = modemQuery(cmd,"",-1,HERE);
    LEAF_NOTICE("Check %s with >[%s]: <[%s]", queries[i][1], cmd, result.c_str());
    if (result == "") {
      LEAF_ALERT("Modem did not answer status query %s (%s)", queries[i][0], queries[i][1]);
    }
  }
  
  // check sim status

  //LEAF_INFO("Check Carrier status");
  String sim_status = modemQuery("AT+CPIN?",-1,HERE);
  if (!sim_status) {
    LEAF_ALERT("SIM status not available");
    post_error(POST_ERROR_LTE_NOSIM, 0);
    modemReleasePortMutex(HERE);
    LEAF_BOOL_RETURN(false);
  }
  else {
    //LEAF_INFO("SIM status: %s", sim_status.c_str());
  }
  if (strstr(modem_response_buf, "ERROR")) {
    LEAF_ALERT("SIM ERROR: %s", modem_response_buf);
    post_error(POST_ERROR_LTE, 3);
    post_error(POST_ERROR_LTE_NOSIM, 0);
    ERROR("NO SIM");
    modemReleasePortMutex(HERE);
    LEAF_BOOL_RETURN(false);
  }

  LEAF_NOTICE("Set modem NETLIGHT modes");
  modemSendCmd(HERE, "AT+CNETLIGHT=1");
  modemSendCmd(HERE, "AT+SLEDS=%d,%d,%d", 1, 40, 460);// 1=offline on/off, mode, timer_on, timer_off
  modemSendCmd(HERE, "AT+SLEDS=%d,%d,%d", 2, 40, 9960); // 2=online on/off, mode, timer_on, timer_off
  modemSendCmd(HERE, "AT+SLEDS=%d,%d,%d", 3, 40, 9960); // 3=PPP on/off, mode, timer_on, timer_off

  modemReleasePortMutex(HERE);
  LEAF_NOTICE("Modem is configured for use");
  LEAF_BOOL_RETURN(true);
}

bool AbstractIpSimcomLeaf::ipConnectCautious()
{
  LEAF_ENTER(L_NOTICE);

  ip_connected = false;
  if (wake_reason == "poweron") {
    if (!ipModemConfigure()) {
      LEAF_BOOL_RETURN(false);
    }
  }
  
  if (!modemWaitPortMutex(HERE)) {
    LEAF_ALERT("Could not acquire modem mutex");
    LEAF_BOOL_RETURN(false);
  }

  if (ip_enable_gps && ip_simultaneous_gps) {
    //LEAF_INFO("Check GPS state");
    if (!ipGPSPowerStatus()) {
      LEAF_NOTICE("GPS needs to be powered on");
      ipEnableGPS();
    }
    if (ip_gps_active) {
      LEAF_NOTICE("Polling GPS for initial fix during cautious connect");
      ipPollGPS();
    }
  }
  
  if (ip_enable_sms) {
    modemSendCmd(HERE, "AT+CNMI=2,1"); // will send +CMTI on SMS Recv
  }

  if (ipGetAddress()) {
    LEAF_NOTICE("Wireless connection is already established (set connected=true)");
    ip_connected = true;
  }
  else {
    if (ipModemNeedsReboot()) {
      LEAF_WARN("IP Aborting connect until modem reboot completes");
      modemReleasePortMutex(HERE);
      LEAF_BOOL_RETURN(false);
    }

    //LEAF_INFO("Opening wireless connection");
    ipLinkUp();
    if (ipGetAddress()) {
      ip_connected = true;
      //LEAF_INFO("Wireless connection is (now) established (connected=true)");
    }
    else if (ipModemNeedsReboot()) {
      LEAF_WARN("IP Aborting connect until modem reboot completes");
      modemReleasePortMutex(HERE);
      LEAF_BOOL_RETURN(false);
    }
  }

  if (!ip_connected) {
    if (ip_abort_no_service && !modemCarrierStatus()) {
      LEAF_ALERT("NO LTE CARRIER");
      post_error(POST_ERROR_LTE, 3);
      post_error(POST_ERROR_LTE_NOSERV, 0);
      ERROR("NO SERVICE");
      modemReleasePortMutex(HERE);
      LEAF_BOOL_RETURN(false);
    }

    //LEAF_INFO("Check signal strength");
    if (ip_abort_no_signal && !modemSignalStatus()) {
      LEAF_ALERT("NO LTE SIGNAL");
      
      //ipModemReboot(HERE);
      post_error(POST_ERROR_LTE, 3);
      post_error(POST_ERROR_LTE_NOSIG, 0);
      ERROR("NO SIGNAL");
      modemReleasePortMutex(HERE);
      LEAF_BOOL_RETURN(false);
    }

#if 0
    //LEAF_INFO("Check task status");
    String response = modemQuery("AT+CSTT?","+CSTT: ",-1,HERE);
    if (response.indexOf(ip_ap_name) < 0) { // no_apn
      //LEAF_INFO("Start task");
      if (!modemSendCmd(HERE, "AT+CSTT=\"%s\"", ip_ap_name)) {
	LEAF_NOTICE("Modem LTE is not cooperating.  Retry later.");
	//ipModemReboot(HERE);
	post_error(POST_ERROR_LTE, 3);
	post_error(POST_ERROR_LTE_NOCONN, 0);
	ERROR("IP fail");
	modemReleasePortMutex(HERE);
	LEAF_BOOL_RETURN(false);
      }
    }

    //LEAF_INFO("Enable IP");
    modemSendCmd(HERE, "AT+CIICR");
    //LEAF_INFO("Wait for IP address");
    response = modemSendCmd(HERE, "AT+CIFSR");

    unsigned long timebox = millis()+20*modem_timeout_default;
    do {
      if (ipGetAddress()) {
	ip_connected = true;
      }
      else {
	if (ipModemNeedsReboot()) {
	  LEAF_WARN("IP Aborting connect until modem reboot completes");
	  modemReleasePortMutex(HERE);
	  LEAF_BOOL_RETURN(false);
	}

	delay(1000);
      }
    } while (!ip_connected && (millis()<=timebox));
    if (!ip_connected) {

      LEAF_NOTICE("Modem Connection is not cooperating.  Reset modem and retry later.");
      ipModemReboot(HERE);
      post_error(POST_ERROR_LTE, 3);
      post_error(POST_ERROR_LTE_NOCONN, 0);
      ERROR("LTE Connect fail");
      modemReleasePortMutex(HERE);
      LEAF_BOOL_RETURN(false);
    }
#endif
      modemReleasePortMutex(HERE);
      LEAF_BOOL_RETURN(false);
  }
  
  LEAF_NOTICE("Connection complete (IP=%s)", ip_addr_str.c_str());
  ip_connect_time = millis();

  modemReleasePortMutex(HERE);
  LEAF_BOOL_RETURN(true);
}


bool AbstractIpSimcomLeaf::modemInstallCert()
{
  const char *cacert =
    "-----BEGIN CERTIFICATE-----\r\n"
    "MIICdzCCAeCgAwIBAgIJAK3TxzrHW8SsMA0GCSqGSIb3DQEBCwUAMFMxCzAJBgNV\r\n"
    "BAYTAkFVMRMwEQYDVQQIDApxdWVlbnNsYW5kMRwwGgYDVQQKDBNTZW5zYXZhdGlv\r\n"
    "biBQdHkgTHRkMREwDwYDVQQDDAhzZW5zYWh1YjAeFw0yMDAzMjEyMzMzMTJaFw0y\r\n"
    "NTAzMjAyMzMzMTJaMFMxCzAJBgNVBAYTAkFVMRMwEQYDVQQIDApxdWVlbnNsYW5k\r\n"
    "MRwwGgYDVQQKDBNTZW5zYXZhdGlvbiBQdHkgTHRkMREwDwYDVQQDDAhzZW5zYWh1\r\n"
    "YjCBnzANBgkqhkiG9w0BAQEFAAOBjQAwgYkCgYEAtAag8LBkdk+QmMJWxT/yDtqH\r\n"
    "iwFKIpIoz4PwFlPHi1bisRM1VB3IajU/bhMLc8AdhSIhG6GuSo4abfesYsFdEmTd\r\n"
    "Z0es5TTDNZWj+dPOLEBDkKyi4RDrRmzh/N8axZ3Yhoc/k4QuzGhnUKOvA6z07Sg5\r\n"
    "XsNUfIYGatxPl8JYSScCAwEAAaNTMFEwHQYDVR0OBBYEFD7Ad200vd05FMewsGsW\r\n"
    "WJy09X+dMB8GA1UdIwQYMBaAFD7Ad200vd05FMewsGsWWJy09X+dMA8GA1UdEwEB\r\n"
    "/wQFMAMBAf8wDQYJKoZIhvcNAQELBQADgYEAOad6RxxSFaG8heBXY/0/WNLudt/W\r\n"
    "WLeigKMPXZmY72Y4U8/FQzxDj4bP+AOE+xoVVFcmZURmX3V80g+ti6Y/d9QFDQ+t\r\n"
    "YsHyzwrWsMusM5sRlmfrxlExrRjw1oEwdLefAM8L5WDEuhCdXrLxwFjUK2TcJ9u0\r\n"
    "rQ09npAQ1MgeaRo=\r\n"
    "-----END CERTIFICATE-----\r\n";

  if (modemWriteFileVerify("cacert.pem", cacert)) {
    LEAF_ALERT("CA cert write failed");
  }

  const char *clientcert =
    "-----BEGIN CERTIFICATE-----\r\n"
    "MIIC+zCCAmSgAwIBAgICEAAwDQYJKoZIhvcNAQELBQAwUzELMAkGA1UEBhMCQVUx\r\n"
    "EzARBgNVBAgMCnF1ZWVuc2xhbmQxHDAaBgNVBAoME1NlbnNhdmF0aW9uIFB0eSBM\r\n"
    "dGQxETAPBgNVBAMMCHNlbnNhaHViMB4XDTIwMDcxNzAwMDAxMloXDTIxMDcyNzAw\r\n"
    "MDAxMlowazELMAkGA1UEBhMCQVUxEzARBgNVBAgMClF1ZWVuc2xhbmQxDzANBgNV\r\n"
    "BAcMBlN1bW5lcjEUMBIGA1UECgwLQWNjZWxlcmFuZG8xDDAKBgNVBAsMA1BVQzES\r\n"
    "MBAGA1UEAwwJcHVjMDAwMDAxMIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQDW\r\n"
    "siRHo4hVgYz6EEINbWraXnouvKJ5qTb+xARdOIsCnZxx4A2nEf//VXUhz+uAffpo\r\n"
    "+p3YtQ42wG/j0G0uWxOqgGjGom6KhF7Bt4n8AtSJeoDfZV1imGsY+mL+PqsLjJhx\r\n"
    "85gnhFgC4ii38V9bwQU7WjTSO/1TfHw+vFjVd0AkDwIDAQABo4HFMIHCMAkGA1Ud\r\n"
    "EwQCMAAwEQYJYIZIAYb4QgEBBAQDAgWgMDMGCWCGSAGG+EIBDQQmFiRPcGVuU1NM\r\n"
    "IEdlbmVyYXRlZCBDbGllbnQgQ2VydGlmaWNhdGUwHQYDVR0OBBYEFN9SYA+m3ZlF\r\n"
    "eR81YxXf9CbNOFw4MB8GA1UdIwQYMBaAFD7Ad200vd05FMewsGsWWJy09X+dMA4G\r\n"
    "A1UdDwEB/wQEAwIF4DAdBgNVHSUEFjAUBggrBgEFBQcDAgYIKwYBBQUHAwQwDQYJ\r\n"
    "KoZIhvcNAQELBQADgYEAqIOpCct75VymZctHb98U1leUdG1eHYubP0YLc9ae3Sph\r\n"
    "T6R7JnwFynSFgXeAbTzC57O1onQwNq1fJ+LsyaeTWmxEb3PeVzoVqitO92Pw/kgj\r\n"
    "IXrFxaaEYEBIPsk3Ez5KUEhQicH8Y1zyneAAvxzKhSRwoJZ1YPfeGnp7SvTlNC0=\r\n"
    "-----END CERTIFICATE-----\r\n";


  if (modemWriteFileVerify("client.crt", clientcert)) {
    LEAF_ALERT("Client cert write failed");
  }

  const char *clientkey =
    "-----BEGIN RSA PRIVATE KEY-----\r\n"
    "MIICXQIBAAKBgQDWsiRHo4hVgYz6EEINbWraXnouvKJ5qTb+xARdOIsCnZxx4A2n\r\n"
    "Ef//VXUhz+uAffpo+p3YtQ42wG/j0G0uWxOqgGjGom6KhF7Bt4n8AtSJeoDfZV1i\r\n"
    "mGsY+mL+PqsLjJhx85gnhFgC4ii38V9bwQU7WjTSO/1TfHw+vFjVd0AkDwIDAQAB\r\n"
    "AoGBALZoehyHm3CSdjWLlKMV4KARfxuwVxaopzoDTnXpcWnSgTXbF55n06mbcL4+\r\n"
    "iicMYbHJpEyXX7EzBJ142xp0dRpr51mOCF9pLtLsDSOslA87X74pffnY6boMvW2+\r\n"
    "Tiou1AP5XXlemTmKiT3vMLno+JKfxqu+DhLyCdV5zHyeyw4hAkEA9PjtgN6Xaru0\r\n"
    "BFdBYlURllGq2J11ZioM1HlhNUX1UA6WR7jC6ZLXxFSbrZkLKgInuwiJxUn6j2mb\r\n"
    "/ZypzrOo8QJBAOBcTmHlqTWSK6r32A6+1WVN1QdSU7Nif/lIAUG+Y4XBMij3mJgX\r\n"
    "decI/qGQI/6P3LhSErbUOZVlsHh7zUzYnP8CQQDp6mRHIMUu+qrrVjIt5hMUGUls\r\n"
    "6/W1J0P3AywqRXH4DuW6+JbNmBUF+NBqlG/Pnh03//Al/f0OQgbcxWJz6KPRAkB+\r\n"
    "M23jo0uK1q25fbAKm01trlolxClQvhc+IUKTuIRCuGl+occzxf6L9oNEXc/hYQrG\r\n"
    "o2Pjc3zwjEK3guv4TeABAkBXEi5Vair5yvU+3dV3+21tbnWnDM5UXmwh4PRgyoHQ\r\n"
    "ifrMHbpTscUNv+3Alc9gJJrUhZO4MxnebIRmKn2DzO87\r\n"
    "-----END RSA PRIVATE KEY-----\r\n";

  if (modemWriteFileVerify("client.key", clientkey)) {
    LEAF_ALERT("Client key write failed");
  }

  return false;
}

bool AbstractIpSimcomLeaf::modemProcessURC(String Message) 
{
  LEAF_ENTER(L_INFO);
  bool result = false;

  //LEAF_INFO("AbstractIpSimcomLeaf::modemProcessURC [%s]", Message.c_str());
  if (canRun() && Message.startsWith("+SMSUB: ")) {
    // Chop off the "SMSUB: " part plus the begininng quote
    // After this, Message should be: "topic_name","message"
    Message = Message.substring(8);
    //LEAF_INFO("Parsing SMSUB input [%s]", Message.c_str());
    
    int idx = Message.indexOf("\",\""); // Search for second quote
    if (idx > 0) {
      // Found a comma separating topic and payload
      String Topic = Message.substring(1, idx); // Grab only the text (without quotes)
      String Payload = Message.substring(idx+3, Message.length()-1);
      
      //last_external_input = millis();
      LEAF_NOTICE("Received MQTT Message Topic=[%s] Payload=[%s]", Topic.c_str(), Payload.c_str());
      ACTION("PUBSUB recv");
      if (stacx_comms_state != ONLINE) {
	LEAF_WARN("Recording Pubsub as online since we are receiving subscriptions");
	ipCommsState(ONLINE, HERE);
      }
      ipCommsState(TRANSACTION, HERE);
      pubsubLeaf->_mqtt_route(Topic, Payload);
      ipCommsState(REVERT, HERE);
    }
    else {
      LEAF_ALERT("Payload separator not found in MQTT SMSUB input: [%s]", Message.c_str());
    }
    result = true;
  }
  else if (canRun() && (Message == "+SMSTATE: 0")) {
    LEAF_ALERT("Lost MQTT connection");
    pubsubLeaf->pubsubDisconnect(false);
    result = true;
  }
  else if (Message == "UNDER-VOLTAGE WARNNING") { // yes the modem misspells it!
    LEAF_NOTICE("Modem reports low voltage");
    result = true;
  }
  else {
    result = AbstractIpLTELeaf::modemProcessURC(Message);
  }
  LEAF_BOOL_RETURN(result);
}

void AbstractIpSimcomLeaf::pre_sleep(int duration)
{
  LEAF_ENTER(L_NOTICE);
  if (!modemIsPresent()) {
    LEAF_VOID_RETURN;
  }

  // Disconnect IP, then maybe the modem itself
  if (isConnected(HERE)) {
    ipDisconnect();
    //ipOnDisconnect();
  }

  LEAF_NOTICE("Putting LTE modem to lower power state");
  if (ip_modem_use_poweroff) {
    LEAF_NOTICE("Powering off modem");
    ACTION("MODEM off");
    modemSetPower(false);
  }
  else if (ip_modem_use_sleep) {
    if (pin_sleep >= 0) {
      LEAF_NOTICE("Telling modem to allow sleep via DTR pin");
      ACTION("MODEM sleep");
      modemSendCmd(HERE, "AT+CSCLK=1");
      modemSetSleep(true);
    }
    else {
      LEAF_NOTICE("Putting modem to soft-off");
      ipModemPowerOff(HERE);
    }
  }
  LEAF_VOID_RETURN;
}

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
