#ifndef _TRAIT_MODEM_H
#define _TRAIT_MODEM_H

#include <HardwareSerial.h>
#include "freertos/semphr.h"

#define MODEM_PROBE_QUICK true
#define MODEM_PROBE_NORMAL false

#define MODEM_MUTEX_TRACE(loc,...) __LEAF_DEBUG_AT__((loc), modem_mutex_trace_level, __VA_ARGS__)
#define MODEM_CHAT_TRACE(loc,...) __LEAF_DEBUG_AT__((loc), modem_chat_trace_level, __VA_ARGS__)

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
  bool invert_power = false;
  void (*modem_set_power_cb)(bool) = NULL;

  // GPIO (if any) that is a soft-power key for this modem
  int8_t pin_key = -1;
  bool invert_key = false;
  uint16_t duration_key_on = 100;
  uint16_t duration_key_off = 1500;
  void (*modem_set_key_cb)(bool) = NULL;

  // GPIO (if any) that controls sleep mode for this modem
  int8_t pin_sleep = -1;
  bool invert_sleep = false;
  uint16_t timeout_bootwait = 10000;
  byte level_ri = HIGH;
  void (*modem_set_sleep_cb)(bool) = NULL;

  // Buffering for commands and responses
  SemaphoreHandle_t modem_port_mutex = NULL;
  SemaphoreHandle_t modem_buffer_mutex = NULL;
  static const int modem_command_max = 512;
  static const int modem_response_max = 512;
  char modem_command_buf[modem_command_max];
  char modem_response_buf[modem_command_max];
  int modem_timeout_default = 500;
  bool modem_disabled = false; // the modem is undergoing maintenance,
			       // ignore lock failues
  bool modem_trace = false;
  int modem_chat_trace_level = L_INFO;
  int modem_mutex_trace_level = L_DEBUG;
  
  virtual const char *get_name_str() = 0;


  virtual bool modemCheckURC();
  virtual bool modemProcessURC(String Message) {
    DumpHex(L_ALERT, "Unsolicited Response Code not handled: ", (const uint8_t *)Message.c_str(), Message.length());
    return false;
  };
  void wdtReset() { Leaf::wdtReset(); }

