
// Redirect adafruit fona debug to accelerando trace.
// (The string-to-cstr hack handles flash-memory strings)
#define DEBUG_PRINT(x) __DEBUGINLINE__(L_INFO,"%s",String(x).c_str())
#define DEBUG_PRINTLN(x) {__DEBUGINLINE__(L_INFO,"%s\n",String(x).c_str());inline_fresh=true;}
#define DEBUG_PRINTF(...) __DEBUGINLINE__(L_INFO,__VA_ARGS__)

#include "Adafruit_FONA.h" // https://github.com/botletics/SIM7000-LTE-Shield/tree/master/Code

typedef std::function<bool(int,size_t,const uint8_t *)> Sim7000HttpHeaderCallback;
typedef std::function<bool(const uint8_t *,size_t)> Sim7000HttpDataCallback;

class Sim7000Modem : public Adafruit_FONA_LTE
{
public:
  static const bool MULTILINE=true;
  static const bool TRACE=true;
  static const bool NOTRACE=false;

  const char *get_reply_buffer()
  {
    return replybuffer;
  }


  void send(const char *send) {
    flushInput();
    DEBUG_PRINT(F("\t---> ")); DEBUG_PRINTLN(send);
    mySerial->println(send);
  }

  size_t write(const uint8_t *buf, size_t size) {
    DEBUG_PRINTF("\t--->[binary %d]\n", (int)size);
    return mySerial->write(buf, size);
  }

  int readToBuffer(cbuf *buf, size_t size)
  {
    int got = 0;
    while (size && (!buf->full())) {
      if (mySerial->available()) {
	char c = mySerial->read();
	buf->write(c);
	got++;
	size--;
      }
    }
    return got;
  }

  void chat(bool echo = false)
  {
    char c = '\0';
    char prev = '\0';
    bool chat = true;
    // Enter a loop relaying data from console to modem (for testing the
    // modem)
    Serial.println("Modem chat session, type '<LF> .' to exit");
    while (chat) {
      if (Serial.available()) {
	prev = c;
	c = Serial.read();
	if ((prev =='\n') && (c=='.')) {
	  chat = false;
	  Serial.println("Modem chat session terminated");
	  break;
	}
	if (echo) Serial.write(c);
	mySerial->write(c);
      }
      if (mySerial->available()) {
	char d = mySerial->read();
	Serial.write(d);
      }
    }
  }


  bool waitfor(const char *send, fona_timeout_t timeout, char *buf, int bufmax)
  {
    unsigned long start = millis();
    unsigned long now;

    while ((now=millis()) < (start+timeout)) {
      if (sendCheckReply(send, "ERROR", timeout*1000)) {
	delay(1000);
      } else {
	strlcpy(buf, replybuffer, bufmax);
	return true;
      }
    }
    return false;
  }

  //
  // Parse an AT command response that contains a value (eg +CFSGFIS: 932)
  //
  bool expectReplyWithInt(const char *expected, int *r_int, int wait=5000, int resp_max=80, int lines = 1) {
    unsigned long start = millis();
    unsigned long waitUntil = start + wait;
    char resp[resp_max];
    int p=0;
    resp[p] = '\0';

    while (1) {
      unsigned long now = millis();
      if ( ( (waitUntil < start) && (now < start) && (now > waitUntil) ) ||
	   ( (waitUntil > start) && (now > waitUntil) )
	) {
	DEBUG_PRINTLN("<--x Timeout"); // fixme: breaks at wrap
	break;
      }

      if (!available()) {
	//DEBUG_PRINTLN(F("...snooze"));
	delay(10);
      }
      else {
	char c = read();
	// ignore a CRLF at start of input
	if (c == '\r') continue;
	if ((c == '\n') && (p==0)) continue;
	resp[p++] = c;
	resp[p]='\0';
	//DEBUG_PRINT(F("read char: ["));
	//DEBUG_PRINT(c);
	//DEBUG_PRINTLN(F("]"));

	if (p >= (sizeof(resp)-1)) {
	  DEBUG_PRINTLN("<--x Overflow");
	  break;
	}
	if (strncmp(expected, resp, strlen(expected)) == 0) {
	  //DEBUG_PRINTLN(F("found"));
	  break;
	}
	if (c=='\n') {
	  DEBUG_PRINTF("<--x EOL met after %s\n",resp);
	  if (lines > 1) {
	    // discard this line and look for another
	    p=0;
	    resp[p]='\0';
	    lines--;
	    continue;
	  }
	  else {
	    break;
	  }
	}
      }
    }
    DEBUG_PRINT(F("\t<--- ")); DEBUG_PRINTLN(resp);

    if (strncmp(resp, expected, sizeof(resp)) != 0) {
      expectReply(F(""),100); // eat up rest of line
      DEBUG_PRINTF("<--x Got %s but expected %s\n", resp, expected);
      return false;
    }
    if (resp) {
      // use parseInt from the stream superclass
      *r_int = parseInt();
      DEBUG_PRINTF("\t<--- %s\n", resp);
    }
    else {
      DEBUG_PRINTLN();
    }

    return true;
  }

