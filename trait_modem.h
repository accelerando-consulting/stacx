#ifndef _TRAIT_MODEM_H
#define _TRAIT_MODEM_H

#include <HardwareSerial.h>
#include "freertos/semphr.h"

class TraitModem
{
  
protected:
  bool modem_present = false;
  Stream *modem_stream=NULL;
  int8_t uart_number = -1;
  unsigned long uart_baud = 115200;
  uint32_t uart_options = SERIAL_8N1;
  int8_t pin_rx = -1;
  int8_t pin_tx = -1;
  int8_t pin_rts = -1;
  int8_t pin_cts = -1;
  int8_t pin_ri = -1;

  // GPIO (if any) that controls hard power to this modem
  int8_t pin_power = -1;
  byte level_power = HIGH;

  // GPIO (if any) that is a soft-power key for this modem
  int8_t pin_key = -1;
  byte level_key = HIGH;
  uint16_t duration_key_on = 100;
  uint16_t duration_key_off = 1500;

  // GPIO (if any) that controls sleep mode for this modem
  int8_t pin_sleep = -1;
  byte level_sleep = HIGH;
  uint16_t timeout_bootwait = 10000;
  byte level_ri = HIGH;

  // Buffering for commands and responses
  SemaphoreHandle_t modem_port_mutex = NULL;
  SemaphoreHandle_t modem_buffer_mutex = NULL;
  int modem_command_max = 512;
  int modem_response_max = 512;
  char *modem_command_buf = 0;
  char *modem_response_buf = 0;
  int modem_timeout_default = 500;
  bool modem_disabled = false; // the modem is undergoing maintenance, ignore lock failues
  
  virtual const char *get_name_str() = 0;


  virtual bool modemCheckURC();
  virtual bool modemProcessURC(String Message) {
    DumpHex(L_ALERT, "Unsolicited Response Code not handled: ", (const uint8_t *)Message.c_str(), Message.length());
    return false;
  };

public:
  TraitModem(int uart_number, int8_t pin_rx, int8_t pin_tx, int uart_baud=115200, uint32_t uart_options=SERIAL_8N1, int8_t pin_pwr=-1, int8_t pin_key=-1, int8_t pin_sleep=-1) ;


  void setModemStream(Stream *s) { modem_stream = s; }
  virtual bool modemSetup();
  virtual bool modemProbe(bool quick=false);
  bool modemIsPresent() { return modem_present; }
  void setModemPresent(bool state=true) { modem_present = state; LEAF_NOTICE("setModemPresent(%s)", TRUTH(modem_present)); }
  void modemSetPower(bool state=true);
  void modemSetKey(bool state=true);
  void modemSetSleep(bool state=true);

  bool modemWaitPortMutex(const char *file, int line);
  bool modemHoldPortMutex(const char *file, int line,TickType_t timeout=0);
  void modemReleasePortMutex() ;

  bool modemWaitBufferMutex() ;
  bool modemHoldBufferMutex(TickType_t timeout=0);
  void modemReleaseBufferMutex() ;


  void modemFlushInput();

  bool modemSend(const char *cmd);
  int modemGetReply(char *buf=NULL, int buf_max=-1, int timeout=-1, int max_lines=1, int max_chars=0);

  bool modemSendExpect(const char *cmd, const char *expect, char *buf=NULL, int buf_max=0, int timeout=-1, int max_lines=1);
  String modemQuery(const char *cmd, const char *expect="", int timeout=-1);
  String modemQuery(String cmd, int timeout=-1);
  bool modemSendCmd(int timeout, const char *fmt, ...); // expects OK
  bool modemSendCmd(const char *fmt, ...); // expects OK