public:
  TraitModem(int uart_number, int8_t pin_rx, int8_t pin_tx, int uart_baud=115200, uint32_t uart_options=SERIAL_8N1, int8_t pin_pwr=-1, int8_t pin_key=-1, int8_t pin_sleep=-1) ;


  void setModemStream(Stream *s) { modem_stream = s; }
  virtual bool modemSetup();
  virtual bool modemProbe(codepoint_t where = undisclosed_location, bool quick=false);
  bool modemIsPresent() { return modem_present; }
  void modemSetPresent(bool state=true) {
    if (state != modem_present) {
      idle_state(state?WAIT_IP:WAIT_MODEM, HERE);
      modem_present = state;
      LEAF_NOTICE("modemSetPresent(%s)", TRUTH(modem_present));
    }
  }
  void modemSetPower(bool state=true);
  void modemSetKey(bool state=true);
  void modemPulseKey(bool state=true);
  void modemSetSleep(bool state=true);
  void modemInstallPowerSetter(void (*cb)(bool)) {modem_set_power_cb=cb;}
  void modemInstallKeySetter(void (*cb)(bool)) {modem_set_key_cb=cb;}
  void modemInstallSleepSetter(void (*cb)(bool)) {modem_set_sleep_cb=cb;}

  bool modemWaitPortMutex(codepoint_t where = undisclosed_location);
  bool modemHoldPortMutex(codepoint_t where = undisclosed_location, TickType_t timeout=0);
  void modemReleasePortMutex(codepoint_t where = undisclosed_location) ;

  bool modemWaitBufferMutex() ;
  bool modemHoldBufferMutex(TickType_t timeout=0);
  void modemReleaseBufferMutex() ;
  bool modemDoTrace() { return modem_trace; }
  void modemSetTrace(bool value=true) {modem_trace=value;}

  void modemFlushInput();

  bool modemSend(const char *cmd, codepoint_t where=undisclosed_location);
  int modemGetReply(char *buf=NULL, int buf_max=-1, int timeout=-1, int max_lines=1, int max_chars=0, codepoint_t where = undisclosed_location);

  bool modemSendExpect(const char *cmd, const char *expect, char *buf=NULL, int buf_max=0, int timeout=-1, int max_lines=1, codepoint_t where=undisclosed_location);
  String modemQuery(const char *cmd, const char *expect="", int timeout=-1, codepoint_t where=undisclosed_location);
  String modemQuery(String cmd, int timeout=-1, codepoint_t where=undisclosed_location);
  bool modemSendCmd(int timeout, codepoint_t where, const char *fmt, ...); // expects OK
  bool modemSendCmd(codepoint_t where, const char *fmt, ...); // expects OK

  bool modemSendExpectOk(const char *cmd, codepoint_t where=undisclosed_location)
  {
    return modemSendExpect(cmd, "OK", NULL, 0, -1, 1, CODEPOINT(where));
  }
  bool modemSendExpectPrompt(const char *cmd, int timeout=-1,codepoint_t where=undisclosed_location);
  bool modemSendExpectInt(const char *cmd, const char *expect, int *value_r, int timeout=-1, codepoint_t where=undisclosed_location);
  bool modemSendExpectIntPair(const char *cmd, const char *expect, int *value_r, int *value2_r,int timeout=-1, int lines=2, codepoint_t where=undisclosed_location);
  bool modemSendExpectIntField(const char *cmd, const char *expect, int field_num, int *value_r=NULL, char separator=',', int timeout=-1, codepoint_t where=undisclosed_location);
  String modemSendExpectQuotedField(const char *cmd, const char *expect, int field_num, int separator=',', int timeout=-1, codepoint_t where=undisclosed_location) ;
  int modemGetReplyOfSize(char *resp, int size, int timeout=-1, int trace=-1, codepoint_t where=undisclosed_location);
  bool modemSetParameter(String verb, String parameter, String value, codepoint_t where=undisclosed_location) {
    return modemSendCmd(CODEPOINT(where), "AT+%s=\"%s\",%s", verb, parameter, value.c_str());
  }
  bool modemSetParameterQuoted(String verb, String parameter, String value, codepoint_t where=undisclosed_location) {
    return modemSetParameter(verb, parameter, String("\"")+value+String("\""),CODEPOINT(where));
  }
  bool modemSetParameterPair(String verb, String parameter, String value1, String value2, codepoint_t where=undisclosed_location) {
    return modemSendCmd(CODEPOINT(where), "AT+%s=\"%s\",%s", verb, parameter, value1+","+value2);
  }
  bool modemSetParameterQQ(String verb, String parameter, String value1, String value2, codepoint_t where=undisclosed_location) {
    return modemSetParameter(verb, parameter, String("\"")+value1+"\",\""+value2+"\"",CODEPOINT(where));
  }
  bool modemSetParameterUQ(String verb, String parameter, String value1, String value2, codepoint_t where=undisclosed_location) {
    return modemSetParameter(verb, parameter, value1+",\""+value2+"\"",CODEPOINT(where));
  }
  bool modemSetParameterUQQ(String verb, String parameter, String value1, String value2, String value3, codepoint_t where=undisclosed_location) {
    return modemSetParameter(verb, parameter, value1+",\""+value2+"\",\""+value3+"\"",CODEPOINT(where));
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
  if (this->pin_sleep == 1) {
    Serial.println("\n\n\n ** WTAF **\n\n\n");
  }
}

#ifndef ARDUINO_USB_CDC_ON_BOOT
#define ARDUINO_USB_CDC_ON_BOOT 0
#endif

bool TraitModem::modemSetup() 
{
  LEAF_ENTER(L_NOTICE);
  
  if (this->pin_sleep == 1) {
    Serial.println("\n\n\n ** WTAF **\n\n\n");
  }

  if (!modem_stream) {
    LEAF_NOTICE("Setting up UART %d, rx=%d tx=%d, baud=%d , options=0x%x", uart_number, pin_rx, pin_tx, uart_baud, uart_options);
    wdtReset();
    HardwareSerial *uart=NULL;// = new HardwareSerial(uart_number);
    switch (uart_number) {
    case 0:
#if ARDUINO_USB_CDC_ON_BOOT
      uart = &Serial0;
#else
      uart = &Serial;
#endif
      break;
#if SOC_UART_NUM > 1
    case 1:
      uart = &Serial1;
      break;
#if SOC_UART_NUM > 2
    case 2:
      uart = &Serial2;
      break;
#endif
#endif
    default:
      LEAF_ALERT("Unsupported uart index %d", uart_number);
    }
    
    if (uart==NULL) {
      LEAF_ALERT("uart port create failed");
      LEAF_BOOL_RETURN(false);
    }
    wdtReset();

    
    LEAF_NOTICE("uart %d begin baud=%d", uart_number, (int)uart_baud);
    Serial.flush();
    delay(500);
    uart->begin(uart_baud,uart_options, pin_rx, pin_tx);
    //uart->begin((int)uart_baud);

    /*
    if ((pin_rx >= 0) || (pin_tx >= 0)) {
      // pass the rx/tx pins as -1 to mean "use the default"
      LEAF_NOTICE("uart %d set pins [%d,%d]", uart_number, pin_rx, pin_tx);
      Serial.flush();
      delay(500);[
      uart->setPins(pin_rx, pin_tx);
    }
    */


    modem_stream = uart;
    LEAF_DEBUG("uart ready");
    wdtReset();
  }
  LEAF_BOOL_RETURN(true);
}