  bool sendExpectIntReply(const char *send, const char *expected, int *r_int, int wait=5000, int resp_max=80, bool multiline=false, bool trace=false) {
    unsigned long start = millis();
    unsigned long waitUntil = start + wait;
    char resp[resp_max];
    int p=0;
    resp[p] = '\0';

    // Write the command (if any)
    if (send) {
      flushInput();
      DEBUG_PRINT(F("\t---> ")); DEBUG_PRINTLN(send);
      mySerial->println(send);
    }

    // Now parse a response
    while (1) {
      unsigned long now = millis();
      if ( ( (waitUntil > start) && (now > waitUntil) ) // normal case
	     ||
	   ( (waitUntil < start) && (now < start) && (now > waitUntil) ) // edge-case where millis wraps
	) {
	DEBUG_PRINTLN("<--x Timeout");
	break;
      }

      if (!available()) {
	if (trace) {
	  DEBUG_PRINTLN(F("...snooze"));
	}

	delay(100);
      }
      else {
	char c = read();
	// ignore a CRLF at start of input
	if (c == '\r') continue;
	if ((c == '\n') && (p==0)) continue;
	resp[p++] = c;
	resp[p]='\0';
	if (trace) {
	  DEBUG_PRINT(F("read char: ["));
	  DEBUG_PRINT(c);
	  DEBUG_PRINTLN(F("]"));
	}

	if (p >= (sizeof(resp)-1)) {
	  DEBUG_PRINTLN(F("\t<--x Overflow"));
	  break;
	}
	if (strncmp(expected, resp, strlen(expected)) == 0) {
	  if (trace) {
	    DEBUG_PRINTLN(F("found expected response"));
	  }
	  break;
	}
	if (c=='\n') {
	  if (!multiline) {
	    DEBUG_PRINT(F("\t<--x EOL met after "));
	    DEBUG_PRINTLN(resp);
	    break;
	  }
	  else {
	    DEBUG_PRINT(F("\t<--x discard unwanted line "));
	    DEBUG_PRINTLN(resp);
	    if (strstr(resp, "ERROR") != NULL) {
	      DEBUG_PRINTLN("Error response");
	      break;
	    }
	    p=0;
	  }
	}
      }
    }
    DEBUG_PRINT(F("\t<--- ")); DEBUG_PRINTLN(resp);

    if (strncmp(resp, expected, sizeof(resp)) != 0) {
      expectReply(F(""),100); // eat up rest of line
      DEBUG_PRINT("<--x Got ");
      DEBUG_PRINT(resp);
      DEBUG_PRINT(" but expected ");
      DEBUG_PRINTLN(expected);
      return false;
    }
    if (r_int) {
      *r_int = parseInt();
      DEBUG_PRINT(F("\t<--- "));
      DEBUG_PRINTLN(*r_int);
    }
    else {
      DEBUG_PRINTLN();
    }

    return true;
  }

  bool sendExpectIntPairReply(const char *send, const char *expected, int *r_int1, int *r_int2, int wait=5000, int resp_max=80, int multiline=false, bool trace=false) {
    unsigned long start = millis();
    unsigned long waitUntil = start + wait;
    char resp[resp_max];
    int p=0;
    resp[p] = '\0';

    // Write the command (if any)
    if (send != NULL) {
      flushInput();
      DEBUG_PRINT(F("\t---> ")); DEBUG_PRINTLN(send);
      mySerial->println(send);
    }


    // Now parse a response
    while (1) {
      unsigned long now = millis();
      if ( ( (waitUntil > start) && (now > waitUntil) ) // normal case
	     ||
	   ( (waitUntil < start) && (now < start) && (now > waitUntil) ) // edge-case where millis wraps
	) {
	DEBUG_PRINTLN("<--x Timeout");
	break;
      }

      if (!available()) {
	if (trace) {
	  //DEBUG_PRINTLN(F("...snooze"));
	}
	delay(10);
      }
      else {
	char c = read();
	// ignore a CRLF at start of input
	if (c == '\r') continue;
	if ((c == '\n') && (p==0)) continue;
	resp[p++] = c;
	resp[p]='\0';
	if (trace) {
	  DEBUG_PRINT(F("read char: ["));
	  DEBUG_PRINT(c);
	  DEBUG_PRINTLN(F("]"));
	}

	if (p >= (sizeof(resp)-1)) {
	  DEBUG_PRINTLN(F("<--x Overflow"));
	  break;
	}
	if (strncmp(expected, resp, strlen(expected)) == 0) {
	  if (trace) {
	    DEBUG_PRINTLN(F("found"));
	  }
	  break;
	}
	if (c=='\n') {
	  if (!multiline) {
	    DEBUG_PRINT(F("<--x EOL met after "));
	    DEBUG_PRINTLN(resp);
	    break;
	  }
	  else {
	    DEBUG_PRINT(F("<--x discard unwanted line"));
	    DEBUG_PRINTLN(resp);
	    if (strstr(resp, "ERROR") != NULL) {
	      DEBUG_PRINTLN("Error response");
	      break;
	    }
	    p=0;
	  }
	}
      }
    }
    DEBUG_PRINT(F("\t<--- got marker: ")); DEBUG_PRINTLN(resp);

    if (strncmp(resp, expected, sizeof(resp)) != 0) {
      expectReply(F(""),100); // eat up rest of line
      DEBUG_PRINT("<--x Got ");
      DEBUG_PRINT(resp);
      DEBUG_PRINT(" but expected ");
      DEBUG_PRINTLN(expected);
      return false;
    }
    if (r_int1) {
      *r_int1 = parseInt();
      DEBUG_PRINT(F("\t<--- value A:  "));
      DEBUG_PRINTLN(*r_int1);
      mySerial->read();
      if (r_int2) {
	*r_int2 = parseInt(',');
	DEBUG_PRINT(F("\t<--- value B:  "));
	DEBUG_PRINTLN(*r_int2);
      }
    }
    else {
      DEBUG_PRINTLN();
    }

    return true;
  }