  bool modemSendExpectPrompt(const char *cmd, int timeout=-1);
  bool modemSendExpectInt(const char *cmd, const char *expect, int *value_r, int timeout=-1);
  bool modemSendExpectIntPair(const char *cmd, const char *expect, int *value_r, int *value2_r,int timeout=-1);
  bool modemSendExpectIntField(const char *cmd, const char *expect, int field_num, int *value_r=NULL, char separator=',', int timeout=-1);
  String modemSendExpectQuotedField(const char *cmd, const char *expect, int field_num, int separator=',', int timeout=-1) ;
  int modemGetReplyOfSize(char *resp, int size, int timeout=-1, int trace=-1);
  bool modemSetParameter(String verb, String parameter, String value) {
    return modemSendCmd("AT+%s=\"%s\",%s", verb, parameter, value.c_str());
  }
  bool modemSetParameterQuoted(String verb, String parameter, String value) {
    return modemSetParameter(verb, parameter, String("\"")+value+String("\""));
  }
  bool modemSetParameterPair(String verb, String parameter, String value1, String value2) {
    return modemSendCmd("AT+%s=\"%s\",%s", verb, parameter, value1+","+value2);
  }
  bool modemSetParameterQQ(String verb, String parameter, String value1, String value2) {
    return modemSetParameter(verb, parameter, String("\"")+value1+"\",\""+value2+"\"");
  }
  bool modemSetParameterUQ(String verb, String parameter, String value1, String value2) {
    return modemSetParameter(verb, parameter, value1+",\""+value2+"\"");
  }
  bool modemSetParameterUQQ(String verb, String parameter, String value1, String value2, String value3) {
    return modemSetParameter(verb, parameter, value1+",\""+value2+"\",\""+value3+"\"");
  }
  
  
  void modemChat(Stream *console_stream=&Serial, bool echo = false);
};
  
TraitModem::TraitModem(int uart_number, int8_t pin_rx, int8_t pin_tx, int uart_baud, uint32_t uart_options, int8_t pin_pwr, int8_t pin_key, int8_t pin_sleep) 
{
  this->uart_number = uart_number;
  this->uart_baud = uart_baud;
  this->uart_options = uart_options;
  this->pin_rx = pin_rx;
  this->pin_tx = pin_tx;
  this->pin_power = pin_power;
  this->pin_key = pin_key;
  this->pin_sleep = pin_sleep;
}

bool TraitModem::modemSetup() 
{
  LEAF_ENTER(L_NOTICE);
  
  if (!modem_command_buf) {
    modem_command_buf= (char *)calloc(modem_command_max, sizeof(char));
    if (!modem_command_buf) {
      LEAF_ALERT("modem_command_buf alloc failed");
      LEAF_RETURN(false);
    }
  }
  
  if (!modem_response_buf) {
    modem_response_buf=(char *)calloc(modem_response_max, sizeof(char));
    if (!modem_response_buf) {
      LEAF_ALERT("modem_response_buf alloc failed");
      LEAF_RETURN(false);
    }
  }

  if (!modem_stream) {
    LEAF_NOTICE("Setting up UART %d, rx=%d tx=%d, baud=%d , options=0x%x", uart_number, pin_rx, pin_tx, uart_baud, uart_options);
    HardwareSerial *uart = new HardwareSerial(uart_number);
    if (!uart) {
      LEAF_ALERT("uart port create failed");
      return false;
    }
    uart->begin(uart_baud, uart_options, pin_rx, pin_tx);
    modem_stream = uart;
  }
  LEAF_RETURN(true);
}


bool TraitModem::modemProbe(bool quick) 
{
  LEAF_ENTER(L_DEBUG);
  
  modemSetPower(true);
  modemSetSleep(false);
  modemSetKey(true);
    
  LEAF_NOTICE("Wait for modem powerup (configured max wait is %dms)", (int)timeout_bootwait);
  
  int retry = 1;
  setModemPresent(false);
  unsigned long timebox = millis() + timeout_bootwait;
  modemHoldPortMutex(__FILE__,__LINE__);

  do {
    LEAF_NOTICE("Initialising Modem (attempt %d)", retry);

    // We hammer the modem with AT<CR><LF> until it answers or we get sick
    // of waiting

    modemFlushInput();
    String response = modemQuery("AT");
    if (response.startsWith("AT")) {
      LEAF_NOTICE("Echo detected, attempting to disable");
      modemSendCmd("ATE0");
    }
    else if (response == "OK") {
      // Got a response, yay
      LEAF_NOTICE("Modem probe succeeded");
      setModemPresent(true);
      break;
    }
    ++retry;
    delay(modem_timeout_default);
  } while (millis() <= timebox);

  modemReleasePortMutex();
  
  LEAF_RETURN(modemIsPresent());
}

void TraitModem::modemSetPower(bool state) 
{
  if (pin_power >= 0) {
    pinMode(pin_power, OUTPUT);
    if (state) {
      LEAF_NOTICE("Activating modem power");
      digitalWrite(pin_power, level_power);
    }
    else {
      LEAF_NOTICE("Disconnecting modem power");
      digitalWrite(pin_power, !level_power);
    }
  }
}