bool TraitModem::modemProbe(codepoint_t where, bool quick) 
{
  LEAF_ENTER(L_NOTICE);
  LEAF_NOTICE_AT(where, "modem probe (%s)", quick?"quick":"normal");
  if (!modem_stream) {
    LEAF_ALERT("Modem stream is not present");
    LEAF_BOOL_RETURN(false);
  }
  if (quick && modemSendCmd(where, "AT")) {
    LEAF_INFO("Modem responded OK to quick probe");
    modemSetPresent(true);
    LEAF_BOOL_RETURN(true);
  }

  ACTION("MODEM try");
  idle_state(TRY_MODEM, HERE);

  LEAF_NOTICE("modem handshake pins pwr=%d sleep=%d key=%d", (int)pin_power, (int)pin_sleep, (int)pin_key);
  
  wdtReset();
  modemSetPower(true);
  if (pin_sleep == 1) { LEAF_ALERT("WTAFF");}
  //modemSetSleep(false);
  modemPulseKey(true);

  LEAF_NOTICE("Wait for modem powerup (configured max wait is %dms)", (int)timeout_bootwait);
  
  int retry = 1;
  wdtReset();
  modemSetPresent(false);
  unsigned long timebox = millis() + timeout_bootwait;
  wdtReset();
  modemHoldPortMutex(where);

  do {
    LEAF_NOTICE("Initialising Modem (attempt %d)", retry);
    wdtReset();

    // We hammer the modem with AT<CR><LF> until it answers or we get sick
    // of waiting

    modemFlushInput();
    String response = modemQuery("AT");
    if (response.startsWith("AT")) {
      LEAF_NOTICE("Echo detected, attempting to disable");
      modemSendCmd(HERE, "ATE0");
    }
    else if (response == "OK") {
      // Got a response, yay
      LEAF_NOTICE("Modem probe succeeded");
      modemSetPresent(true);
      break;
    }
    ++retry;
    delay(modem_timeout_default);
  } while (millis() <= timebox);

  modemReleasePortMutex(HERE);
  bool result = modemIsPresent();
  if (result) {
    idle_state(WAIT_IP);
  }
  else {
    idle_state(WAIT_MODEM);
  }
  
  LEAF_BOOL_RETURN_SLOW(2000, result);
}

void TraitModem::modemSetPower(bool state) 
{
  LEAF_ENTER_BOOL(L_NOTICE,state);
  
  if (pin_power >= 0) {
    pinMode(pin_power, OUTPUT);
  }
  if (pin_power >= 0) {
    digitalWrite(pin_power, state^invert_power);
  }
  if (modem_set_power_cb) {
    LEAF_NOTICE("Invoke modem_set_power_cb");
    modem_set_power_cb(state^invert_power);
  }
  LEAF_LEAVE;
}

void TraitModem::modemSetSleep(bool state) {
  LEAF_ENTER_BOOL(L_NOTICE, state);

  if (pin_sleep >= 0) {
    pinMode(pin_sleep, OUTPUT);
  }
  
  if (pin_sleep >= 0) {
    digitalWrite(pin_sleep, state^invert_sleep);
  }
  if (modem_set_sleep_cb) {
    LEAF_NOTICE("Invoke modem_set_sleep_cb");
    modem_set_sleep_cb(state^invert_sleep);
  }
  LEAF_LEAVE;
}