  bool sendExpectStringReply(const char *send, const char *expected, char *resp, int wait=5000, int resp_max=80, int lines=1) {
    unsigned long start = millis();
    unsigned long waitUntil = start + wait;
    int p=0;
    resp[p] = '\0';

    // Write the command
    if (send && strlen(send)) {
      flushInput();
      DEBUG_PRINT(F("\t---> ")); DEBUG_PRINTLN(send);
      mySerial->println(send);
    }

    // Now parse a response
    while (1) {
      unsigned long now = millis();
      if ( ( (waitUntil > start) && (now > waitUntil) ) // normal case
	     ||
	   ( (waitUntil < start) && (now < start) && (now > waitUntil) ) // edge-case where millis wraps
	) {
	DEBUG_PRINTLN("<--x Timeout");
	break;
      }

      if (!available()) {
	//DEBUG_PRINTLN(F("...snooze"));
	delay(10);
      }
      else {
	char c = read();
	// ignore a CRLF at start of input
	if (c == '\r') continue;
	if ((c == '\n') && (p==0)) continue;
	resp[p++] = c;
	resp[p]='\0';
	//DEBUG_PRINT(F("read char: ["));
	//DEBUG_PRINT(c);
	//DEBUG_PRINTLN(F("]"));

	if (p >= (resp_max-1)) {
	  DEBUG_PRINTLN(F("<--x Overflow"));
	  break;
	}

	if (((strncmp(expected, resp, strlen(expected))) == 0) && (c=='\n')) {
	  //DEBUG_PRINTLN(F("found"));
	  break;
	}
	if (c=='\n') {
	  DEBUG_PRINTLN(resp);
	  if (lines > 1) {
	    // we were warned to expect multiple lines, discard this line and look for another
	    if (strstr(resp, "ERROR") != NULL) {
	      DEBUG_PRINTLN("Error response");
	      break;
	    }
	    p=0;
	    resp[p]='\0';
	    lines--;
	    continue;
	  }
	  else {
	    DEBUG_PRINT(F("<--x EOL met after "));
	    break;
	  }
	}
      }
    }
    DEBUG_PRINT(F("\t<--- ")); DEBUG_PRINTLN(resp);

    flushInput();

    if (strncmp(resp, expected, strlen(expected)) != 0) {
      expectReply(F(""),100); // eat up rest of line
      DEBUG_PRINT("<--x Got ");
      DEBUG_PRINT(resp);
      DEBUG_PRINT(" but expected ");
      DEBUG_PRINTLN(expected);
      return false;
    }
    memmove(resp, resp+strlen(expected), strlen(resp)-strlen(expected)+1);

    while (int l = strlen(resp)) {
      // Trim trailing whitespace
      if ((resp[l-1]=='\r') || (resp[l-1]=='\n')) {
	resp[l-1] = '\0';
      }
      else {
	break;
      }
    }
    DEBUG_PRINT(F("\t<--- "));
    DEBUG_PRINTLN(resp);

    return true;
  }