void TraitModem::modemSetSleep(bool state) {
  if (pin_sleep >= 0) {
    pinMode(pin_sleep, OUTPUT);
    if (state) {
      LEAF_NOTICE("Asserting sleep pin");
      digitalWrite(pin_sleep, level_sleep);
    }
    else {
      LEAF_NOTICE("Deasserting sleep pin");
      digitalWrite(pin_sleep, !level_sleep);
    }
  }
}

void TraitModem::modemSetKey(bool state)
{
  if (pin_key >= 0) {
    if (state) {
      pinMode(pin_key, OUTPUT);
      LEAF_NOTICE("Powering on modem");
      digitalWrite(pin_key, !level_key);
      delay(100);
      digitalWrite(pin_key, level_key);
      delay(duration_key_on);
      digitalWrite(pin_key, !level_key);
    }
    else {
      LEAF_NOTICE("Powering off modem");
      digitalWrite(pin_key, !level_key);
      delay(100);
      digitalWrite(pin_key, level_key);
      delay(duration_key_off);
      digitalWrite(pin_key, !level_key);
    }
  }
}


bool TraitModem::modemHoldPortMutex(const char *file, int line,TickType_t timeout) 
{
  LEAF_ENTER(L_DEBUG);
  if (!modem_port_mutex) {
    SemaphoreHandle_t new_mutex = xSemaphoreCreateMutex();
    if (!new_mutex) {
      LEAF_ALERT("Modem port semaphore create failed");
      LEAF_RETURN(false);
    }
    if (xSemaphoreTake(new_mutex, timeout) != pdTRUE) {
      LEAF_ALERT("Modem port initial semaphore acquire failed");
      LEAF_RETURN(false);
    }
    else {
      //LEAF_INFO(">TAKE portMutex @%s:%d", file, line);
    }
    modem_port_mutex = new_mutex;
  }
  else {
    if (xSemaphoreTake(modem_port_mutex, timeout) != pdTRUE) {
      if (!modem_disabled) {
	LEAF_ALERT("Modem port semaphore acquire failed (@%s:%d)", file, line);
      }
      LEAF_RETURN(false);
    }
    else {
      //LEAF_INFO(">TAKE portMutex @%s:%d", file, line);
    }
  }
  LEAF_RETURN(true);
}

bool TraitModem::modemWaitPortMutex(const char *file, int line) 
{
  LEAF_ENTER(L_DEBUG);
  int wait_total=0;
  int wait_ms = 100;
  if (modem_disabled) {
    LEAF_RETURN(false);
  }
  while (1) {
    if (modemHoldPortMutex(file,line,wait_ms * portTICK_PERIOD_MS)) {
      //LEAF_INFO(">TAKE portMutex @%s:%d", file, line);
      LEAF_RETURN(true);
    }
    wait_total += wait_ms;
    wait_ms += 100;
    LEAF_NOTICE("Have been waiting %dms for modem port mutex", wait_total);
    if (wait_total > 5000) {
      LEAF_ALERT("DEADLOCK detected in modemWaitPortMutex");
      modemReleasePortMutex();
    }
  }
  LEAF_RETURN(false);
}

void TraitModem::modemReleasePortMutex() 
{
  LEAF_ENTER(L_DEBUG);
  if (xSemaphoreGive(modem_port_mutex) != pdTRUE) {
    LEAF_ALERT("Modem port mutex release failed");
  }
  else {
    //LEAF_INFO("<GIVE portMutex");
  }
  
  LEAF_VOID_RETURN;
}

bool TraitModem::modemHoldBufferMutex(TickType_t timeout) 
{
  LEAF_ENTER(L_DEBUG);
  if (!modem_buffer_mutex) {
    SemaphoreHandle_t new_mutex = xSemaphoreCreateMutex();
    if (!new_mutex) {
      LEAF_ALERT("Modem buffer semaphore create failed");
      LEAF_RETURN(false);
    }
    if (xSemaphoreTake(new_mutex, timeout) != pdTRUE) {
      LEAF_ALERT("Modem buffer semaphore acquire failed");
      LEAF_RETURN(false);
    }
    else {
      //LEAF_INFO(">TAKE bufMutex");
    }

    modem_buffer_mutex = new_mutex;
  }
  else {
    if (xSemaphoreTake(modem_buffer_mutex, timeout) != pdTRUE) {
      LEAF_ALERT("Modem buffer semaphore acquire failed");
      LEAF_RETURN(false);
    }
    else {
      //LEAF_INFO(">TAKE bufMutex");
    }
    

  }
  LEAF_RETURN(true);
}