void TraitModem::modemSetKey(bool state) {
  LEAF_ENTER_BOOL(L_NOTICE, state);

  if (pin_key >= 0) {
    pinMode(pin_key, OUTPUT);
  }
  
  if (pin_key >= 0) {
    digitalWrite(pin_key, state^invert_key);
  }
  if (modem_set_key_cb) {
    LEAF_NOTICE("Invoke modem_set_key_cb");
    modem_set_key_cb(state^invert_key);
  }
  LEAF_LEAVE;
}

void TraitModem::modemPulseKey(bool state)
{
  LEAF_ENTER_BOOL(L_NOTICE, state);

  if (state) {
    LEAF_NOTICE("Powering on modem");
    modemSetKey(LOW);
    delay(100);
    modemSetKey(HIGH);
    delay(duration_key_on);
    modemSetKey(LOW);
  }
  else {
    LEAF_NOTICE("Powering off modem");
    modemSetKey(LOW);
    delay(100);
    modemSetKey(HIGH);
    delay(duration_key_off);
    modemSetKey(LOW);
  }
  LEAF_LEAVE;
}


bool TraitModem::modemHoldPortMutex(codepoint_t where,TickType_t timeout) 
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
      MODEM_MUTEX_TRACE(where, ">TAKE portMutex");
    }
    modem_port_mutex = new_mutex;
  }
  else {
    if (xSemaphoreTake(modem_port_mutex, timeout) != pdTRUE) {
      if (!modem_disabled) {
	LEAF_ALERT_AT(where, "Modem port semaphore acquire failed");
      }
      LEAF_RETURN(false);
    }
    else {
      MODEM_MUTEX_TRACE(where, ">TAKE portMutex");
    }
  }
  LEAF_RETURN(true);
}

bool TraitModem::modemWaitPortMutex(codepoint_t where) 
{
  LEAF_ENTER(L_DEBUG);
  int wait_total=0;
  int wait_ms = 100;
  if (modem_disabled) {
    LEAF_RETURN(false);
  }
  while (1) {
    wdtReset();
    if (modemHoldPortMutex(CODEPOINT(where) ,wait_ms * portTICK_PERIOD_MS)) {
      MODEM_MUTEX_TRACE(where, ">TAKE portMutex");
      LEAF_RETURN(true);
    }
    wait_total += wait_ms;
    wait_ms += 100;
    LEAF_NOTICE_AT(where, "Have been waiting %dms for modem port mutex", wait_total);
    if (wait_total > 5000) {
      LEAF_ALERT_AT(where, "DEADLOCK detected in modemWaitPortMutex");
      modemReleasePortMutex(CODEPOINT(where));
    }
  }
  LEAF_RETURN(false);
}

