#ifndef _TRAIT_MODEM_H
#define _TRAIT_MODEM_H

#include <HardwareSerial.h>
#include "freertos/semphr.h"

#ifndef IP_MODEM_CHAT_TRACE_LEVEL
#define IP_MODEM_CHAT_TRACE_LEVEL L_INFO
#endif

#ifndef IP_MODEM_KEY_INVERT
#define IP_MODEM_KEY_INVERT false
#endif

#ifndef MODEM_USE_MUTEX
#define MODEM_USE_MUTEX 1
#endif

#define MODEM_PROBE_QUICK true
#define MODEM_PROBE_NORMAL false

#define MODEM_REPLY_NO_FLUSH false


#define MODEM_MUTEX_TRACE(loc,...) __LEAF_DEBUG_AT__((loc), modem_mutex_trace_level, __VA_ARGS__)
#define MODEM_CHAT_TRACE(loc,...) __LEAF_DEBUG_AT__((loc), modem_chat_trace_level, __VA_ARGS__)

class TraitModem: virtual public Debuggable
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
  uint32_t modem_last_cmd = 0;
  Leaf *parent=NULL;
  int mutex_deadlock_limit = 10000;


  // GPIO (if any) that controls hard power to this modem
  int8_t pin_power = -1;
  bool invert_power = false;
  void (*modem_set_power_cb)(bool) = NULL;

  // GPIO (if any) that is a soft-power key for this modem
  int8_t pin_key = -1;
  bool invert_key = IP_MODEM_KEY_INVERT;
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
  static const int modem_command_max = 1024;
  static const int modem_response_max = 1024;
  char modem_command_buf[modem_command_max];
  char modem_response_buf[modem_command_max];
  int modem_timeout_default = 500;
  bool modem_disabled = false; // the modem is undergoing maintenance,
			       // ignore lock failues
  bool modem_trace = false;
  int modem_chat_trace_level = IP_MODEM_CHAT_TRACE_LEVEL;
  int modem_mutex_trace_level = L_DEBUG;
  bool modem_probe_at_urc = false;
  int modem_urc_probe_interval = 10000;
  unsigned long last_urc_probe = 0;

  virtual bool modemCheckURC();
  virtual bool modemProcessURC(String Message) {
    DumpHex(L_ALERT, "Unsolicited Response Code not handled: ", (const uint8_t *)Message.c_str(), Message.length());
    return false;
  };
  void wdtReset(codepoint_t where) { Leaf::wdtReset(where); }