bool TraitModem::modemWaitBufferMutex() 
{
  LEAF_ENTER(L_DEBUG);
  int wait_total=0;
  int wait_ms = 100;
  while (1) {
    if (modemHoldBufferMutex(wait_ms * portTICK_PERIOD_MS)) {
      LEAF_RETURN(true);
    }
    wait_total += wait_ms;
    wait_ms += 100;
    LEAF_NOTICE("Have been waiting %dms for modem port mutex", wait_total);
    if (wait_total > 5000) {
      LEAF_ALERT("DEADLOCK detected in modemWaitBufferMutex");
      modemReleaseBufferMutex();
      break;
    }
  }
  LEAF_RETURN(false);
}

void TraitModem::modemReleaseBufferMutex() 
{
  LEAF_ENTER(L_DEBUG);
  if (xSemaphoreGive(modem_buffer_mutex) != pdTRUE) {
    LEAF_ALERT("Buffer mutex release failed");
  }
  else {
    //LEAF_INFO("<GIVE bufMutex");
  }

  LEAF_VOID_RETURN;
}

void TraitModem::modemFlushInput() 
{
  char discard[64];
  int d =0;
  while (modem_stream->available()) {
    discard[d++] = modem_stream->read();
    if (d==sizeof(discard)) {
      DumpHex(L_DEBUG, "discard", (const uint8_t *)discard, d);
      d=0;
    }
  }
  if (d) {
    DumpHex(L_DEBUG, "discard", (const uint8_t *)discard, d);
  }
}

bool TraitModem::modemSend(const char *cmd)
{
  modem_stream->println(cmd);
  return true;
}


// precondition: hold buffer mutex if using shared buffer
int TraitModem::modemGetReply(char *buf, int buf_max, int timeout, int max_lines, int max_chars) 
{
  int count = 0;
  int line = 0;
  if (timeout < 0) timeout = modem_timeout_default;
  unsigned long start = millis();
  unsigned long timebox = start + timeout;
  unsigned long now = start;
  if (buf==NULL) {
    buf = modem_response_buf;
    buf_max = modem_response_max;
  }
  buf[0]='\0';
  
  bool done = false;
  while ((!done) && (now <= timebox)) {
    
    while (!done && modem_stream->available()) {
      char c = modem_stream->read();
      now = millis();

      switch (c) {
      case '\r':
	LEAF_DEBUG("modemGetReply   <CR");
	break;
      case '\n':
	if (line == 0) {
	  LEAF_DEBUG("modemGetReply   <LF (skip)");
	  ++line;
	  continue; // ignore the first pair of CRLF
	}
	else {
	  LEAF_DEBUG("modemGetReply   <LF (line %d/%d)", line, max_lines);
	}
	LEAF_INFO("modemGetReply   <[%s] (%dms, line %d/%d)", buf, (int)(now-start), line, max_lines);
	++line;
	if (line >= max_lines) {
	  LEAF_DEBUG("modemGetReply done");
	  done = true;
	  continue;
	}
	// fall thru
      default:
	buf[count++] = c;
	buf[count] = '\0';
	if (count >= buf_max) {
	  LEAF_ALERT("modemGetReply: buffer full");
	  return count;
	}
	if ((max_lines==0) && (count >= max_chars)) {
	  // if waiting for an inline prompt like ">" count chars not lines
	  done = true;
	  continue;
	}
      }
    }
    if (!done) delay(1);
    now = millis();
  }

  modemFlushInput(); // consume any trailing OK
  

  if (!done) {
    int elapsed = now-start;
    LEAF_NOTICE("modemGetReply: timeout (%dms)",elapsed);
  }
  return count;
}