  int getReplyOfSize(char *resp, int size, int wait=5000, bool echo=false, bool trace=false) {
    unsigned long start = millis();
    unsigned long waitUntil = start + wait;
    int p=0;
#if 0    
    while (p<size) {
      if (!available()) {
	if (trace) {
	  DEBUG_PRINTF("getReplyOfSize: stalled, (got %d of %d so far)\n", p, size);
	}

	unsigned long now = millis();
	if ( now >= waitUntil) {
	  DEBUG_PRINTLN(F("Timeout"));
	  break;
	}
      }
      else {      
	char c = read();
	resp[p] = c;
	resp[p+1]='\0';
	if (echo) {
	  DEBUG_PRINTF("%c", c);
	}
	p += 1;
      }
    }
#else
    mySerial->setTimeout(wait);
    p = mySerial->readBytes(resp, size);
    resp[p]='\0';
#endif    
    if (echo) {
      DEBUG_PRINTF("\ngetReplyOfSize got %d/%d bytes\n%s\n", p,size,resp);
    }
    return (p >= size);
  }

  bool writeFile(const char *filename, const char *contents, int size=-1, int area=3) {
    int free = 0;
    int reads = 0;
    static char cmd[80]="";
    if (size < 0) size = strlen(contents);

    if (size > 10240) {
      DEBUG_PRINTLN(F("File too large (10240 bytes max)"));
      return false;
    }
    if (!sendExpectIntReply("AT+CFSGFRS?","+CFSGFRS: ", &free,10000)) {
      DEBUG_PRINTLN(F("Cannot get filesystem size"));
      return false;
    }
    if (free < size) {
      DEBUG_PRINTLN(F("Insufficient free space"));
      return false;
    }

    if (!sendCheckReply("AT+CFSINIT","OK")) {
      DEBUG_PRINTLN(F("Write prepare command not accepted"));

      if (sendCheckReply("AT+CFSTERM","OK")) {
	DEBUG_PRINTLN(F("Closed out a dangling session, retry write prepare"));
	if (!sendCheckReply("AT+CFSINIT","OK")) {
	  DEBUG_PRINTLN(F("Write prepare command still not accepted"));
	  return false;
	}
	// fall thru
      }
      else {
	return false;
      }
    }

    DEBUG_PRINTLN(F("Write to flash filesystem (cmd)"));
    snprintf(cmd, sizeof(cmd), "AT+CFSWFILE=%d,\"%s\",0,%d,10000", area, filename, size);
    if (!sendCheckReply(cmd, F("DOWNLOAD"))) {
      DEBUG_PRINTLN(F("Download command not accepted"));
      return false;
    }
    send(contents); // fixme: does not handle binary


    if (!expectReply(F("OK"))) {
      DEBUG_PRINTLN(F("Download failed"));
      return false;
    }

    if (!sendCheckReply("AT+CFSTERM","OK",5000)) {
      DEBUG_PRINTLN(F("Write commit failed"));
      return false;
    }

    return true;
  }


  bool readFile(const char *filename, char *buf, int buf_size, int area=3) {
    int size = 0;
    static char cmd[80]="";

    if (!sendCheckReply("AT+CFSINIT","OK")) {
      DEBUG_PRINTLN(F("Flash prepare command not accepted"));

      if (sendCheckReply("AT+CFSTERM","OK")) {
	DEBUG_PRINTLN(F("Closed out a dangling session, retry write prepare"));
	if (!sendCheckReply("AT+CFSINIT","OK")) {
	  DEBUG_PRINTLN(F("Flash prepare command still not accepted"));
	  return false;
	}
	// fall thru
      }
      else {
	return false;
      }
    }

    snprintf(cmd, sizeof(cmd), "AT+CFSGFIS=%d,\"%s\"", area, filename);
    if (!sendExpectIntReply(cmd, "+CFSGFIS: ", &size)) {
      DEBUG_PRINTLN(F("File not found"));
      return false;
    }
    if (size >= buf_size) {
      // insist on room for a null terminator so need n+1 buf space to read n bytes
      DEBUG_PRINTLN(F("File too large"));
      return false;
    }

    snprintf(cmd, sizeof(cmd), "AT+CFSRFILE=3,\"%s\",0,%d,0", filename, size);
    if (sendExpectIntReply(cmd, "+CFSRFILE: ", &size)) {
      if (size >= buf_size) {
	// wtf, size changed
	DEBUG_PRINTLN(F("File too large"));
	return false;
      }
      if (!getReplyOfSize(buf, size, 5000, false)) {
	DEBUG_PRINTLN(F("Read failed"));
	return false;
      }
      if (size < buf_size) buf[size]='\0'; // null terminate
    }

    if (!expectReply(F("OK"))) {
      DEBUG_PRINTLN(F("Read failed"));
      return false;
    }

    if (!sendCheckReply("AT+CFSTERM","OK",5000)) {
      DEBUG_PRINTLN(F("Read cleanup failed"));
      return false;
    }

    return true;
  }