public:
  TraitModem(String name, int uart_number, int8_t pin_rx, int8_t pin_tx, int uart_baud=115200, uint32_t uart_options=SERIAL_8N1, int8_t pin_pwr=-1, int8_t pin_key=-1, int8_t pin_sleep=-1);

  void modemSetParent(Leaf *p) { parent=p; }
  void modemSetMutexLimit(int limit) { mutex_deadlock_limit = limit; }
  void modemSetStream(Stream *s) { modem_stream = s; }
  virtual bool modemSetup();
  virtual bool modemProbe(codepoint_t where = undisclosed_location, bool quick=false, bool no_presence=false);
  virtual void onModemPresent() {}
  bool modemIsPresent() { return modem_present; }
  void modemSetPresent(bool state=true) {
    if (state != modem_present) {
      comms_state(state?WAIT_IP:WAIT_MODEM, HERE, parent);
      bool was_present = modem_present;
      modem_present = state;
      LEAF_NOTICE("modemSetPresent(%s)", TRUTH(modem_present));
      if (!was_present && modem_present) {
	// Modem was detected from a state of non-presence.   Trigger setup
	onModemPresent();
      }
    }
  }
  void modemSetPower(bool state=true);
  void modemSetKey(bool state=true);
  void modemPulseKey(bool state=true);
  void modemPulseKey(int duration);
  void modemSetSleep(bool state=true);
  void modemInstallPowerSetter(void (*cb)(bool)) {modem_set_power_cb=cb;}
  void modemInstallKeySetter(void (*cb)(bool)) { modem_set_key_cb=cb; }
  void modemInstallSleepSetter(void (*cb)(bool)) {modem_set_sleep_cb=cb;}

  bool modemWaitPortMutex(codepoint_t where = undisclosed_location, bool quiet=false, int timeout=0);
  bool modemHoldPortMutex(codepoint_t where = undisclosed_location, TickType_t timeout=0, bool quiet=false);
  void modemReleasePortMutex(codepoint_t where = undisclosed_location) ;

  bool modemWaitBufferMutex(codepoint_t where = undisclosed_location) ;
  bool modemHoldBufferMutex(TickType_t timeout=0,codepoint_t where = undisclosed_location);
  void modemReleaseBufferMutex(codepoint_t where = undisclosed_location) ;
  bool modemDoTrace() { return modem_trace; }
  void modemSetTrace(bool value=true) {modem_trace=value;}

  void modemFlushInput(codepoint_t where = undisclosed_location);

  bool modemSend(const char *cmd, codepoint_t where=undisclosed_location);
  int modemSendRaw(const uint8_t *buf, size_t len, codepoint_t where=undisclosed_location) {
    MODEM_CHAT_TRACE(where, "modemSendRaw (%d)", len);
    DumpHex(modem_chat_trace_level, "modemSendRaw", buf, len);
    int result = modem_stream->write((char *)buf, len);
    modem_stream->flush();
    return result;
  }
  int modemGetReply(char *buf=NULL, int buf_max=-1, int timeout=-1, int max_lines=1, int max_chars=0, codepoint_t where = undisclosed_location, bool flush=true);

  bool modemSendExpect(const char *cmd, const char *expect, char *buf=NULL, int buf_max=0, int timeout=-1, int max_lines=1, codepoint_t where=undisclosed_location, bool flush=true);
  String modemQuery(const char *cmd, const char *expect="", int timeout=-1, codepoint_t where=undisclosed_location);
  String modemQuery(String cmd, int timeout=-1, codepoint_t where=undisclosed_location);
  bool modemSendCmd(int timeout, codepoint_t where, const char *fmt, ...); // expects OK
  bool modemSendCmd(codepoint_t where, const char *fmt, ...); // expects OK

  bool modemSendExpectOk(const char *cmd, codepoint_t where=undisclosed_location)
  {
    return modemSendExpect(cmd, "OK", NULL, 0, -1, 1, CODEPOINT(where));
  }
  bool modemSendExpectPrompt(const char *cmd, int timeout=-1,codepoint_t where=undisclosed_location);
  bool modemSendExpectInt(const char *cmd, const char *expect, int *value_r, int timeout=-1, codepoint_t where=undisclosed_location, int lines=2, bool flush=true);
  bool modemSendExpectIntPair(const char *cmd, const char *expect, int *value_r, int *value2_r,int timeout=-1, int lines=2, codepoint_t where=undisclosed_location);
  bool modemSendExpectIntField(const char *cmd, const char *expect, int field_num, int *value_r=NULL, char separator=',', int timeout=-1, codepoint_t where=undisclosed_location, bool flush=true);
  String modemSendExpectQuotedField(const char *cmd, const char *expect, int field_num, int separator=',', int timeout=-1, codepoint_t where=undisclosed_location) ;
  bool modemSendExpectInlineInt(const char *cmd, const char *expect, int *value_r, char delimiter=',', int timeout=-1, codepoint_t where=undisclosed_location);
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

  int modemReadToBuffer(cbuf *buf, size_t size, codepoint_t where=undisclosed_location);

  void modemChat(Stream *console_stream=&Serial, bool echo = false);
};

TraitModem::TraitModem(String name, int uart_number, int8_t pin_rx, int8_t pin_tx, int uart_baud, uint32_t uart_options, int8_t pin_pwr, int8_t pin_key, int8_t pin_sleep)
  : Debuggable("name")
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
    DBGPRINTLN("\n\n\n ** WTAF **\n\n\n");
  }
}

#ifndef ARDUINO_USB_CDC_ON_BOOT
#define ARDUINO_USB_CDC_ON_BOOT 0
#endif

bool TraitModem::modemSetup()
{
  LEAF_ENTER(L_NOTICE);

  if (this->pin_sleep == 1) {
    DBGPRINTLN("\n\n\n ** WTAF **\n\n\n");
  }

  if (!modem_stream) {
    LEAF_NOTICE("Setting up UART %d, rx=%d tx=%d, baud=%d , options=0x%x", uart_number, pin_rx, pin_tx, uart_baud, uart_options);
    wdtReset(HERE);
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
    wdtReset(HERE);


    LEAF_NOTICE("uart %d begin baud=%d", uart_number, (int)uart_baud);
    uart->begin(uart_baud,uart_options, pin_rx, pin_tx);

    modem_stream = uart;
    //LEAF_DEBUG("uart ready");
    wdtReset(HERE);
  }
  LEAF_BOOL_RETURN(true);
}