// precondtion: hold buffer mutex if using the shared response_buf
bool TraitModem::modemSendExpect(const char *cmd, const char *expect, char *buf, int buf_max, int timeout, int max_lines)
{
  LEAF_ENTER(L_DEBUG);
  
  if (timeout < 0) timeout = modem_timeout_default;
  modemFlushInput();
  unsigned long start = millis();
  LEAF_NOTICE("modemSendExpect >[%s] <[%s]", cmd?cmd:"", expect?expect:"");
  bool result = true;
  if (cmd) {
    modemSend(cmd);
  }
  if (!buf) {
    buf = modem_response_buf;
    buf_max = modem_response_max;
  }
  int count = modemGetReply(buf, buf_max, timeout, max_lines);
  if (expect) {
    int expect_len = strlen(expect);

    if ((count >= expect_len) && (strncmp(buf, expect, expect_len) == 0)) {
      // shift off the expect leader
      memmove(buf, buf+expect_len, count-expect_len+1);
    }
    else {
      result = false;
    }
  }
  unsigned long elapsed = millis() - start;
  if (result) {
    LEAF_NOTICE("modemSendExpect <[%s] (MATCHED [%s], elapsed %dms)", buf, expect?expect:"", (int)elapsed);
  }
  else {
    LEAF_NOTICE("modemSendExpect <[%s] (MISMATCH expected [%s], elapsed %dms)", buf, expect?expect:"", (int)elapsed);
  }
  
  LEAF_RETURN(result);
}

// precondtion: hold buffer mutex
bool TraitModem::modemSendExpectPrompt(const char *cmd, int timeout)
{
  LEAF_ENTER(L_DEBUG);
  
  if (timeout < 0) timeout = modem_timeout_default;
  modemFlushInput();
  unsigned long start = millis();
  LEAF_INFO("modemSendExpect >[%s] <[<] (limit %dms)", cmd?cmd:"", timeout);
  bool result = true;
  if (cmd) {
    modemSend(cmd);
  }
  char *buf = modem_response_buf;
  int buf_max = modem_response_max;

  int count = modemGetReply(buf, buf_max, timeout, 0, 1);
  unsigned long elapsed = millis() - start;
  if (buf[0] == '>') {
    LEAF_INFO("modemSendExpect <[%s] (MATCHED [>], elapsed %dms)", buf, (int)elapsed);
  }
  else {
    LEAF_NOTICE("modemSendExpect <[%s] (MISMATCH expected [>], elapsed %dms)", buf, (int)elapsed);
    result=false;
  }
  
  LEAF_RETURN(result);
}


String TraitModem::modemQuery(const char *cmd, const char *expect, int timeout)
{
  if (!modemSendExpect(cmd, expect, modem_response_buf, modem_response_max, timeout)) {
    return "";
  }
  return String(modem_response_buf);
}

String TraitModem::modemQuery(String cmd, int timeout)
{
  if (!modemSendExpect(cmd.c_str(), "", modem_response_buf, modem_response_max, timeout)) {
    return "";
  }
  return String(modem_response_buf);
}



bool TraitModem::modemSendCmd(int timeout, const char *fmt, ...) 
{
  va_list ap;
  va_start(ap, fmt);
  bool result = false;
  modemWaitBufferMutex();
  vsnprintf(modem_command_buf, modem_command_max, fmt, ap);
  result = modemSendExpect(modem_command_buf, "OK", modem_response_buf, modem_response_max, timeout);
  modemReleaseBufferMutex();
  return result;
}

bool TraitModem::modemSendCmd(const char *fmt, ...) 
{
  va_list ap;
  va_start(ap, fmt);
  bool result = false;
  modemWaitBufferMutex();
  vsnprintf(modem_command_buf, modem_command_max, fmt, ap);
  result = modemSendExpect(modem_command_buf, "OK", modem_response_buf, modem_response_max, -1);
  modemReleaseBufferMutex();
  return result;
}
  
bool TraitModem::modemSendExpectInt(const char *cmd, const char *expect, int *value_r, int timeout) 
{
  bool result = false;
  if (timeout < 0) timeout = modem_timeout_default;
  modemWaitBufferMutex();
  modemSendExpect(cmd, expect, modem_response_buf, modem_response_max, timeout);
  if (strspn(modem_response_buf,"0123456789")>0) {
    if (value_r) *value_r = atoi(modem_response_buf);
    result = true;
  }
  modemReleaseBufferMutex();
  return result;
}


bool TraitModem::modemSendExpectIntField(const char *cmd, const char *expect, int field_num, int *value_r, char separator, int timeout) 
{
  if (timeout < 0) timeout = modem_timeout_default;
  if (field_num == 0) return modemSendExpectInt(cmd, expect, value_r, timeout);
  
  modemWaitBufferMutex();
  modemSendExpect(cmd, expect, modem_response_buf, modem_response_max, timeout);

  char *pos = modem_response_buf;
  int field = 1;

  while (field < field_num) {
    pos = strchr(pos, separator);
    ++field;
    if (!pos) {
      LEAF_ALERT("Field %d not found (exhausted at %d)", field_num, field);
      modemReleaseBufferMutex();
      return String("");
    }
    ++pos;
  }

  if (value_r) *value_r = atoi(pos);
  modemReleaseBufferMutex();
  return true;
}