void TraitModem::modemReleasePortMutex(codepoint_t where) 
{
  LEAF_ENTER(L_DEBUG);
  if (xSemaphoreGive(modem_port_mutex) != pdTRUE) {
    LEAF_ALERT_AT(where, "Modem port mutex release failed");
  }
  else {
    MODEM_MUTEX_TRACE(where, "<GIVE portMutex");
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
    wdtReset();
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
  if (!modem_stream) {
    LEAF_ALERT("WTF modem_stream is null");
  }
  while (modem_stream && (modem_stream->available())) {
    wdtReset();
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

bool TraitModem::modemSend(const char *cmd, codepoint_t where)
{
  if (modemDoTrace()) Serial.println(cmd);
  modem_stream->println(cmd);
  return true;
}


// precondition: hold buffer mutex if using shared buffero
int TraitModem::modemGetReply(char *buf, int buf_max, int timeout, int max_lines, int max_chars, codepoint_t where) 
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
  bool trace = modemDoTrace();
  
  bool done = false;
  while ((!done) && (now <= timebox)) {
    wdtReset();
    
    while (!done && modem_stream->available()) {
      char c = modem_stream->read();
      if (trace) Serial.println(c);
      now = millis();
      wdtReset();

      switch (c) {
      case '\r':
	LEAF_DEBUG_AT(where, "modemGetReply   <CR");
	break;
      case '\n':
	if (line == 0) {
	  LEAF_DEBUG_AT(where, "modemGetReply   <LF (skip)");
	  ++line;
	  continue; // ignore the first pair of CRLF
	}
	else {
	  MODEM_CHAT_TRACE(where, "modemGetReply   RCVD LF (line %d/%d)", line, max_lines);
	}
	MODEM_CHAT_TRACE(where, "modemGetReply   RCVD[%s] (%dms, line %d/%d)", buf, (int)(now-start), line, max_lines);
	++line;
	if (line >= max_lines) {
	  MODEM_CHAT_TRACE(where, "modemGetReply done (line=%d)", line);
	  done = true;
	  continue;
	}
	// fall thru
      default:
	buf[count++] = c;
	buf[count] = '\0';
	if (count >= buf_max) {
	  LEAF_ALERT_AT(where, "modemGetReply: buffer full");
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
    LEAF_NOTICE_AT(where, "modemGetReply: timeout (%dms)",elapsed);
  }
  return count;
}

// precondtion: hold buffer mutex if using the shared response_buf
bool TraitModem::modemSendExpect(const char *cmd, const char *expect, char *buf, int buf_max, int timeout, int max_lines, codepoint_t where)
{
  LEAF_ENTER(L_DEBUG);
  
  if (timeout < 0) timeout = modem_timeout_default;
  modemFlushInput();
  unsigned long start = millis();
  MODEM_CHAT_TRACE(where, "modemSendExpect SEND[%s] EXPECT[%s]", cmd?cmd:"", expect?expect:"");
  bool result = true;
  if (cmd) {
    modemSend(cmd);
  }
  if (!buf) {
    buf = modem_response_buf;
    buf_max = modem_response_max;
  }
  int count = modemGetReply(buf, buf_max, timeout, max_lines, 0, where);
  if (expect) {
    int expect_len = strlen(expect);
    if (count >= expect_len) {
      char *pos = strstr(buf, expect);
      if (pos) {
	int shift = pos - buf + expect_len;
	memmove(buf, buf+shift, count-shift+1);
      }
      else {
	result = false;
      }
    }
    else {
      result = false;
    }
  }
  unsigned long elapsed = millis() - start;
  if (result) {
    MODEM_CHAT_TRACE(where, "modemSendExpect RCVD[%s] (MATCHED [%s], elapsed %dms)", buf, expect?expect:"", (int)elapsed);
  }
  else {
    MODEM_CHAT_TRACE(where, "modemSendExpect RCVD[%s] (MISMATCH expected [%s], elapsed %dms)", buf, expect?expect:"", (int)elapsed);
  }
  
  LEAF_RETURN_SLOW(timeout, result);
}

// precondtion: hold buffer mutex
bool TraitModem::modemSendExpectPrompt(const char *cmd, int timeout, codepoint_t where)
{
  LEAF_ENTER(L_DEBUG);
  
  if (timeout < 0) timeout = modem_timeout_default;
  modemFlushInput();
  unsigned long start = millis();
  LEAF_INFO_AT(where, "modemSendExpect >[%s] <[<] (limit %dms)", cmd?cmd:"", timeout);
  bool result = true;
  if (cmd) {
    modemSend(cmd);
  }
  char *buf = modem_response_buf;
  int buf_max = modem_response_max;

  int count = modemGetReply(buf, buf_max, timeout, 0, 1, where);
  unsigned long elapsed = millis() - start;
  if (buf[0] == '>') {
    MODEM_CHAT_TRACE(where, "modemSendExpect SEND[%s] (MATCHED [>], elapsed %dms)", buf, (int)elapsed);
  }
  else {
    MODEM_CHAT_TRACE(where, "modemSendExpect SEND[%s] (MISMATCH expected [>], elapsed %dms)", buf, (int)elapsed);
    result=false;
  }
  
  LEAF_RETURN(result);
}


String TraitModem::modemQuery(const char *cmd, const char *expect, int timeout, codepoint_t where)
{
  if (!modemSendExpect(cmd, expect, modem_response_buf, modem_response_max, timeout, 1, CODEPOINT(where))) {
    return "";
  }
  return String(modem_response_buf);
}

String TraitModem::modemQuery(String cmd, int timeout, codepoint_t where)
{
  if (!modemSendExpect(cmd.c_str(), "", modem_response_buf, modem_response_max, timeout, 1, CODEPOINT(where))) {
    return "";
  }
  return String(modem_response_buf);
}



bool TraitModem::modemSendCmd(int timeout, codepoint_t where, const char *fmt, ...) 
{
  va_list ap;
  va_start(ap, fmt);
  bool result = false;
  modemWaitBufferMutex();
  vsnprintf(modem_command_buf, modem_command_max, fmt, ap);
  result = modemSendExpect(modem_command_buf, "OK", modem_response_buf, modem_response_max, timeout, 1, CODEPOINT(where));
  modemReleaseBufferMutex();
  return result;
}

bool TraitModem::modemSendCmd(codepoint_t where, const char *fmt, ...) 
{
  va_list ap;
  va_start(ap, fmt);
  bool result = false;
  modemWaitBufferMutex();
  vsnprintf(modem_command_buf, modem_command_max, fmt, ap);
  result = modemSendExpect(modem_command_buf, "OK", modem_response_buf, modem_response_max, -1, 1, CODEPOINT(where));
  modemReleaseBufferMutex();
  return result;
}
  
bool TraitModem::modemSendExpectInt(const char *cmd, const char *expect, int *value_r, int timeout, codepoint_t where) 
{
  bool result = false;
  if (timeout < 0) timeout = modem_timeout_default;
  modemWaitBufferMutex();
  modemSendExpect(cmd, expect, modem_response_buf, modem_response_max, timeout, 1, CODEPOINT(where));
  if (strspn(modem_response_buf,"0123456789")>0) {
    if (value_r) *value_r = atoi(modem_response_buf);
    result = true;
  }
  modemReleaseBufferMutex();
  return result;
}


bool TraitModem::modemSendExpectIntField(const char *cmd, const char *expect, int field_num, int *value_r, char separator, int timeout, codepoint_t where) 
{
  if (timeout < 0) timeout = modem_timeout_default;
  if (field_num == 0) return modemSendExpectInt(cmd, expect, value_r, timeout, CODEPOINT(where));
  
  modemWaitBufferMutex();
  modemSendExpect(cmd, expect, modem_response_buf, modem_response_max, timeout, 1, CODEPOINT(where));

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

String TraitModem::modemSendExpectQuotedField(const char *cmd, const char *expect, int field_num, int separator, int timeout, codepoint_t where) 
{
  if (timeout < 0) timeout = modem_timeout_default;
  if (!modemWaitBufferMutex()) {
    LEAF_ALERT("Mutex acquire failed");
    return "";
  }
  
  modemSendExpect(cmd, expect, modem_response_buf, modem_response_max, timeout, 1, CODEPOINT(where));

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

bool TraitModem::modemSendExpectIntPair(const char *cmd, const char *expect, int *value_r, int *value2_r,int timeout, int lines, codepoint_t where) 
{
  if (timeout < 0) timeout = modem_timeout_default;
  bool result = false;
  modemWaitBufferMutex();
  modemSendExpect(cmd, expect, modem_response_buf, modem_response_max, timeout, lines, CODEPOINT(where));
  char *comma = strchr(modem_response_buf, ',');
  if (comma) {
    *comma++='\0';
    if (value_r) *value_r = atoi(modem_response_buf);
    if (value2_r) *value2_r = atoi(comma);
    result = true;
  }
  else {
    if (value_r) *value_r = atoi(modem_response_buf); // might be an error code
    LEAF_WARN("Did not find expected int pair in [%s]", modem_response_buf);
  }
  modemReleaseBufferMutex();
  return result;
}

int TraitModem::modemGetReplyOfSize(char *resp, int size, int timeout, int trace, codepoint_t where)
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
  if (!modemHoldPortMutex(HERE)) {
    LEAF_ALERT("Modem port busy");
    return;
  }
  modem_disabled = true;
  while (chat) {
    wdtReset();
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
    yield();
  }
  modem_disabled = false;
  modemReleasePortMutex(HERE);
}

bool TraitModem::modemCheckURC() 
{
  while (modem_stream->available()) {
    LEAF_INFO("Modem said something");
    modem_response_buf[0]='\0';
    wdtReset();

    if (!modemHoldPortMutex(HERE)) {
      if (!modem_disabled) {
	LEAF_ALERT("modemCheckURC: modem is busy");
      }
      // somebody else is talking to the modem right now
      return false;
    }
  
    int count = modemGetReply(modem_response_buf, modem_response_max,-1,1,0,HERE);

    modemReleasePortMutex(HERE);

    // Strip off any leading OKs
    while ((count > 4) && (strncmp(modem_response_buf, "OK\r\n", 4)==0)) {
      memmove(modem_response_buf, modem_response_buf+4, count+1);
      count -= 4;
    }
    
    if ((strlen(modem_response_buf) > 0) && (strcmp(modem_response_buf, "OK")!=0)) {
      modemProcessURC(String(modem_response_buf));
    }
  }
  return true;
}
     

#endif
// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