  bool writeFileVerify(const char *filename, const char *contents, int size=-1, int area=3) {
    if (size < 0) size = strlen(contents);

    if (writeFile(filename, contents,size,area)) {
      static char buf[10240];
      DEBUG_PRINTLN(F("Wrote file, reading back to compare"));

      if (readFile(filename, buf, sizeof(buf))) {
	DEBUG_PRINTLN(F("Readback complete"));

	if (strcmp(contents, buf) == 0) {
	  DEBUG_PRINTLN("Compare matched");
	  return true;
	}
	else {
	  DEBUG_PRINTLN("Compare MISMATCH");
	  for (int n=0; n<strlen(contents);n++) {
	    if (!strncmp(contents, buf, n)) {
	      DEBUG_PRINTF("mismatch starts at position %d\n",n);
	      break;
	    }
	  }
	}
      }
      else {
	DEBUG_PRINTLN(F("Readback failed"));
      }
    }
    else {
      DEBUG_PRINTLN("Write failed");
    }
    return false;
  }

  bool ftpPut(const char *host, const char *user, const char *pass, const char *path, const char *buf, int buf_len, int bearer=1)
  {
    char cmd[40];
    char exp[40];
    char dir[80];
    char name[40];
    int state,err;
    int retry = 0;

    while (retry<2) {
      // Get bearer status
      snprintf(cmd, sizeof(cmd), "AT+SAPBR=2,%d", bearer);
      snprintf(exp, sizeof(exp), "+SAPBR: %d,", bearer);
      if (!sendExpectIntReply(cmd, exp, &state)) {
	DEBUG_PRINTLN("Bearer query failed");
      }

      if (state != 1) {
	DEBUG_PRINTLN("Need to open session");
	// Configure bearer profile
	snprintf(cmd, sizeof(cmd), "AT+SAPBR=3,%d,\"APN\",\"telstra.m2m\"", bearer);
	if (!sendCheckReply(cmd, "OK")) {
	  DEBUG_PRINTLN("Bearer configure failed");
	  return false;
	}

	snprintf(cmd, sizeof(cmd), "AT+SAPBR=1,%d", bearer);
	if (!sendCheckReply(cmd, "OK")) {
	  DEBUG_PRINTLN("Bearer open failed");
	}
      }

      snprintf(cmd, sizeof(cmd), "AT+FTPCID=%d", bearer);
      if (sendCheckReply(cmd, "OK")) {
	// initate suceeded, break out of the retry loop
	break;
      }
      //else {
      //DEBUG_PRINTLN("FTP bearer profile set rejected, try muddling on");
//	break;
	// was fall through to dead code below
      //    }

      if (!retry) {
	DEBUG_PRINTLN("FTP not available, try reopen");
	snprintf(cmd, sizeof(cmd), "AT+SAPBR=0,%d", bearer);
	if (!sendCheckReply(cmd, "OK")) {
	  DEBUG_PRINTLN("Bearer close failed");
	  return false;
	}
	DEBUG_PRINTLN("Closed bearer, will retry open");
	++retry;
      }
    }

    DEBUG_PRINTLN("We seem to have an FTP session now");
    if (!sendCheckReply("AT+FTPMODE=1", "OK")) {
      DEBUG_PRINTLN("FTP passive mode set failed");
      return false;
    }

    snprintf(cmd, sizeof(cmd), "AT+FTPSERV=\"%s\"", host);
    if (!sendCheckReply(cmd, "OK")) {
      DEBUG_PRINTLN("FTP server failed");
      return false;
    }

    snprintf(cmd, sizeof(cmd), "AT+FTPUN=\"%s\"", user);
    if (!sendCheckReply(cmd, "OK")) {
      DEBUG_PRINTLN("FTP user failed");
      return false;
    }

    snprintf(cmd, sizeof(cmd), "AT+FTPPW=\"%s\"", pass);
    if (!sendCheckReply(cmd, "OK")) {
      DEBUG_PRINTLN("FTP pass failed");
      return false;
    }

    strlcpy(dir, path, sizeof(dir));
    char *dirsep = strrchr(dir, '/');
    if (dirsep == NULL) {
      // Path contains no slash, presume root
      DEBUG_PRINTLN("Upload path does not contain directory, presuming /home/ftp/images/");
      strlcpy(name, dir, sizeof(dir));
      strcpy(dir, "/home/ftp/images/");
    }
    else {
      // Split path into dir and name
      strlcpy(name, dirsep+1, sizeof(name));
      dirsep[1] = '\0';
      DEBUG_PRINTF("Split upload path into '%s' and '%s'\n", dir, name);
    }
    snprintf(cmd, sizeof(cmd), "AT+FTPPUTPATH=\"%s\"", dir);
    if (!sendCheckReply(cmd, "OK")) {
      DEBUG_PRINTLN("FTP path failed");
      return false;
    }

    snprintf(cmd, sizeof(cmd), "AT+FTPPUTNAME=\"%s\"", name);
    if (!sendCheckReply(cmd, "OK")) {
      DEBUG_PRINTLN("FTP name failed");
      return false;
    }

    int chunk = 200;
    snprintf(cmd, sizeof(cmd), "AT+FTPPUT=1");
    //snprintf(cmd, sizeof(cmd), "AT+FTPPUT=1,%d", buf_len);
    if (!sendExpectIntPairReply(cmd, "+FTPPUT: 1,",&err,&chunk,60000,80,Sim7000Modem::MULTILINE,Sim7000Modem::NOTRACE)) {
      DEBUG_PRINTLN("FTP put initiate failed");
      return false;
    }
    if (err != 1) {
      DEBUG_PRINTF("FTP state machine reports error %d, %s\n", err, ftp_error(err));
      return false;
    }


    int sent = 0;

    while (sent < buf_len) {
      int remain = buf_len - sent;
      int thischunk = (remain > chunk) ? chunk : remain;

      DEBUG_PRINTF("Sending chunk of %d bytes from remaining size %d\n", thischunk, remain);
      snprintf(cmd, sizeof(cmd), "AT+FTPPUT=2,%d", thischunk);
      if (!sendExpectIntPairReply(cmd, "+FTPPUT: ",&err,&thischunk,10000,80,Sim7000Modem::MULTILINE,Sim7000Modem::NOTRACE)) {
	DEBUG_PRINTLN("FTP data put failed");
	return false;
      }
      if (err != 2) {
	DEBUG_PRINTLN("FTP data put rejected");
	return false;
      }
      if (thischunk == 0) {
	DEBUG_PRINTLN("Modem not ready, wait");
	delay(500);
	continue;
      }
      this->write((const uint8_t *)buf+sent, thischunk);
      sent += thischunk;
      DEBUG_PRINTF("Wrote %d bytes, total so far %d of %d\n", thischunk, sent, buf_len);

      if (!sendExpectIntPairReply(NULL, "+FTPPUT: 1,",&err,&chunk,10000,80,Sim7000Modem::MULTILINE,Sim7000Modem::NOTRACE)) {
	DEBUG_PRINTLN("FTP continuation message not parsed");
	return false;
      }
      if (err != 1) {
	DEBUG_PRINTLN("FTP state machine did not invite continuation");
	return false;
      }
      DEBUG_PRINTF("FTP invites us to continue, next chunk size is %d\n",chunk);
    }

    // give tcp a little time to drain
    delay(500);

    // close the control connection
    if (!sendCheckReply("AT+FTPPUT=2,0", "OK")) {
      DEBUG_PRINTLN("FTP put close failed");
      return false;
    }

    snprintf(cmd, sizeof(cmd), "AT+SAPBR=0,%d", bearer);
    if (!sendCheckReply(cmd, "OK")) {
      DEBUG_PRINTLN("Bearer close failed (non fatal)");
      // this seems to be non-fatal
      //return false;
    }

    return true;

  }