String TraitModem::modemSendExpectQuotedField(const char *cmd, const char *expect, int field_num, int separator, int timeout) 
{
  if (timeout < 0) timeout = modem_timeout_default;
  if (!modemWaitBufferMutex()) {
    LEAF_ALERT("Mutex acquire failed");
    return "";
  }
  
  modemSendExpect(cmd, expect, modem_response_buf, modem_response_max, timeout);

  char *pos = modem_response_buf;
  char *next;
  int field = 1;

  while (field < field_num) {
    pos = strchr(pos, separator);
    ++field;
    if (!pos) {
      LEAF_ALERT("Field %d not found (exhausted at %d)", field_num, field);
      return String("");
    }
    ++pos;
  }

  if (*pos == '"') {
    // quoted field, ends at the next quote
    pos++;
    next=strchr(pos, '"');
    if (!next) {
      LEAF_ALERT("Quote mismatch");
      return String("");
    }
    *next = '\0';
  }
  else {
    // unquoted string, ends at the next separator
    next = strchr(pos, separator);
    if (next) *next = '\0';
  }
  modemReleaseBufferMutex();
  return String(pos);
}

bool TraitModem::modemSendExpectIntPair(const char *cmd, const char *expect, int *value_r, int *value2_r,int timeout) 
{
  if (timeout < 0) timeout = modem_timeout_default;
  bool result = false;
  modemWaitBufferMutex();
  modemSendExpect(cmd, expect, modem_response_buf, modem_response_max, timeout);
  char *comma = strchr(modem_response_buf, ',');
  if (comma) {
    *comma++='\0';
    if (value_r) *value_r = atoi(modem_response_buf);
    if (value2_r) *value2_r = atoi(comma);
    result = true;
  }
  modemReleaseBufferMutex();
  return result;
}

int TraitModem::modemGetReplyOfSize(char *resp, int size, int timeout, int trace)
{
  if (timeout < 0) timeout = modem_timeout_default;
  
  modem_stream->setTimeout(timeout);
  int p = modem_stream->readBytes(resp, size);
  resp[p]='\0';
  if (trace>=0) {
    __LEAF_DEBUG__(trace, "getReplyOfSize got %d/%d bytes\n%s\n", p,size,resp);
  }
  return (p);
}


void TraitModem::modemChat(Stream *console_stream, bool echo)
{
  char c = '\0';
  char prev = '\0';
  bool chat = true;
  // Enter a loop relaying data from console to modem (for testing the
  // modem or doing manual setup)
  console_stream->println("Modem chat session, type '<LF> .' to exit");
  if (!modemHoldPortMutex(__FILE__,__LINE__)) {
    LEAF_ALERT("Modem port busy");
    return;
  }
  modem_disabled = true;
  while (chat) {
    if (console_stream->available()) {
      prev = c;
      c = console_stream->read();
      if ((prev =='\n') && (c=='.')) {
	chat = false;
	console_stream->println("Modem chat session terminated");
	break;
      }
      if (echo) console_stream->write(c);
      modem_stream->write(c);
    }
    if (modem_stream->available()) {
      char d = modem_stream->read();
      console_stream->write(d);
    }
  }
  modem_disabled = false;
  modemReleasePortMutex();
}

bool TraitModem::modemCheckURC() 
{
  if (!modemHoldPortMutex(__FILE__,__LINE__)) {
    if (!modem_disabled) {
      LEAF_ALERT("modemCheckURC: modem is busy");
    }
    // somebody else is talking to the modem right now
    return false;
  }
  
  if (modem_stream->available()) {
    LEAF_INFO("Modem said something");
    modem_response_buf[0]='\0';

    int count = modemGetReply(modem_response_buf, modem_response_max);

    // Strip off any leading OKs
    while ((count > 4) && (strncmp(modem_response_buf, "OK\r\n", 4)==0)) {
      memmove(modem_response_buf, modem_response_buf+4, count+1);
      count -= 4;
    }
    if ((strlen(modem_response_buf) > 0) && (strcmp(modem_response_buf, "OK")!=0)) {
      modemProcessURC(String(modem_response_buf));
    }
  }
  modemReleasePortMutex();
  return true;
}
     

#endif
// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