bool TraitModem::modemProbe(codepoint_t where, bool quick, bool no_presence)
{
  LEAF_ENTER(L_INFO);
  __LEAF_DEBUG_AT__(where, quick?L_INFO:L_NOTICE, "modemProbe (%s)", quick?"quick":"normal");

  if (!modem_stream) {
    LEAF_ALERT("Modem stream is not present");
    LEAF_BOOL_RETURN(false);
  }
  if (quick) {
    int attempt=0;
    modemFlushInput(HERE);
    while (attempt<3) {
      String response = modemQuery("AT",-1,CODEPOINT(where));
      if ((response=="AT") || (response == "ATOK")) {
	LEAF_NOTICE("Disabling modem echo");
	// modem is answering, but needs echo turned off (and then we re-test)
	if (modemSendCmd(HERE, "ATE0")) {
	  response = modemQuery("AT",-1,CODEPOINT(where));
	}
      }
      if (response.startsWith("OK")) {
	//LEAF_INFO("Modem responded OK to quick probe");
	if (!no_presence) {
	  modemSetPresent(true);
	}
	LEAF_BOOL_RETURN(true);
      }
      LEAF_NOTICE("Quick probe response was unexpected [%s]", response.c_str());
      ++attempt;
    }
    // fall thru
  }

  ACTION("MODEM try");
  comms_state(TRY_MODEM, HERE, parent);

  //LEAF_INFO("modem handshake pins pwr=%d sleep=%d key=%d", (int)pin_power, (int)pin_sleep, (int)pin_key);
  wdtReset(HERE);
  modemSetPower(true); // turn on the power supply to modem (if configured)
  if (pin_sleep == 1) { LEAF_ALERT("WTAFF");}
  //modemSetSleep(false); // ensure sleep mode is disabled (if configured)
  modemPulseKey(true); // press the modem "soft power key" (if configured)"

  //LEAF_INFO("Wait for modem powerup (configured max wait is %dms)", (int)timeout_bootwait);

  int retry = 1;
  wdtReset(HERE);
  if (!no_presence) {
    modemSetPresent(false);
  }

  unsigned long timebox = millis() + timeout_bootwait;
  wdtReset(HERE);
  if (!modemWaitPortMutex(where)) {
    LEAF_NOTICE("Modem is busy");
    LEAF_BOOL_RETURN(false);
  }

  do {
    LEAF_NOTICE("Initialising Modem (attempt %d)", retry);
    wdtReset(HERE);

    // We hammer the modem with AT<CR><LF> until it answers or we get sick
    // of waiting

    modemFlushInput(HERE);
    String response = modemQuery("AT", "", -1, HERE);
    if (response.startsWith("AT")) {
      LEAF_NOTICE("Echo detected, attempting to disable");
      modemSendCmd(HERE, "ATE0");
    }
    else if (response == "OK") {
      // Got a response, yay
      LEAF_NOTICE("Modem probe succeeded");
      if (!no_presence) {
	modemSetPresent(true);
      }
      break;
    }
    else {
      MODEM_CHAT_TRACE(HERE, "modemProbe unexpected response [%s]", response.c_str());
    }
    ++retry;
    yield();
    delay(modem_timeout_default);
  } while (millis() <= timebox);

  modemReleasePortMutex(HERE);
  bool result = modemIsPresent();
  if (result) {
    comms_state(WAIT_IP, HERE, parent);
  }
  else {
    LEAF_ALERT("Modem not found");
    post_error(POST_ERROR_MODEM, 3);
    comms_state(WAIT_MODEM, HERE, parent);
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
    LEAF_INFO("Invoke modem_set_power_cb");
    modem_set_power_cb(state^invert_power);
  }
  LEAF_LEAVE;
}

void TraitModem::modemSetSleep(bool state) {
  LEAF_ENTER_BOOL(L_INFO, state);

  if (pin_sleep >= 0) {
    pinMode(pin_sleep, OUTPUT);
  }

  if (pin_sleep >= 0) {
    digitalWrite(pin_sleep, state^invert_sleep);
  }
  if (modem_set_sleep_cb) {
    LEAF_INFO("Invoke modem_set_sleep_cb");
    modem_set_sleep_cb(state^invert_sleep);
  }
  LEAF_LEAVE;
}

void TraitModem::modemSetKey(bool state) {
  LEAF_ENTER_STATE(L_INFO, state);

  if (pin_key >= 0) {
    pinMode(pin_key, OUTPUT);
  }

  bool value = state^invert_key;
  bool did = false;
  if (pin_key >= 0) {
    LEAF_NOTICE("Modem key pin %d <= %s (%s)", pin_key, TRUTH(state), STATE(value));
    digitalWrite(pin_key, value);
    did = true;
  }
  if (modem_set_key_cb != NULL) {
    LEAF_NOTICE("Invoke modem_set_key_cb(%s)", STATE(value));
    modem_set_key_cb(value);
    did = true;
  }
  if (!did) {
    LEAF_WARN("No action was taken, modem is probably misconfigured");
  }

  LEAF_LEAVE;
}

void TraitModem::modemPulseKey(bool state)
{
  LEAF_ENTER_BOOL(L_INFO, state);

  if (state) {
    LEAF_WARN("Powering on modem with soft-key%s pulse of %dms", invert_key?" (inverted)":"", (int)duration_key_on);
    modemSetKey(LOW); // should be unnecessary, but just in case
    delay(100);
    modemSetKey(HIGH);
    delay(duration_key_on);
    modemSetKey(LOW);
  }
  else {
    LEAF_WARN("Powering off modem with soft-key pulse of %dms", (int)duration_key_off);
    modemSetKey(LOW); // should be unnecessary, but just in case
    delay(100);
    modemSetKey(HIGH);
    delay(duration_key_off);
    modemSetKey(LOW);
  }
  LEAF_LEAVE;
}

void TraitModem::modemPulseKey(int duration)
{
  LEAF_ENTER_INT(L_INFO, duration);

  LEAF_WARN("Triggering modem soft-key with%s pulse of %dms", invert_key?" (inverted)":"",duration);
    modemSetKey(LOW); // should be unnecessary, but just in case
    delay(100);
    modemSetKey(HIGH);
    delay(duration);
    modemSetKey(LOW);
  LEAF_LEAVE;
}


bool TraitModem::modemHoldPortMutex(codepoint_t where,TickType_t timeout, bool quiet)
{
#if MODEM_USE_MUTEX
  LEAF_ENTER(L_DEBUG);
  if (!modem_port_mutex) {
    SemaphoreHandle_t new_mutex = xSemaphoreCreateMutex();
    if (!new_mutex) {
      LEAF_ALERT("Modem port semaphore create failed");
      LEAF_RETURN(false);
    }
    if (xSemaphoreTake(new_mutex, timeout) != pdTRUE) {
      if (!quiet) {
	LEAF_ALERT("Modem port initial semaphore acquire failed");
      }
      LEAF_RETURN(false);
    }
    else {
      MODEM_MUTEX_TRACE(where, ">TAKE portMutex");
    }
    modem_port_mutex = new_mutex;
  }
  else {
    if (xSemaphoreTake(modem_port_mutex, timeout) != pdTRUE) {
      if (modem_disabled || quiet) {
	//LEAF_DEBUG_AT(where, "Modem port semaphore acquire failed (which might not be a problem)");
      }
      else {
	LEAF_ALERT_AT(where, "Modem port semaphore acquire failed");
      }
      LEAF_RETURN(false);
    }
    else {
      MODEM_MUTEX_TRACE(where, ">TAKE portMutex");
    }
  }
  LEAF_RETURN(true);
#else
  return true;
#endif
}

bool TraitModem::modemWaitPortMutex(codepoint_t where, bool quiet, int timeout)
{
#if MODEM_USE_MUTEX
  LEAF_ENTER(L_DEBUG);
  if (timeout == 0) {
    if (mutex_deadlock_limit) {
      // make the default timeout longer than the deadlock limit
      timeout = mutex_deadlock_limit+1000;
    }
    else {
      // no deadlock limit, just pick a reasonable timeout
      timeout = 5000;
    }
  }
  static codepoint_t last_port_acquire = undisclosed_location;
  int wait_total=0;
  int wait_ms = 100;
  if (modem_disabled) {
    LEAF_RETURN(false);
  }

  while (1) {
    wdtReset(HERE);
    if (modemHoldPortMutex(CODEPOINT(where) ,wait_ms * portTICK_PERIOD_MS, quiet)) {
      // successful acquire (hold function will log TAKE)
      last_port_acquire = where;
      LEAF_RETURN(true);
    }
    wait_total += wait_ms;
    wait_ms += 100;
    if (wait_total > timeout) {
      break;
    }
    LEAF_NOTICE_AT(where, "Have been waiting %dms for modem port mutex", wait_total);
    LEAF_NOTICE_AT(last_port_acquire, "This is the point of last port mutex acquisition");
    if (mutex_deadlock_limit && (wait_total > mutex_deadlock_limit)) {
      LEAF_ALERT_AT(where, "DEADLOCK detected in modemWaitPortMutex");
      modemReleasePortMutex(CODEPOINT(where));
    }
    yield();
  }
  LEAF_RETURN(false);
#else
  return true;
#endif
}

void TraitModem::modemReleasePortMutex(codepoint_t where)
{
#if MODEM_USE_MUTEX
  LEAF_ENTER(L_DEBUG);
  if (xSemaphoreGive(modem_port_mutex) != pdTRUE) {
    LEAF_ALERT_AT(where, "Modem port mutex release failed");
  }
  else {
    MODEM_MUTEX_TRACE(where, "<GIVE portMutex");
  }
  LEAF_VOID_RETURN;
#endif
}

bool TraitModem::modemHoldBufferMutex(TickType_t timeout, codepoint_t where)
{
#if MODEM_USE_MUTEX
  LEAF_ENTER(L_DEBUG);
  if (!modem_buffer_mutex) {
    SemaphoreHandle_t new_mutex = xSemaphoreCreateMutex();
    if (!new_mutex) {
      LEAF_ALERT_AT(where, "Modem buffer semaphore create failed");
      LEAF_RETURN(false);
    }
    if (xSemaphoreTake(new_mutex, timeout) != pdTRUE) {
      LEAF_ALERT_AT(where, "Modem buffer semaphore acquire failed");
      LEAF_RETURN(false);
    }
    else {
      MODEM_MUTEX_TRACE(where, ">TAKE bufMutex");
    }

    modem_buffer_mutex = new_mutex;
  }
  else {
    if (xSemaphoreTake(modem_buffer_mutex, timeout) != pdTRUE) {
      LEAF_ALERT_AT(where, "Modem buffer semaphore acquire failed");
      LEAF_RETURN(false);
    }
    else {
      MODEM_MUTEX_TRACE(where, ">TAKE bufMutex");
    }
  }
  LEAF_RETURN(true);
#else
  return true;
#endif
}

bool TraitModem::modemWaitBufferMutex(codepoint_t where)
{
#if MODEM_USE_MUTEX
  LEAF_ENTER(L_DEBUG);

  static codepoint_t last_buffer_acquire = undisclosed_location;
  int wait_total=0;
  int wait_ms = 100;
  while (1) {
    wdtReset(HERE);
    if (modemHoldBufferMutex(wait_ms * portTICK_PERIOD_MS, where)) {
      last_buffer_acquire = where;
      LEAF_RETURN(true);
    }
    wait_total += wait_ms;
    wait_ms += 100;
    LEAF_NOTICE_AT(where, "Have been waiting %dms for modem port mutex", wait_total);
    LEAF_NOTICE_AT(last_buffer_acquire, "This is the point of last buffer mutex acquisition");
    if (mutex_deadlock_limit && (wait_total > mutex_deadlock_limit)) {
      LEAF_ALERT_AT(where, "DEADLOCK detected in modemWaitBufferMutex");
      modemReleaseBufferMutex(where);
      break;
    }
    yield();
  }
  LEAF_RETURN(false);
#else
  return true;
#endif
}

void TraitModem::modemReleaseBufferMutex(codepoint_t where)
{
#if MODEM_USE_MUTEX
  LEAF_ENTER(L_DEBUG);
  if (xSemaphoreGive(modem_buffer_mutex) != pdTRUE) {
    LEAF_ALERT_AT(where, "Buffer mutex release failed");
  }
  else {
    MODEM_MUTEX_TRACE(where, "<GIVE bufMutex");
  }

  LEAF_VOID_RETURN;
#endif
}

void TraitModem::modemFlushInput(codepoint_t where)
{
  char discard[128];
  int d =0;
  if (!modem_stream) {
    LEAF_ALERT("WTF modem_stream is null");
  }
  while (modem_stream && (modem_stream->available())) {
    wdtReset(HERE);
    discard[d++] = modem_stream->read();
    if (d==sizeof(discard)) {
      __LEAF_DEBUG_AT__(CODEPOINT(where), modem_chat_trace_level, "discard %d...", d);
      DumpHex(modem_chat_trace_level, "", (const uint8_t *)discard, d);
      d=0;
    }
  }
  if (d) {
    __LEAF_DEBUG_AT__(CODEPOINT(where), modem_chat_trace_level, "discard %d", d);
    if (getDebugLevel()>=modem_chat_trace_level) {
      DumpHex(0, "", (const uint8_t *)discard, d);
    }
  }
}

bool TraitModem::modemSend(const char *cmd, codepoint_t where)
{
  if (modemDoTrace()) DBGPRINTLN(cmd);
  modem_stream->println(cmd);
  modem_last_cmd=millis();
  return true;
}


// precondition: hold buffer mutex if using shared buffer
int TraitModem::modemGetReply(char *buf, int buf_max, int timeout, int max_lines, int max_chars, codepoint_t where, bool flush)
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
    wdtReset(HERE);

    while (!done && modem_stream->available()) {
      char c = modem_stream->read();
      if (trace) {
	if ((c <= ' ') || (c>'~')) {
	  DBGPRINTF("0x%02x\n", (int)c);
	}
	else {
	  DBGPRINTLN(c);
	}
      }

      now = millis();
      wdtReset(HERE);

      switch (c) {
      case '\r':
	//LEAF_DEBUG_AT(where, "modemGetReply   <CR");
	break;
      case '\n':
	if (count == 0) {
	  //LEAF_DEBUG_AT(where, "modemGetReply   <LF (skip)");
	  ++line;
	  continue; // ignore a leading CRLF
	}
	else {
	  LEAF_INFO_AT(where, "modemGetReply   RCVD LF (line %d/%d)", line, max_lines);
	}
	MODEM_CHAT_TRACE(where, "modemGetReply   RCVD[%s] (%dms, line %d/%d)", buf, (int)(now-start), line, max_lines);
	++line;
	if (line >= max_lines) {
	  LEAF_INFO_AT(where, "modemGetReply done (line=%d)", line);
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
      yield();
    }
    now = millis();
  }

  if (flush) {
    modemFlushInput(CODEPOINT(where)); // consume any trailing OK
  }

  if (!done) {
    int elapsed = now-start;
    LEAF_NOTICE_AT(where, "modemGetReply: timeout (%dms)",elapsed);
  }
  return count;
}

int TraitModem::modemReadToBuffer(cbuf *buf, size_t size, codepoint_t where)
{
  int got = 0;
  while (size && (!buf->full())) {
    if (modem_stream->available()) {
      char c = modem_stream->read();

      if (modemDoTrace()) {
	if ((c <= ' ') || (c>'~')) {
	  DBGPRINTF("0x%02x\n", (int)c);
	}
	else {
	  DBGPRINTLN(c);
	}
      }

      buf->write(c);
      got++;
      size--;
    }
  }
  MODEM_CHAT_TRACE(where, "modemReadToBuffer received %d bytes", got);
  char dump_buf[512];
  int len = buf->peek(dump_buf, sizeof(dump_buf));
  DumpHex(modem_chat_trace_level, "modemReadToBuffer", dump_buf, len);

  return got;
}

// precondtion: hold buffer mutex if using the shared response_buf
bool TraitModem::modemSendExpect(const char *cmd, const char *expect, char *buf, int buf_max, int timeout, int max_lines, codepoint_t where, bool flush)
{
  LEAF_ENTER(L_DEBUG);

  if (timeout < 0) timeout = modem_timeout_default;
  if (flush) {
    modemFlushInput(HERE);
  }
  unsigned long start = millis();
  if (expect && strlen(expect)) {
    MODEM_CHAT_TRACE(where, "modemSendExpect SEND[%s] EXPECT[%s]", cmd?cmd:"", expect?expect:"");
  }
  else {
    MODEM_CHAT_TRACE(where, "modemSendExpect SEND[%s]", cmd?cmd:"", expect?expect:"");
  }

  bool result = true;
  if (cmd) {
    modemSend(cmd);
  }
  if (!buf) {
    buf = modem_response_buf;
    buf_max = modem_response_max;
  }
  int count = modemGetReply(buf, buf_max, timeout, max_lines, 0, where, flush);
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
    if (expect && strlen(expect)) {
      // log that we saw the expected response
      MODEM_CHAT_TRACE(where, "modemSendExpect RCVD[%s] (MATCHED [%s], elapsed %dms)", buf, expect?expect:"", (int)elapsed);
    }
    else {
      // empty expect, do not log an expect string
      MODEM_CHAT_TRACE(where, "modemSendExpect RCVD[%s] (elapsed %dms)", buf, (int)elapsed);
    }
  }
  else {
    MODEM_CHAT_TRACE(where, "modemSendExpect RCVD[%s] (MISMATCH expected [%s], elapsed %dms)", buf, expect?expect:"", (int)elapsed);
  }

  LEAF_RETURN_SLOW(timeout+100, result);
}

// precondtion: hold buffer mutex
bool TraitModem::modemSendExpectPrompt(const char *cmd, int timeout, codepoint_t where)
{
  LEAF_ENTER(L_DEBUG);

  if (timeout < 0) timeout = modem_timeout_default;
  modemFlushInput(HERE);
  unsigned long start = millis();
  LEAF_INFO_AT(where, "modemSendExpectPrompt >[%s] <[<] (limit %dms)", cmd?cmd:"", timeout);
  bool result = true;
  if (cmd) {
    MODEM_CHAT_TRACE(where, "modemSendExpectPrompt SEND %s", cmd);
    modemSend(cmd);
  }
  char *buf = modem_response_buf;
  int buf_max = modem_response_max;

  int count = modemGetReply(buf, buf_max, timeout, 0, 1, where, MODEM_REPLY_NO_FLUSH);
  unsigned long elapsed = millis() - start;
  if (buf[0] == '>') {
    MODEM_CHAT_TRACE(where, "modemSendExpectPrompt (MATCHED [>], elapsed %dms)", (int)elapsed);
    modemFlushInput(HERE);
  }
  else {
    // did not get a prompt; read remainder of probable error message
    modemGetReply(buf+count,buf_max-count,100);
    modemFlushInput(HERE);
    MODEM_CHAT_TRACE(where, "modemSendExpectPrompt RCVD[%s] (MISMATCH expected [>], elapsed %dms)", buf, (int)elapsed);
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
  modemWaitBufferMutex(HERE/*CODEPOINT(where)*/);
  vsnprintf(modem_command_buf, modem_command_max, fmt, ap);
  result = modemSendExpect(modem_command_buf, "OK", modem_response_buf, modem_response_max, timeout, 1, CODEPOINT(where));
  modemReleaseBufferMutex(CODEPOINT(where));
  return result;
}

bool TraitModem::modemSendCmd(codepoint_t where, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  bool result = false;
  modemWaitBufferMutex(HERE/*CODEPOINT(where)*/);
  vsnprintf(modem_command_buf, modem_command_max, fmt, ap);
  result = modemSendExpect(modem_command_buf, "OK", modem_response_buf, modem_response_max, -1, 1, CODEPOINT(where));
  modemReleaseBufferMutex(CODEPOINT(where));
  return result;
}

bool TraitModem::modemSendExpectInt(const char *cmd, const char *expect, int *value_r, int timeout, codepoint_t where, int lines, bool flush)
{
  bool result = false;
  if (timeout < 0) timeout = modem_timeout_default;
  modemWaitBufferMutex(HERE/*CODEPOINT(where)*/);
  modemSendExpect(cmd, expect, modem_response_buf, modem_response_max, timeout, lines, CODEPOINT(where), flush);
  if (strspn(modem_response_buf,"0123456789")>0) {
    if (value_r) *value_r = atoi(modem_response_buf);
    result = true;
  }
  modemReleaseBufferMutex(HERE/*CODEPOINT(where)*/);
  return result;
}

bool TraitModem::modemSendExpectInlineInt(const char *cmd, const char *expect, int *value_r, char delimiter, int timeout, codepoint_t where)
{
  bool result = false;
  if (timeout < 0) timeout = modem_timeout_default;
  MODEM_CHAT_TRACE(where, "modemSendExpectInlineInt SEND[%s] EXPECT[%s] (timeout %dms)", cmd?cmd:"", expect?expect:"", timeout);
  modemWaitBufferMutex(HERE/*CODEPOINT(where)*/);
  modemSend(cmd);
  int count = modemGetReply(modem_response_buf, modem_response_max, timeout, 0, strlen(expect), where, /*NO flush*/false);
  if (count == 0) {
    MODEM_CHAT_TRACE(where, "modemSendExpectInlineInt got no reply");
    modemReleaseBufferMutex(CODEPOINT(where));
    return false;
  }
  MODEM_CHAT_TRACE(where, "modemSendExpectInlineInt got expected reply leader of %d: [%s]", count, modem_response_buf);

  unsigned long start = millis();
  unsigned long timebox = start + timeout;
  unsigned long now = start;
  int len = -1;
  bool done = false;
  count=0;

  while ((!done) && (now <= timebox)) {
    wdtReset(HERE);

    while (!done && modem_stream->available()) {
      char c = modem_stream->read();
      now = millis();
      wdtReset(HERE);

      if (c==delimiter) {
	done=true;
	len = atoi(modem_response_buf);
	MODEM_CHAT_TRACE(where, "modemSendExpectInlineInt got size marker %d", len);
	if (value_r) *value_r = len;
	continue;
      }

      modem_response_buf[count++]=c;
      modem_response_buf[count]='\0';
    }
  }
  modemReleaseBufferMutex(CODEPOINT(where));

  if (!done) {
    LEAF_NOTICE("Timeout");
    return false;
  }

  return true;
}


bool TraitModem::modemSendExpectIntField(const char *cmd, const char *expect, int field_num, int *value_r, char separator, int timeout, codepoint_t where, bool flush)
{
  if (timeout < 0) timeout = modem_timeout_default;
  if (field_num == 0) return modemSendExpectInt(cmd, expect, value_r, timeout, CODEPOINT(where));

  modemWaitBufferMutex(HERE/*CODEPOINT(where)*/);
  modemSendExpect(cmd, expect, modem_response_buf, modem_response_max, timeout, 1, CODEPOINT(where), flush);

  char *pos = modem_response_buf;
  int field = 1;

  while (field < field_num) {
    pos = strchr(pos, separator);
    if (!pos) {
      LEAF_ALERT("Field %d not found (exhausted at %d)", field_num, field);
      modemReleaseBufferMutex(CODEPOINT(where));
      return false;
    }
    ++pos;
    ++field;
    LEAF_NOTICE("Field %d == [%s]", field, pos);
  }
  LEAF_NOTICE("Selected field is [%s]", pos);
  char *end;
  if (end = strchr(pos, separator)) {
    *end = '\0';
  }
  LEAF_NOTICE("Trimmed field is [%s]", pos);

  if (value_r) *value_r = atoi(pos);
  modemReleaseBufferMutex(CODEPOINT(where));
  return true;
}

String TraitModem::modemSendExpectQuotedField(const char *cmd, const char *expect, int field_num, int separator, int timeout, codepoint_t where)
{
  if (timeout < 0) timeout = modem_timeout_default;
  if (!modemWaitBufferMutex(HERE/*CODEPOINT(where)*/)) {
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
      modemReleaseBufferMutex(CODEPOINT(where));
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
      modemReleaseBufferMutex(CODEPOINT(where));
      return String("");
    }
    *next = '\0';
  }
  else {
    // unquoted string, ends at the next separator
    next = strchr(pos, separator);
    if (next) *next = '\0';
  }
  modemReleaseBufferMutex(CODEPOINT(where));
  return String(pos);
}

bool TraitModem::modemSendExpectIntPair(const char *cmd, const char *expect, int *value_r, int *value2_r,int timeout, int lines, codepoint_t where)
{
  if (timeout < 0) timeout = modem_timeout_default;
  bool result = false;
  modemWaitBufferMutex(HERE/*CODEPOINT(where)*/);
  modemSendExpect(cmd, expect, modem_response_buf, modem_response_max, timeout, lines, CODEPOINT(where));
  char *comma = strchr(modem_response_buf, ',');
  if (comma) {
    *comma++='\0';
    if (value_r) *value_r = atoi(modem_response_buf);
    if (value2_r) *value2_r = atoi(comma);
    result = true;
  }
  else {
    if (strcmp(modem_response_buf, "ERROR")!=0) {
      LEAF_WARN("Did not find expected int pair in [%s]", modem_response_buf);
      if (value_r) *value_r = atoi(modem_response_buf); // might be an error code
    }
  }
  modemReleaseBufferMutex(CODEPOINT(where));
  return result;
}

int TraitModem::modemGetReplyOfSize(char *resp, int size, int timeout, int trace, codepoint_t where)
{
  if (timeout < 0) timeout = modem_timeout_default;
  unsigned long start = millis();
  unsigned long now;
  int got = 0;

  modem_stream->setTimeout(timeout);
  do {
    int p = modem_stream->readBytes(resp+got, size);
    //resp[got+p]='\0';
    if (trace>=0) {
      __LEAF_DEBUG__(trace, "getReplyOfSize got %d/%d bytes", p,size);
    }
    got += p;
    if (got >= size) {
       break;
     }
    now = millis();
  } while (now < (start+timeout));

  if ((trace>=0) && (trace <= getDebugLevel())) {
    StreamString buf;
    buf.write((uint8_t *)resp, got);
    int line = 1;
    while (buf.available()) {
      String s = buf.readStringUntil('\n');
      __LEAF_DEBUG__(trace, "    reply line %03d: %s", line, s.c_str());
      ++line;
    }
  }

  return (got);
}

void TraitModem::modemChat(Stream *console_stream, bool echo)
{
  char c = '\0';
  char prev = '\0';
  bool chat = true;
  // Enter a loop relaying data from console to modem (for testing the
  // modem or doing manual setup)
  console_stream->println("Modem chat session, type '<LF> .' to exit");
  if (!modemWaitPortMutex(HERE)) {
    LEAF_ALERT("Modem port busy");
    return;
  }
  modem_disabled = true;
  while (chat) {
    wdtReset(HERE);
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
#ifndef ESP8266
    yield();
#endif
  }
  modem_disabled = false;
  modemReleasePortMutex(HERE);
}

bool TraitModem::modemCheckURC()
{
  static unsigned long last_urc_log = 0;
  int level = L_TRACE;
  if (!last_urc_log || (millis() > (last_urc_log + 2000))) {
    last_urc_log=millis();
    level= L_INFO;
  }
  LEAF_ENTER(level);

/*
  if ((millis()-modem_last_cmd) < 1000) {
    // the modem was recently sent a command, stay out of its way
    LEAF_TRACE("modemCheckURC: suppressed mid transaction");
    return false;
  }
*/

  int avail = 0;
  do {
    //
    // don't call wait here, do hold with no timeout, because URC check can+should be
    // delayed if the modem is in the middle of some other operation
    //
    if (!modemHoldPortMutex(HERE, 0, true)) {
      if (!modem_disabled) {
	LEAF_DEBUG("modemCheckURC: modem is busy");
      }
      // somebody else is talking to the modem right now
      LEAF_BOOL_RETURN(false);
    }
    avail = modem_stream->available();
    if (avail == 0) {
      // There's no input from the modem.  This is probably fine but
      // check if it is awake just in case the little blighter has gone to sleep on us
      unsigned long now = millis();
      modemReleasePortMutex(HERE);
      if (modem_probe_at_urc && (now > (last_urc_probe + modem_urc_probe_interval))) {
	last_urc_probe = now;
	modemProbe(HERE, MODEM_PROBE_QUICK);
      }
      continue;
    }

    LEAF_INFO("Modem avail = %d", avail);
    modem_response_buf[0]='\0';
    wdtReset(HERE);

    int count = modemGetReply(modem_response_buf, modem_response_max,-1,1,0,HERE,false);
    modemReleasePortMutex(HERE);
    if (count==1 && modem_response_buf[0]=='\0') {
      // Got a weird null
      LEAF_INFO("Got a weird NUL from the modem");
      break;
    }
    else {
      LEAF_INFO("Got modem reply of %d bytes", count);
      if (getDebugLevel()>=L_INFO) {
	DumpHex(L_ALERT, "Here's what we got", modem_response_buf, count);
      }
    }

    // Strip off any leading OKs
    while ((count > 4) && (strncmp(modem_response_buf, "OK\r\n", 4)==0)) {
      memmove(modem_response_buf, modem_response_buf+4, count+1);
      count -= 4;
    }

    if (strlen(modem_response_buf) == 0) {
      // this is whitespace that a previous interaction ought to have ceonsumed
      LEAF_WARN("Hmmn, URC is empty after whitespace strip, somebody didn't clean up");
    }
    else if (strcmp(modem_response_buf, "OK")==0) {
      // this is a response a previous interaction ought to have consumed
      LEAF_WARN("Hmmn, URC is merely a stray \"OK\", someobody didn't clean up");
    }
    else {
      modemProcessURC(String(modem_response_buf));
    }
  } while (avail > 0);

  LEAF_BOOL_RETURN(true);
}


#endif
// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