  int ftpGet(const char *host, const char *user, const char *pass, const char *path, char *buf, int buf_max, int bearer=1)
  {
    char cmd[40];
    char exp[40];
    int state,count,err;
    int size = 0;

    // Configure bearer profile
    snprintf(cmd, sizeof(cmd), "AT+SAPBR=3,%d,\"APN\",\"telstra.m2m\"", bearer);
    if (!sendCheckReply(cmd, "OK")) {
      DEBUG_PRINTLN("Bearer configure failed");
      return -1;
    }

    snprintf(cmd, sizeof(cmd), "AT+SAPBR=2,%d", bearer);
    snprintf(exp, sizeof(exp), "+SAPBR: %d,", bearer);
    if (!sendExpectIntReply(cmd, exp, &state)) {
      DEBUG_PRINTLN("Bearer query failed");
    }

    if (state != 1) {
      DEBUG_PRINTF("Bearer state is %d, need to reopen (0=connecting, 1=connected, 2=closing, 3=closed)\n", state);
      snprintf(cmd, sizeof(cmd), "AT+SAPBR=1,%d", bearer);
      if (!sendCheckReply(cmd, "OK")) {
	DEBUG_PRINTLN("Bearer open failed");
      }
    }

    snprintf(cmd, sizeof(cmd), "AT+FTPCID=%d", bearer);
    if (!sendCheckReply(cmd, "OK")) {
      DEBUG_PRINTLN("FTP bearer profile set rejected");
      //return -1;
    }

    snprintf(cmd, sizeof(cmd), "AT+FTPSERV=\"%s\"", host);
    if (!sendCheckReply(cmd, "OK")) {
      DEBUG_PRINTLN("FTP server failed");
      return -1;
    }

    snprintf(cmd, sizeof(cmd), "AT+FTPUN=\"%s\"", user);
    if (!sendCheckReply(cmd, "OK")) {
      DEBUG_PRINTLN("FTP user failed");
      return -1;
    }

    snprintf(cmd, sizeof(cmd), "AT+FTPPW=\"%s\"", pass);
    if (!sendCheckReply(cmd, "OK")) {
      DEBUG_PRINTLN("FTP pass failed");
      return -1;
    }

    snprintf(cmd, sizeof(cmd), "AT+FTPGETNAME=\"%s\"", path);
    if (!sendCheckReply(cmd, "OK")) {
      DEBUG_PRINTLN("FTP name failed");
      return -1;
    }

    snprintf(cmd, sizeof(cmd), "AT+FTPGETPATH=\"/home/ftp/\"");
    if (!sendCheckReply(cmd, "OK")) {
      DEBUG_PRINTLN("FTP path failed");
      return -1;
    }

    if (!sendExpectIntPairReply("AT+FTPGET=1", "+FTPGET: ",&state,&err,75000,80,Sim7000Modem::MULTILINE)) {
      DEBUG_PRINTLN("FTP get initiate failed");
      return -1;
    }
    if (err != 1) {
      DEBUG_PRINTLN(ftp_error(err));
      return -1;
    }

    int chunk_max = 200;
    snprintf(cmd, sizeof(cmd), "AT+FTPGET=2,%d", chunk_max);
    state = 2;
    while (state == 2) {
      if (!sendExpectIntPairReply(cmd, "+FTPGET: ",&state, &count, 75000, 80, Sim7000Modem::MULTILINE)) {
	DEBUG_PRINTLN("FTP data fetch failed");
	return -1;
      }
      if (state == 1) {
	// connection is over
	DEBUG_PRINTLN("FTP data connection complete");
	break;
      }
      if ((state==2) && (count == 0)) {
	// connection still open but data not ready yet
	delay(100);
      }
      if (count > 0) {
	if (count > buf_max) {
	  DEBUG_PRINTLN(F("Reply buffer overflow"));
	  return -1;
	}
	DEBUG_PRINT(F("Reading chunk of ")); DEBUG_PRINTLN(count);
	if (!getReplyOfSize(buf, count, 30000, true)) {
	  DEBUG_PRINTLN(F("Read failed"));
	  return -1;
	}
	DEBUG_PRINT(F("Got a chunk of ")); DEBUG_PRINTLN(count);
	buf += count;
	size += count;
	buf_max -=count;
      }

      if (sendExpectIntPairReply(NULL, "+FTPGET: ",&state, &count, 500, 80, Sim7000Modem::MULTILINE)) {
	if (state == 1) {
	  if (count == 0) {
	    DEBUG_PRINTLN(F("Connection complete"));
	    // Connection complete
	    break;
	  }
	  else if (count == 1) {
	    DEBUG_PRINTLN(F("Read again"));
	    state=2;
	    // There is more data, but we don't know how mouch
	    continue;
	  }
	  else {
	    DEBUG_PRINTLN(F("Unexpected status"));
	  }
	}
	else if (state == 2) {
	  DEBUG_PRINT(F("Bytes remain ")); DEBUG_PRINTLN(count);
	}
      }
    }

    DEBUG_PRINTLN(F("ftpGet complete"));
    return size;
  }

  int httpGetWithCallback(const char *url,
			  Sim7000HttpHeaderCallback header_callback,
			  Sim7000HttpDataCallback chunk_callback,
			  int bearer=1,
			  int chunk_size=1024)
  {
    char cmd[255];
    char exp[40];
    int err, state,size,hdr_size;
    size_t file_size =0;
    size_t got_size = 0;
    int chunk;
    const int chunk_size_max = 1024;
    static char chunk_buf[chunk_size_max];

    if (chunk_size > chunk_size_max) chunk_size = chunk_size_max;
    

    // 
    // The basic HTTP get example from the simcom app note only works for
    // files up to about 40k.   Above this it returns 602 NO MEMORY
    //
    // We work around this by doing partial-range queries for small
    // chunks of the file
    // 
    DEBUG_PRINTF("httpGetWithCallback url=%s bearer=%d\n", url, bearer);

    // configure bearer profile
    snprintf(cmd, sizeof(cmd), "AT+SAPBR=3,%d,\"APN\",\"telstra.m2m\"", bearer);
    if (!sendCheckReply(cmd, "OK")) {
      DEBUG_PRINTLN("Bearer configure failed");
      return -1;
    }

    snprintf(cmd, sizeof(cmd), "AT+SAPBR=2,%d", bearer);
    snprintf(exp, sizeof(exp), "+SAPBR: %d,", bearer);
    if (!sendExpectIntReply(cmd, exp, &state)) {
      DEBUG_PRINTLN("Bearer query failed");
    }

    if (state != 1) {
      DEBUG_PRINTF("Bearer state is %d, need to reopen (0=connecting, 1=connected, 2=closing, 3=closed)\n", state);
      snprintf(cmd, sizeof(cmd), "AT+SAPBR=1,%d", bearer);
      if (!sendCheckReply(cmd, "OK")) {
	DEBUG_PRINTLN("Bearer open failed");
      }
    }

    if (!sendCheckReply("AT+HTTPINIT", "OK")) {
      DEBUG_PRINTLN("HTTP session initiation failed");
      goto abort;
    }

    snprintf(cmd, sizeof(cmd), "AT+HTTPPARA=CID,%d", bearer);
    if (!sendCheckReply(cmd, "OK")) {
      DEBUG_PRINTLN("HTTP bearer profile set rejected");
      return -1;
    }

    snprintf(cmd, sizeof(cmd), "AT+HTTPPARA=URL,\"%s\"", url);
    if (!sendCheckReply(cmd, "OK")) {
      DEBUG_PRINTLN("HTTP URL set failed");
      return -1;
    }

    do {

      snprintf(cmd, sizeof(cmd), "AT+HTTPPARA=BREAK,%lu", (unsigned long)got_size);
      if (!sendCheckReply(cmd, "OK")) {
	DEBUG_PRINTLN("HTTP BREAK set failed");
	return -1;
      }

      snprintf(cmd, sizeof(cmd), "AT+HTTPPARA=BREAKEND,%lu", (unsigned long)got_size+chunk_size-1);
      if (!sendCheckReply(cmd, "OK")) {
	DEBUG_PRINTLN("HTTP BREAKEND set failed");
	return -1;
      }

      // Send a HTTP get.   Because we are using partial range we expect 206,1024
      // i.e. size here is the size of the first chunk only
      if (!sendExpectIntPairReply("AT+HTTPACTION=0", "+HTTPACTION: 0,",
				  &err,&size,75000,80,Sim7000Modem::MULTILINE)) {
	DEBUG_PRINTLN("HTTP partial-get initiate failed");
	return -1;
      }

      if (err != 206) { // partial content
	DEBUG_PRINTF("HTTP error code %d\n", err);
	goto abort;
      }
	
      if (got_size == 0) {
	// This is the first chunk, get the total size from the HTTP header,
	// and pass it to the callback
	if (!sendExpectIntReply("AT+HTTPHEAD", "+HTTPHEAD: ",&hdr_size, 75000, 80)) {
	  DEBUG_PRINTLN("HTTP header fetch failed");
	  goto abort;
	}
	if (hdr_size > chunk_size_max) hdr_size=chunk_size_max;
	if (hdr_size > 0) {
	  DEBUG_PRINT(F("Reading header chunk of ")); DEBUG_PRINTLN(hdr_size);
	  if (!getReplyOfSize(chunk_buf, hdr_size, 10000, true)) {
	    DEBUG_PRINTLN(F("Chunk Read failed"));
	    goto abort;
	  }
	}
	else {
	  chunk_buf[0]='\0';
	}

	char *range_hdr = strstr(chunk_buf, "\r\ncontent-range: bytes ");
	if (!range_hdr) {
	  DEBUG_PRINTLN("Did not find range header");
	  goto abort;
	}
	char *slash = strchr(range_hdr, '/');
	if (!slash) {
	  DEBUG_PRINTLN("Did not find file-size marker in range header");
	  goto abort;
	}
	file_size = strtoul(slash+1, NULL, 10);
	if (file_size == 0) {
	  DEBUG_PRINTLN("Did not find file-size marker in range header");
	  goto abort;
	}

	if (!header_callback(err, file_size, (const uint8_t *)chunk_buf)) {
	  DEBUG_PRINTLN("Header callback indicated abort\n");
	  goto abort;
	}
      }



      // For first and all subsequent chunks, read the chunk data
      if (!sendExpectIntReply("AT+HTTPREAD", "+HTTPREAD: ",&chunk, 75000, 80)) {
	DEBUG_PRINTLN("HTTP data fetch failed");
	goto abort;
      }
      if (chunk > chunk_size) {
	// can't happen.  yeah right.
	DEBUG_PRINTF("HTTP chunk of size %d exceeds configured buffer size %d\n", chunk, chunk_size);
	goto abort;
      }
	
      if (chunk > 0) {
	DEBUG_PRINT(F("Reading chunk of ")); DEBUG_PRINTLN(chunk);
	if (!getReplyOfSize((char *)chunk_buf, chunk, 30000, true)) {
	  DEBUG_PRINTLN(F("Chunk Read failed"));
	  goto abort;
	}
      }
      DEBUG_PRINT(F("Got a chunk of ")); DEBUG_PRINTLN(chunk);


      if (!chunk_callback((const uint8_t *)chunk_buf, chunk)) {
	DEBUG_PRINTLN("Chunk callback indicated to abort");
	goto abort;
      }
      size -= chunk;
      got_size += chunk;

    } while (got_size < file_size);

    if (!sendCheckReply("AT+HTTPTERM", "OK")) {
      DEBUG_PRINTLN("HTTP close failed");
      return -1;
    }

    DEBUG_PRINTLN(F("HTTP get complete"));
    return got_size;

  abort:
    sendCheckReply("AT+HTTPTERM", "OK");
    return -1;

  }

};

// Local Variables:
// mode: C++
// c-basic-offset: 2
// End:
