#ifdef ESP32
#include "esp_system.h"
#include <Update.h>
#endif

#include <HardwareSerial.h>
#include "leaf_ip_abstract.h"
#include "sim7000.h"
#include "sim7000client.h"

//@***************************** constants *******************************

#define TIME_SOURCE_NONE 0
#define TIME_SOURCE_GPS  1
#define TIME_SOURCE_LTE  2

#define SIM7000_PWR_ON 1
#define SIM7000_PWR_OFF 0

#define SIM7000_SLEEP 1
#define SIM7000_WAKE 0

//@******************************* globals *********************************

//
//@************************* class IpSim7000Leaf ***************************
//
// This class encapsulates a simcom SIM7000 nbiot/catm1 modem
//

class IpSim7000Leaf : public AbstractIpLeaf
{
public:
  IpSim7000Leaf(String name, String target="", int uart=2, int baud=115200, int rxpin=14, int txpin=15, uint32_t options=SERIAL_8N1, byte keypin=-1, byte sleeppin=-1, bool run = true) : AbstractIpLeaf(name,target) {
    LEAF_ENTER(L_INFO);
    this->uart = uart;
    this->run = run;
    this->modem = new Sim7000Modem();

    this->baud = baud;
    this->pin_rx = rxpin;
    this->pin_tx = txpin;
    this->serial_options = options;
    this->pin_key = keypin;
    this->pin_sleep = sleeppin;
    replybuffer[0]=asyncbuffer[0]='\0';
    for (int i=0; i<8; i++) {
      this->clients[i] = NULL;
    }

#ifdef NOTYET
    // Turn off internal wifi
    esp_wifi_stop();
#endif

    LEAF_LEAVE;
  }
  virtual void setup();
  virtual void start();
  virtual void loop();
  virtual bool mqtt_receive(String type, String name, String topic, String payload, bool direct=false);
  void schedule_init(int seconds);
  void schedule_reconnect(int seconds);
  bool init_modem();
  void powerOff();
  virtual void pre_sleep(int duration=0);
  virtual bool connect(String reason="");
  bool connect_cautious(bool verbose=false);
  bool connect_fast();

  virtual bool disconnect(bool with_reboot = false);
  bool install_cert();
  float readVcc();
  bool netStatus();
  bool connStatus();
  Sim7000Modem *get_modem() { return modem; };
  bool postJpeg(char *url, const uint8_t *data, int len);
  int getRssi(void);
  virtual void pull_update(String url);
  virtual void rollback_update(String url);
  bool gpsConnected() { return gps_fix; }

  virtual bool isPresent() { return (modem_type!=-1);  }

  Sim7000Client *tcpConnect(String host, int port, int *slot_r)
  {
    int slot;
    for (slot = 0; slot < 8; slot++) {
      if (clients[slot] == NULL) {
	break;
      }
    }
    if (slot >= 8) {
      LEAF_ALERT("No free slots");
      return NULL;
    }
    clients[slot] = new Sim7000Client(this->modem, slot);
    if (slot_r) *slot_r=slot;
    return clients[slot];
  }

  void tcpRelease(Sim7000Client *client)
  {
    int slot = client->getSlot();
    clients[slot] = NULL;
  }

  virtual bool ftpPut(String host, String user, String pass, String path, const char *buf, int buf_len)
  {
    LEAF_ENTER(L_NOTICE);
    LEAF_NOTICE("Uploading %s of size %d", path.c_str(), buf_len);
    bool result = modem->ftpPut(host.c_str(), user.c_str(), pass.c_str(), path.c_str(), buf, buf_len);
    LEAF_RETURN(result);
  }

  virtual int ftpGet(String host, String user, String pass, String path, char *buf, int buf_max)
  {
    LEAF_ENTER(L_NOTICE);
    int result = modem->ftpGet(host.c_str(), user.c_str(), pass.c_str(), path.c_str(), buf, buf_max);
    LEAF_RETURN(result);
  }


protected:
  //
  // Network resources
  //
  HardwareSerial *modemPort=NULL;
  Sim7000Modem *modem=NULL;
  Sim7000Client *clients[8];
  bool ipConnectNotified=false;
  String apn = "telstra.m2m";
  int modem_retries = 5;
  int reconnect_timeout = NETWORK_RECONNECT_SECONDS;
  uint32_t connect_time = 0;
  uint32_t disconnect_time = 0;
  bool enableRTC = true;
  bool enableGPS = true;
  bool enableSMS = true;
  bool autoconnect = true;
  bool autoinit = true;
  bool gpsEnabled = false;
  bool gpsAlways = true;
  bool dstEnabled = false;
  int timeSource = 0;
  unsigned long lteReconnectAt=0;
  unsigned long lteInitAt=0;
  bool abort_no_service = false;
  bool abort_no_signal = true;


  byte uart;
  unsigned long baud;
  uint32_t serial_options;
  int8_t pin_rx;
  int8_t pin_tx;
  int8_t pin_key=-1;
  int8_t pin_sleep=-1;

  int modem_type = -1;

  char imei[16] = {0}; // Use this for device ID
  int rssi;
  uint16_t battLevel = 0; // Battery level (percentage)
  float latitude, longitude, speed_kph, heading, altitude, second;
  time_t location_timestamp = 0;
  time_t location_refresh_interval = 86400;
  uint16_t year;
  uint8_t month, day, hour, minute;
  unsigned long gps_check_interval = 600*1000;
  unsigned long last_gps_check = 0;
  bool gps_fix = false;
  uint8_t counter = 0;
  //char PIN[5] = "1234"; // SIM card PIN

  char replybuffer[257]; // For reading replies to AT commans
  char asyncbuffer[513]; // For reading stuff coming through UART, like MQTT subs

  bool use_ssl = false;
  bool use_sleep = false;

  bool process_async(char *asyncbuffer);
  bool process_sms(int index=-1);
  bool parseGPS(String gps);
  bool maybeEnableGPS();
  bool pollNetworkTime();
  bool parseNetworkTime(String Time);
  void publishTime(void);
};

void IpSim7000Leaf::setup()
{
  AbstractIpLeaf::setup();
  LEAF_ENTER(L_INFO);
  WiFi.mode( WIFI_MODE_NULL );
  StorageLeaf *prefs_leaf = (StorageLeaf *)get_tap("prefs");

  if (prefs_leaf) {
    String value;

    value = prefs_leaf->get("lte_autoinit");
    if (value.length()) autoinit = (value=="on");

    value = prefs_leaf->get("lte_autoconnect");
    if (value.length()) autoconnect = (value=="on");

    value = prefs_leaf->get("lte_apn");
    if (value.length()) apn = value;

    value = prefs_leaf->get("lte_retries");
    if (value.length()) modem_retries = value.toInt();

    value = prefs_leaf->get("lte_reconnect");
    if (value.length()) reconnect_timeout = value.toInt();

    value = prefs_leaf->get("lte_use_sleep");
    if (value.length()) use_sleep = (value=="on");

    value = prefs_leaf->get("lte_abrtnosig");
    if (value.length()) abort_no_signal = (value=="on");

    value = prefs_leaf->get("lte_abrtnosrv");
    if (value.length()) abort_no_service = (value=="on");

    value = prefs_leaf->get("lte_gps_al");
    if (value.length()) gpsAlways = (value=="on");

    value = prefs_leaf->get("lte_gps_ena");
    if (value.length()) enableGPS = (value=="on");

    value = prefs_leaf->get("lte_loc_ts");
    if (value.length()) location_timestamp = value.toInt();

    value = prefs_leaf->get("lte_loc_ref");
    if (value.length()) location_refresh_interval = value.toInt();
  }

  LEAF_LEAVE;
}

void IpSim7000Leaf::start()
{
  Leaf::start();
  LEAF_ENTER(L_INFO);
  if (autoinit) {
    ipCommsState(TRY_MODEM,HERE);
    init_modem();
  }
  LEAF_LEAVE;
}

void IpSim7000Leaf::schedule_init(int seconds)
{
  LEAF_ENTER(L_NOTICE);


  LEAF_NOTICE("Will initialise modem in %d sec", seconds);
  lteInitAt = millis()+(seconds*1000);
  LEAF_LEAVE;
}

void IpSim7000Leaf::schedule_reconnect(int seconds)
{
  LEAF_ENTER(L_NOTICE);

  LEAF_NOTICE("Will retry connection in %d sec", (int)seconds);
  lteReconnectAt = millis()+(seconds*1000);
  LEAF_LEAVE;
}

bool IpSim7000Leaf::init_modem()
{
  LEAF_ENTER(L_NOTICE);
  publish("_ip_init", "initialise modem");

  debug_flush = true;
  disable_bod();

  if (!this->modemPort) {
    // do this only once, reuse if modem_init() is called again
    LEAF_NOTICE("Setting up UART1 %d baud, options=0x%x rx=pin%d tx=pin%d", baud, serial_options, pin_rx, pin_tx);
    this->modemPort = new HardwareSerial(uart);
    //modemPort->begin(baud, serial_options, pin_rx, pin_tx);
    modemPort->begin(115200, SERIAL_8N1, 14, 15);
  }

  if (pin_sleep >= 0) {
    //LEAF_INFO("Deasserting sleep pin");
    pinMode(pin_sleep, OUTPUT);
    digitalWrite(pin_sleep, SIM7000_WAKE);
  }

  if (pin_key >= 0) {
    LEAF_INFO("Powering on modem");
    pinMode(pin_key, OUTPUT);
    digitalWrite(pin_key, SIM7000_PWR_OFF);
    delay(100); // For SIM7000
    digitalWrite(pin_key, SIM7000_PWR_ON);
    //LEAF_INFO("Wait 10s for modem powerup");
    // TODO only at cold boot
    for (int nap=0; nap<100; nap++) {
      delay(100);
      while (modemPort->available()) {
	char c = modemPort->read();
	modemPort->write(c);
      }
    }
    //LEAF_INFO("Finished waiting for powerup");
  }

  int retry = 1;
  while (true) {
    LEAF_NOTICE("Initialising Cellular Modem (attempt %d)", retry);
    if (! modem->begin(*modemPort/*,5000*/)) {
      LEAF_ALERT("Couldn't find Modem");
      post_error(POST_ERROR_MODEM, 3);
      ERROR("Modem unresponsive");

      ++retry;

      if (retry == 3) {
	LEAF_ALERT("Modem unresponsive, might need a reboot)");
	publish("_ip_nophys", "modem unresponsive");
      }

      if (retry > modem_retries) {
	LEAF_ALERT("Giving up on modem detection (try again in a few minutes)");
	modem_type = -1;
	if (reconnect_timeout) {
	  schedule_init(reconnect_timeout);
	}

	enable_bod();
	LEAF_RETURN(false);
      }
    }
    else {
      break;
    }
  }

  modem_type = modem->type();
  //LEAF_INFO("Modem type code is %d", modem_type);
  const char *type_str = NULL;
  switch (modem_type) {
    case SIM800L:
      type_str = "SIM800L";  break;
    case SIM800H:
      type_str = "SIM800H"; break;
    case SIM808_V1:
      type_str = "SIM808 (v1)"; break;
    case SIM808_V2:
      type_str = "SIM808 (v2)"; break;
    case SIM5320A:
      type_str = "SIM5320A (American)"; break;
    case SIM5320E:
      type_str = "SIM5320E (European)"; break;
    case SIM7000A:
      type_str = "SIM7000A (American)"; break;
    case SIM7000C:
      type_str = "SIM7000C (Chinese)"; break;
    case SIM7000E:
      type_str = "SIM7000E (European)"; break;
    case SIM7000G:
      type_str = "SIM7000G (Global)"; break;
    case SIM7500A:
      type_str = "SIM7500A (American)"; break;
    case SIM7500E:
      type_str = "SIM7500E (European)"; break;
    default:
      type_str = "???"; break;
  }
  LEAF_INFO("Found modem: type %d (%s)", modem_type, type_str);

  //modem->sendCheckReply("AT&F","OK");
  //reboot();

  uint8_t imeiLen = modem->getIMEI(imei);
  bool success = false;
  if (imeiLen > 0) {
    LEAF_INFO("Module IMEI: %s", imei);
    success = true;
  }

  if (use_sleep) {
    modem->sendCheckReply("AT+CSCLK=1","OK");
  }
  else {
    modem->sendCheckReply("AT+CSCLK=0","OK");
  }

  debug_flush = false;

  enable_bod();
  LEAF_LEAVE;
  return(success);
}

void IpSim7000Leaf::powerOff(void)
{
  LEAF_ENTER(L_NOTICE);

  if (pin_sleep >= 0) {
    LEAF_NOTICE("Activating SIMcom modem sleep with pin %d", pin_sleep);
    gpio_hold_en((gpio_num_t)pin_sleep);
    digitalWrite(pin_sleep, SIM7000_SLEEP);
  }

  //LEAF_NOTICE("Turning modem the heck off for test");
  //modem->sendCheckReply("AT+CPOWD=0","OK");

}

void IpSim7000Leaf::pre_sleep(int duration)
{
  LEAF_NOTICE("Putting LTE modem to lower power state");

  if (use_sleep) {
    //LEAF_INFO("telling modem to allow sleep");
    modem->sendCheckReply("AT+CSCLK=1","OK");
  }

  //LEAF_INFO("Disconnect LTE");
  disconnect();

  powerOff();
}



int IpSim7000Leaf::getRssi(void)
{
  //LEAF_DEBUG("Check signal strength");
  if (!run || (modem_type < 0)) {
    //LEAF_INFO("Not connected, rssi -99");
    return -99;
  }

  modem->sendExpectStringReply("AT+CSQ","+CSQ: ", replybuffer, 500, sizeof(replybuffer));
  int rssi = -atoi(replybuffer);
  //LEAF_INFO("Got RSSI [%s] %d", replybuffer, rssi);
  return rssi;
}

void IpSim7000Leaf::loop()
{
  LEAF_ENTER(L_TRACE);

  static bool first = true;

  if (run && first && autoconnect && (modem_type >= 0)) {
    connect(String("first autoconnect"));
    first = false;
  }
  else if (!run && !first) {
    // we were stopped, reconnect if restarted
    first = true;
  }

  if (lteInitAt && (millis() >= lteInitAt)) {
    lteInitAt=0;
    LEAF_NOTICE("Delayed LTE modem initialisation");
    init_modem();
  }

  if ((lteReconnectAt!=0) && (millis() >= lteReconnectAt)) {
    lteReconnectAt=0;
    LEAF_NOTICE("Initiate LTE reconnect");
    connect(String("reconnect timer"));
  }

  if (ipConnectNotified != connected) {
    // Tell other interested leaves that IP has changed state
    // (delay this to loop because if done in setup other leaves may not be
    // ready yet).
    // todo: make leaf::start part of core api
    if (connected) {

      time_t now;
      time(&now);

      if (now < 100000000) {
	// looks like the clock is in the "seconds since boot" epoch, not
	// "seconds since 1970".   Ask the netowrk for an update
	LEAF_ALERT("Clock appears stale, setting time from cellular network");
	pollNetworkTime();
      }

      //LEAF_INFO("Publishing ip_connect %s", ip_addr_str.c_str());
      publish("_ip_connect", ip_addr_str);

      // check for any pending sms
      process_sms();

    }
    else {
      publish("_ip_disconnect", "");
    }
    ipConnectNotified = connected;
  }

  AbstractIpLeaf::loop();

  if (run) {
    bool available = modem->available();

    if (modem->available()) {
      //LEAF_DEBUG("Modem said something");
      int i = strlen(asyncbuffer);
      while (modem->available() &&
	     (strlen(asyncbuffer) < (sizeof(asyncbuffer)-1))) {
	char c = modem->read();
	if (i==0 && (c=='\r'||c=='\n')) {
	  continue;
	}
	asyncbuffer[i++] = c;
	asyncbuffer[i] = '\0';
      }

      if ((asyncbuffer[i-1] != '\n') && (asyncbuffer[i-1] != '\r')) {
	// Wait for rest of input
	//LEAF_DEBUG("Waiting for more async input [%s]", asyncbuffer);
      }
      else {
	// We met end of line chop off newlines
	//LEAF_INFO("Got a complete line at [%s]", asyncbuffer);
	while ((i>1) && (asyncbuffer[i-1]=='\n' || asyncbuffer[i-1]=='\r')) {
	  asyncbuffer[--i] = '\0';
	}
	process_async(asyncbuffer);
	asyncbuffer[0]='\0';
      }
    }
  }

  // If we don't have a GPS fix yet, keep polling every 60sec.
  // If we do have a fix, poll at the configured rate (less often)
  if (enableGPS && !gpsEnabled && location_timestamp && location_refresh_interval) maybeEnableGPS();

  unsigned long interval = gps_fix?gps_check_interval:60*1000;

  if (run && connected && enableGPS && gpsEnabled && ((last_gps_check + interval) <= millis())) {
    ipPollGPS();
    last_gps_check = millis();

    // do a backstop SMS check too
    process_sms();
  }


  LEAF_LEAVE;
}

void IpSim7000Leaf::publishTime(void)
{
    time_t now;
    struct tm localtm;
    char ctimbuf[32];
    time(&now);
    localtime_r(&now, &localtm);
    strftime(ctimbuf, sizeof(ctimbuf), "%FT%T", &localtm);
    mqtt_publish("status/time", ctimbuf);
    mqtt_publish("status/time_source", (timeSource==TIME_SOURCE_GPS)?"GPS":(timeSource==TIME_SOURCE_LTE)?"LTE":"SELF");
}


bool IpSim7000Leaf::mqtt_receive(String type, String name, String topic, String payload, bool direct)
{
  LEAF_ENTER(L_INFO);
  bool handled = false;

  //LEAF_INFO("sim7000 mqtt_receive %s %s => %s", type.c_str(), name.c_str(), topic.c_str());

  if (topic == "cmd/lte_sleep") {
    //LEAF_DEBUG("sim7000 lte_sleep");
    if (pin_sleep >= 0) {
      LEAF_NOTICE("Setting SLEEP pin (%d)", pin_sleep);
      digitalWrite(pin_sleep, SIM7000_SLEEP);
    }
    handled = true;
  }
  else if (topic == "cmd/lte_key") {
    //LEAF_DEBUG("sim7000 lte_key");
    if (pin_key >= 0) {
      LEAF_NOTICE("Pulsing pin (%d) LOW (LTE key)", pin_key);
      digitalWrite(pin_key, SIM7000_PWR_OFF);
      delay(250);
      digitalWrite(pin_key, SIM7000_PWR_ON);
    }
    handled = true;
  }
  else if (topic == "cmd/lte_test") {
    modem->chat((payload=="1")?true:false);
  }
  else if (topic == "get/lte_time") {
    publishTime();
  }
  else if (topic == "cmd/lte_reboot") {
    //LEAF_DEBUG("sim7000 lte_reboot");
    modem->sendCheckReply("AT+CFUN=1,1","OK");
    delay(5000);
    reboot();
    handled = true;
  }
  else if (topic == "cmd/lte_poweroff") {
    //LEAF_DEBUG("sim7000 lte_poweroff");
    if (pin_sleep >= 0) {
      LEAF_NOTICE("Turning LTE power off");
      if (payload == "0") {
	// fast poweroff
	modem->sendCheckReply("AT+CPOWD=0","OK");
      }
      else {
	// graceful poweroff
	modem->sendCheckReply("AT+CPOWD=1","OK");
      }
    }
    handled = true;
  }
  else if (topic == "cmd/lte_init") {
    //LEAF_DEBUG("sim7000 lte_init");
    init_modem();
    handled = true;
  }
  else if (topic == "cmd/lte_connect") {
    //LEAF_DEBUG("sim7000 lte_connect");
    connect("cmd");
    handled = true;
  }
  else if (topic == "cmd/lte_connect_verbose") {
    //LEAF_DEBUG("sim7000 lte_connect_verbose");
    connect("cmd_verbose");
    handled = true;
  }
  else if (topic == "cmd/lte_disconnect") {
    //LEAF_DEBUG("sim7000 lte_disconnect");
    disconnect();
    handled = true;
  }
  else if (topic == "cmd/lte_reconnect") {
    //LEAF_DEBUG("sim7000 lte_reconnect");
    reconnect("reconnect cmd");
    handled = true;
  }
  else if (topic == "cmd/lte_status") {
    char status[32];
    uint32_t secs;
    if (connected) {
      secs = (millis() - connect_time)/1000;
      snprintf(status, sizeof(status), "online %d:%02d", secs/60, secs%60);
    }
    else {
      secs = (millis() - disconnect_time)/1000;
      snprintf(status, sizeof(status), "offline %d:%02d", secs/60, secs%60);
    }
    mqtt_publish("status/lte_status", status);
  }
  else if (topic == "cmd/lte_settings") {
    char msg[256];
    snprintf(msg, sizeof(msg),"{\n"
	     "\"lte_autoconnect\": \"%s\",\n"
	     "\"lte_retries\": %d,\n"
	     "\"lte_reconnect\": %d,\n"
	     "\"lte_use_sleep\": \"%s\",\n"
	     "\"lte_abortnosig\": \"%s\",\n"
	     "\"lte_abortnosrv\": \"%s\",\n"
	     "\"lte_gps_al\": \"%s\",\n"
	     "\"lte_gps_ena\": \"%s\",\n"
	     "\"lte_loc_ts\": %lu,\n"
	     "\"lte_log_ref\": %lu\n}",
	     TRUTH(autoconnect),modem_retries,reconnect_timeout,
	     TRUTH(use_sleep),TRUTH(abort_no_signal),
	     TRUTH(abort_no_service),
	     TRUTH(gpsAlways),
	     TRUTH(enableGPS),
	     (unsigned long)location_timestamp,
	     (unsigned long)location_refresh_interval
      );
    mqtt_publish("status/lte_settings", msg);
  }
  else if (topic == "cmd/lte_signal") {
    //LEAF_INFO("Check signal strength");
    modem->sendExpectStringReply("AT+CSQ","+CSQ: ", replybuffer, 500, sizeof(replybuffer));
    mqtt_publish("status/lte_signal", replybuffer);
  }
  else if (topic == "cmd/lte_network") {
    //LEAF_INFO("Check network status");
    modem->sendExpectStringReply("AT+CPSI?","+CPSI: ", replybuffer, 500, sizeof(replybuffer));
    mqtt_publish("status/lte_network", replybuffer);
  }
  else if (topic == "set/gps_enabled") {
    gpsEnabled = (payload=="on");
  }
  else if (topic == "set/lte_sleep") {
    //LEAF_DEBUG("sim7000 lte_sleep");
    if (pin_sleep >= 0) {
      if (payload == "1") {
	LEAF_NOTICE("Setting pin %d (LTE Sleep)", pin_sleep);
	digitalWrite(pin_sleep, SIM7000_SLEEP);
      }
      else {
	LEAF_NOTICE("Setting pin %d (LTE Wake)", pin_sleep);
	digitalWrite(pin_sleep, SIM7000_WAKE);
      }
    }
    else {
      LEAF_ALERT("LTE Sleep pin is not configured");
    }

    handled = true;
  }
  else if (topic == "set/lte_key") {
    //LEAF_DEBUG("sim7000 lte_key");
    if (pin_key >= 0) {
      if (payload == "1") {
	LEAF_NOTICE("Setting lte_power key (%d) on", pin_key);
	digitalWrite(pin_key, SIM7000_PWR_ON);
      }
      else {
	LEAF_NOTICE("Setting lte power key (%d) off ", pin_key);
	digitalWrite(pin_key, SIM7000_PWR_OFF);
      }
    }
    else {
      LEAF_ALERT("LTE Key pin is not configured");
    }

    handled = true;
  }
  else if (topic == "cmd/lte_time") {
    //LEAF_DEBUG("sim7000 cmd/lte_time");
    pollNetworkTime();
  }
  else if (topic == "get/lte_imei") {
    //LEAF_DEBUG("sim7000 get/lte_imei");
    char imei_buf[20];
    int len = modem->getIMEI(imei_buf);
    mqtt_publish("status/lte_imei", imei_buf);
  }
  else if (topic == "get/sms_count") {
    //LEAF_DEBUG("sim7000 get/sms_count");
    int count = modem->getNumSMS();
    mqtt_publish("status/smscount", String(count));
  }
  else if (topic == "get/sms") {
    //LEAF_DEBUG("sim7000 get/sms");
    char sms_buf[161];
    uint16_t sms_len;
    char sdr_buf[32];
    char status_buf[256];
    int msg_index = payload.toInt();
    if (modem->readSMS(msg_index, sms_buf, sizeof(sms_buf), &sms_len)) {
      LEAF_NOTICE("Got SMS message: %d %s", msg_index, sms_buf);
      DumpHex(L_INFO, "RAW SMS", sms_buf, sms_len);
      if (modem->getSMSSender(msg_index, sdr_buf, sizeof(sdr_buf))) {
	snprintf(status_buf, sizeof(status_buf), "{\"msgno\":%d, \"sender\",\"%s\", \"message\":\"%s\"}",
		 msg_index, sdr_buf, sms_buf);
      }
      else {
	snprintf(status_buf, sizeof(status_buf), "{\"msgno\":%d \"sender\",\"unknown\", \"message\":\"%s\"}",
		 msg_index, sms_buf);
      }
      mqtt_publish("status/sms", status_buf);
    }
    else {
      mqtt_publish("status/sms", "{\"error\":\"not found\"}");
    }
  }
  else if (topic == "cmd/sms") {
    //LEAF_DEBUG("sim7000 cmd/sms");
    String number;
    String message;
    int pos = payload.indexOf(",");
    if (pos > 0) {
      number = payload.substring(0,pos);
      message = payload.substring(pos+1);
      LEAF_NOTICE("Send SMS number=%s msg=%s", number.c_str(), message.c_str());
      modem->sendSMS(number.c_str(), message.c_str());
    }
  }
  else if (topic == "cmd/at") {
    char sendbuffer[80];
    char replybuffer[80];
    strlcpy(sendbuffer, payload.c_str(), sizeof(sendbuffer));
    //LEAF_DEBUG("Send AT command %s", sendbuffer);
    modem->sendExpectStringReply(sendbuffer, "", replybuffer);
    mqtt_publish("status/at", String(replybuffer));
  }
  else {
    handled = Leaf::mqtt_receive(type, name, topic, payload, direct);
  }

  LEAF_LEAVE;
  return handled;
}

bool IpSim7000Leaf::process_sms(int msg_index)
{
  int first,last;
  if (msg_index < 0) {
    // process all unread sms
    first = 0;
    last = modem->getNumSMS();
    LEAF_NOTICE("Modem holds %d SMS messages", last);
  }
  else {
    first=msg_index;
    last=msg_index+1;
  }

  for (msg_index = first; msg_index < last; msg_index++) {
    char sms_buf[161];
    uint16_t sms_len;
    char sdr_buf[32];

    if (modem->readSMS(msg_index, sms_buf, sizeof(sms_buf), &sms_len)) {
      LEAF_NOTICE("Got SMS message: %d %s", msg_index, sms_buf);
      DumpHex(L_INFO, "RAW SMS", sms_buf, sms_len);
      if (modem->getSMSSender(msg_index, sdr_buf, sizeof(sdr_buf))) {
	LEAF_NOTICE("SMS message %d is from %s", msg_index, sdr_buf);
      }
      else {
	sdr_buf[0] = '\0';
      }

      // Delete the SMS *BEFORE* processing, else we might end up in a
      // reboot loop forever.   DAMHIKT.
      modem->deleteSMS(msg_index);


      int sep;
      String request = sms_buf;
      String reply = "";
      String command = "";
      String topic;
      String payload;

      // Process each line of the SMS by treating it as MQTT input
      do {
	if ((sep = request.indexOf("\r\n")) >= 0) {
	  command = request.substring(0,sep);
	  request.remove(0,sep+2);
	}
	else {
	  command = request;
	  request = "";
	}
	LEAF_INFO("Processing one line of SMS as a Bogo-MQTT: %s", command.c_str());
	if (!command.length()) continue;

	if ((sep = command.indexOf(' ')) >= 0) {
	  topic = command.substring(0,sep);
	  payload = command.substring(sep+1);
	}
	else {
	  topic = command;
	  payload = "1";
	}

	pubsubLeaf->_mqtt_receive(topic, payload, PUBSUB_LOOPBACK);
	reply += pubsubLeaf->getLoopbackBuffer() + "\r\n";


      } while (request.length());

      // We have now accumulated results in reply
      if (sdr_buf[0]) {
	const int sms_max = 140;
	if (reply.length() < 140) {
	  LEAF_NOTICE("Send SMS reply %s <= %s", sdr_buf, reply.c_str());
	  modem->sendSMS(sdr_buf, reply.c_str());
	}
	else {
	  LEAF_NOTICE("Send Chunked SMS reply %s <= %s", sdr_buf, reply.c_str());
	  String msg;
	  while (reply.length()) {
	    if (reply.length() > 140) {
	      msg = reply.substring(0,140);
	      reply = reply.substring(140);
	    }
	    else {
	      msg = reply;
	      reply = "";
	    }
	    LEAF_INFO("Send SMS chunk %s", msg);
	    modem->sendSMS(sdr_buf, msg.c_str());
	  }
	}
      }
    }
  }
  return true;
}



bool IpSim7000Leaf::process_async(char *asyncbuffer)
{
  LEAF_ENTER(L_INFO);
  LEAF_NOTICE("Asynchronous Modem input: [%s]", asyncbuffer);

  String Message= String(asyncbuffer);

  while (Message.startsWith("OK\r\n")) {
      Message.remove(0,4);
      Message.trim();
  }
  char c;
  while (Message.length() && ((c=Message.charAt(0)) >= 0x7F)) {
    LEAF_ALERT("Drop high-bit crud from modem input (0x%02x)", (int)c);
    Message.remove(0,1);
    Message.trim();
  }
  while (Message.startsWith("\r\n")) {
      Message.remove(0,2);
      Message.trim();
  }


  if (Message == "OK") {
    // ignore OK
  }
  else if (Message == "+PDP: DEACT") {
    LEAF_ALERT("Lost LTE connection");
    connected = false;
    disconnect_time = millis();
    ipCommsState(WAIT_IP,HERE);
    post_error(POST_ERROR_LTE, 3);
    ERROR("Lost LTE");
    post_error(POST_ERROR_LTE_LOST, 0);
    if (autoconnect) {
      connect(String("PDP DEACT"));
    }
  }
  else if (Message.startsWith("+SMSUB: ")) {
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
      LEAF_ALERT("Received MQTT Message Topic=[%s] Payload=[%s]", Topic.c_str(), Payload.c_str());

      pubsubLeaf->_mqtt_receive(Topic, Payload);
    }
    else {
      LEAF_ALERT("Payload separator not found in MQTT SMSUB input: [%s]", Message.c_str());
    }
  }
  else if (Message == "+SMSTATE: 0") {
    //LEAF_ALERT("Lost MQTT connection");
    pubsubLeaf->disconnect(false);
    ipCommsState(WAIT_PUBSUB, HERE);
  }
  else if (Message.startsWith("+PSUTTZ") || Message.startsWith("DST: ")) {
    /*
      [DST: 0
      *PSUTTZ: 21/01/24,23:10:29","+40",0]
     */
    LEAF_NOTICE("Got timestamp from network");
    parseNetworkTime(Message);

    if (!connected && autoconnect) {
      // Having received time means we are back in signal.  Reconnect.
      connect("PSUTTZ");
    }
  }
  else if (Message.startsWith("+SAPBR 1: DEACT")) {
    LEAF_NOTICE("Lost TCP connection");
  }
  else if (Message.startsWith("+APP PDP: DEACTIVE")) {
    LEAF_NOTICE("Lost application layer connection");
  }
  else if (Message.startsWith("+CMTI: \"SM\",")) {
    LEAF_ALERT("Got SMS");
    int idx = Message.indexOf(',');
    if (idx > 0) {
      int msg_id = Message.substring(idx+1).toInt();
      process_sms(msg_id);
    }
  }
  else if (Message == "CONNECT OK") {
    //LEAF_DEBUG("Ignore CONNECT OK");
  }
  else if (Message.startsWith("+RECEIVE,")) {
    int slot = Message.substring(9,10).toInt();
    int size = Message.substring(11).toInt();
    LEAF_NOTICE("Got TCP data for slot %d of %d bytes", slot, size);
    if (clients[slot]) {
      clients[slot]->readToBuffer(size);
    }
  }
  else {
    DumpHex(L_ALERT, "Unhandled asynchronous input:", Message.c_str(), Message.length());
    LEAF_RETURN(false);
  }

  LEAF_RETURN(true);
}

bool IpSim7000Leaf::pollNetworkTime()
{
  char date_buf[40];

  LEAF_ENTER(L_INFO);
  if (modem->sendExpectStringReply("AT+CCLK?", "+CCLK: \"", date_buf, 1000, sizeof(date_buf))) {
    parseNetworkTime(String(date_buf));
  }
  LEAF_RETURN(true);
}

bool IpSim7000Leaf::parseNetworkTime(String Time)
{
  LEAF_ENTER(L_NOTICE);
  bool result = false;

  /*
   * eg       [DST: 0
   *          *PSUTTZ: 21/01/24,23:10:29","+40",0]
   *
   *
   */
  int index = -1;
  index = Time.indexOf("DST: ");
  if (index >= 0) {
    if (index > 0) Time.remove(0, index+5);
    char dstflag = Time[0];
    //LEAF_INFO("LTE network supplies DST flag %c", dstflag);
    dstEnabled = (dstflag=='1');
  }

  index = Time.indexOf("*PSUTTZ: ");
  if (index >= 0) {
    Time.remove(0, index+9);
  }
  else {
    index = Time.indexOf("+CCLK: \"");
    if (index >= 0) {
      Time.remove(0, index+8);
    }
  }

  LEAF_INFO("LTE network supplies TTZ record [%s]", Time.c_str());
  // We should now be looking at a timestamp like 21/01/24,23:10:29
  //                                              01234567890123456
  //
  struct tm tm;
  struct timeval tv;
  struct timezone tz;
  time_t now;
  char ctimbuf[32];

  if ((Time.length() > 16) &&
      (Time[2]=='/') &&
      (Time[5]=='/') &&
      (Time[8]==',') &&
      (Time[11]==':') &&
      (Time[14]==':')) {
    tm.tm_year = Time.substring(0,2).toInt() + 100;
    tm.tm_mon = Time.substring(3,5).toInt()-1;
    tm.tm_mday = Time.substring(6,8).toInt();
    tm.tm_hour = Time.substring(9,11).toInt();
    tm.tm_min = Time.substring(12,14).toInt();
    tm.tm_sec = Time.substring(15,17).toInt();
    tz.tz_minuteswest = -Time.substring(18,20).toInt()*15; // americans are nuts
    tz.tz_dsttime = 0;
    tv.tv_sec = mktime(&tm)+60*tz.tz_minuteswest;
    tv.tv_usec = 0;
    //LEAF_INFO("Parsed time Y=%d M=%d D=%d h=%d m=%d s=%d z=%d",
    //	(int)tm.tm_year, (int)tm.tm_mon, (int)tm.tm_mday,
    //		(int)tm.tm_hour, (int)tm.tm_min, (int)tm.tm_sec, (int)tz.tz_minuteswest);
    time(&now);
    if (now != tv.tv_sec) {
      settimeofday(&tv, &tz);
      strftime(ctimbuf, sizeof(ctimbuf), "%FT%T", &tm);
      LEAF_INFO("Clock differs from LTE by %d sec, set time to %s.%06d+%02d%02d", (int)abs(now-tv.tv_sec), ctimbuf, tv.tv_usec, -tz.tz_minuteswest/60, abs(tz.tz_minuteswest)%60);
      time(&now);
      LEAF_NOTICE("Unix time is now %llu %s", (unsigned long long)now, ctime(&now));
      timeSource = TIME_SOURCE_LTE;
      publish("status/time", ctimbuf);
      //maybeEnableGPS();
    }
  }

  LEAF_RETURN(result);

}


bool IpSim7000Leaf::parseGPS(String gps)
{
  LEAF_ENTER(L_NOTICE);
  bool result = false;

  if (gps.startsWith("1,1,")) {
    LEAF_NOTICE("GPS fix %s", gps.c_str());
    mqtt_publish("status/gps", gps.c_str());
    /*
     * eg 1,1,20201012004322.000,-27.565879,152.936990,16.700,0.00,103.8,1,,0.6,0.9,0.7,,25,9,3,,,34,,
     *
     *  1<GNSS run status>,
     *  2<Fix status>,
     *  3<UTC date & Time>,
     *  4<Latitude>,
     *  5<Longitude>,
     *  6<MSL Altitude>,
     *  7<Speed Over Ground>,
     *  8<Course Over Ground>,
     *  9<Fix Mode>
     * 10<Reserved1>
     * 11<HDOP>
     * 12<PDOP>
     * 13<VDOP>
     * 14<Reserved2>
     * 15<GNSS Satellites in View>
     * 16<GNSS Satellites Used>
     * 17<GLONASS Satellites Used>
     * 18<Reserved3>
     * 19<C/N0 max>
     * 20<HPA>
     * 21<VPA>
     */
    int wordno = 0;
    String word;
    float fv;
    bool locChanged = false;
    struct tm tm;
    struct timeval tv;
    struct timezone tz;
    time_t now;
    char ctimbuf[32];

    while (gps.length()) {
      //LEAF_DEBUG("Looking for next word in %s", gps.c_str());
      ++wordno;
      int pos = gps.indexOf(',');
      if (pos < 0) {
	word = gps;
	gps = "";
      }
      else {
	word = gps.substring(0,pos);
	gps.remove(0,pos+1);
      }

      //LEAF_DEBUG("GPS word %d is %s", wordno, word.c_str());
      switch (wordno) {
      case 1: // run
      case 2: // fix
	break;
      case 3: // date
	tm.tm_year = word.substring(0,4).toInt()-1900;
	tm.tm_mon = word.substring(4,6).toInt()-1;
	tm.tm_mday = word.substring(6,8).toInt();
	tm.tm_hour = word.substring(8,10).toInt();
	tm.tm_min = word.substring(10,12).toInt();
	tm.tm_sec = word.substring(12).toInt();
	tv.tv_sec = mktime(&tm);
	tv.tv_usec = word.substring(14).toInt()*1000;
	tz.tz_minuteswest = 0;
	tz.tz_dsttime = 0;
	time(&now);
	if (now != tv.tv_sec) {
	  settimeofday(&tv, &tz);
	  strftime(ctimbuf, sizeof(ctimbuf), "%FT%T", &tm);
	  LEAF_NOTICE("Clock differs from GPS by %d sec, set time to %s.%06d", (int)abs(now-tv.tv_sec), ctimbuf, tv.tv_usec);
	  timeSource = TIME_SOURCE_GPS;
	  publish("status/time", word);
	  //maybeEnableGPS();
	}
	break;
      case 4: // lat
	fv = word.toFloat();
	if (fv != latitude) {
	  latitude = fv;
	  //LEAF_DEBUG("Set latitude %f",latitude);
	  locChanged = true;
	}
	break;
      case 5: // lon
	fv = word.toFloat();
	if (fv != longitude) {
	  longitude = fv;
	  //LEAF_DEBUG("Set longitude %f",longitude);
	  locChanged = true;
	}
	break;
      case 6: // alt
	fv = word.toFloat();
	if (fv != altitude) {
	  altitude = fv;
	  //LEAF_DEBUG("Set altitude %f",altitude);
	}
	break;
      case 7: // speed
	fv = word.toFloat();
	if (fv != speed_kph) {
	  speed_kph = fv;
	  //LEAF_DEBUG("Set speed %fkm/h",speed_kph);
	}
	break;
      case 8: // course
	fv = word.toFloat();
	if (fv != heading) {
	  heading = word.toFloat();
	  //LEAF_DEBUG("Set heading %f",heading);
	}
	break;
      default: // dont care about rest
	break;
      }
    }

    if (locChanged) {
      publish("status/location", String(latitude,6)+","+String(longitude,6));
      StorageLeaf *prefs_leaf = (StorageLeaf *)get_tap("prefs");
      if (prefs_leaf) {
	time_t now = time(NULL);
	LEAF_NOTICE("Storing time of GPS fix %llu", (unsigned long long)now);
	prefs_leaf->put("lte_loc_ts", String(now));
      }

    }
    gps_fix = true;
    if (!gpsAlways) {
      LEAF_NOTICE("Disable GPS after lock");
      modem->enableGPS(false);

    }
  }
  else {
    LEAF_NOTICE("No GPS fix (%s)", gps.c_str());
    gps_fix = false;
  }

  LEAF_RETURN(result);
}

bool IpSim7000Leaf::maybeEnableGPS()
{
  if (location_timestamp &&
      location_refresh_interval &&
      (time(NULL) >= (location_timestamp + location_refresh_interval))
    ) {
    LEAF_ALERT("GPS location is stale, seeking a new lock");
    gpsEnabled = true;
    modem->enableGPS(true);
    return true;
  }
  return false;
}


bool IpSim7000Leaf::connect(String reason)
{
  LEAF_ENTER(L_INFO);
  LEAF_NOTICE("CONNECT (%s)", reason.c_str());
  bool result = false;
  ipCommsState(TRY_IP, HERE);
  disable_bod();
  if (reason != "cmd_verbose") {
    result = connect_fast();
  }
  if (!result) {
    result = connect_cautious((reason=="cmd_verbose"));
  }
  enable_bod();

  LEAF_RETURN(result);
}


bool IpSim7000Leaf::connect_fast()
{
  char cmdbuf[80];
  int retry;
  int i;

  LEAF_ENTER(L_NOTICE);

  connected = false;

  //
  // Set the value of a bunch of setttings
  //
  static const char *cmds[][2] = {
    {"E0", "command echo suppressed"},
    {NULL, NULL}
  };

  for (i=0; cmds[i][0] != NULL; i++) {
    snprintf(cmdbuf, sizeof(cmdbuf), "AT%s", cmds[i][0]);
    //LEAF_INFO("Set %s using %s", cmds[i][1], cmdbuf);
    modem->sendCheckReply(cmdbuf,"");
  }

  if (enableGPS) {
    //LEAF_INFO("Check GPS state");
    modem->sendExpectIntReply("AT+CGNSPWR?","+CGNSPWR: ", &i);
    retry = 1;
    i = modem->enableGPS(true);
    if (!i) {
      LEAF_ALERT("Failed to turn on GPS");
    } else {
      //LEAF_INFO("GPS is enabled.");
      gpsEnabled = true;
    }

  }

  if (enableSMS) {
    modem->sendCheckReply("AT+CNMI=2,1",""); // will send +CMTI on SMS Recv
  }

  //LEAF_INFO("Check signal strength");
  modem->sendExpectStringReply("AT+CSQ","+CSQ: ", replybuffer, 500, sizeof(replybuffer));
  if (abort_no_signal && atoi(replybuffer) == 99) {
    LEAF_ALERT("NO LTE SIGNAL");
    LEAF_LEAVE;
    return false;
  }

  //LEAF_INFO("Check Carrier status");
  modem->sendExpectStringReply("AT+CPSI?","+CPSI: ", replybuffer, 500, sizeof(replybuffer));
  if (abort_no_service && strstr(replybuffer, "NO SERVICE")) {
    LEAF_ALERT("NO LTE NETWORK: %s", replybuffer);
    LEAF_LEAVE;
    return false;
  }

  //LEAF_INFO("Check LTE status");
  modem->sendExpectStringReply("AT+CSTT?","+CSTT: ", replybuffer, 180000, sizeof(replybuffer));

  if ((String(replybuffer).indexOf(apn) < 0) // no APN
      //||
      //(String(replybuffer).indexOf("\"\"") >0 ) // no IP address
    )
  {
    //LEAF_INFO("Start LTE");
    snprintf(cmdbuf, sizeof(cmdbuf), "AT+CSTT=\"%s\"", apn.c_str());
    if (!modem->sendCheckReply(cmdbuf,"OK", 5000)) {
      LEAF_ALERT("Modem LTE is not cooperating.  Abort.");
      LEAF_LEAVE;
      return false;
    }
  }

  //LEAF_INFO("GET IP Address");
  if (modem->sendExpectStringReply("AT+CNACT?","+CNACT: 1,", replybuffer, 2000, sizeof(replybuffer),5)) {
    replybuffer[strlen(replybuffer)-1] = '\0'; //trim trailing quote
    ip_addr_str = String(replybuffer+1);
  }
  else {
    //LEAF_INFO("No IP, time to Enable IP");
    if (!modem->waitfor("AT+CIICR",2000,replybuffer, sizeof(replybuffer)) ||
	(strstr(replybuffer, "+CME ERROR")) ) {
      LEAF_ALERT("Error bringing up IP");
      LEAF_LEAVE;
      return false;
    }
    int retry = 0;
    int max_retry = 2;
    while (retry < max_retry) {
      if (modem->sendExpectStringReply("AT+CNACT?","+CNACT: 1,", replybuffer, 2000, sizeof(replybuffer))) {
	replybuffer[strlen(replybuffer)-1] = '\0'; //trim trailing quote
	ip_addr_str = String(replybuffer+1);
      }
      else {
	// snooze for a bit and retry
	LEAF_NOTICE("Still no IP, check again in 1s");
	delay(1000);
	++retry;
      }
    }
    if (retry >= max_retry) {
      LEAF_ALERT("No IP after %d retries", retry);
      return false;
    }
  }

  LEAF_NOTICE("Connection complete (IP=%s)", ip_addr_str.c_str());
  connected = true;
  lteReconnectAt = 0;
  connect_time = millis();
  ipCommsState(WAIT_PUBSUB, HERE);
  publish("_ip_connect", ip_addr_str);

  LEAF_LEAVE;
  return true;
}


bool IpSim7000Leaf::connect_cautious(bool verbose)
{
  char cmdbuf[80];
  int retry;
  int i;

  LEAF_ENTER(L_NOTICE);
  lteReconnectAt = 0;

  connected = false;
  if (wake_reason == "poweron") {

    //LEAF_INFO("Check functionality");
    if (!modem->sendExpectIntReply("AT+CFUN?","+CFUN: ", &i, 10000,80,true)) {
      ALERT("Modem is not answering commands");
      if (!init_modem()) {
	LEAF_RETURN(false);
      }
    }

    if (i != 1) {
      //LEAF_INFO("Set functionality mode 1 (full)");
      modem->setFunctionality(1);
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
      {"+CIPQSEND=1", "Quick-send IP mode"},
      {"+CMGR=1", "SMS Text-mode"},
      {"+CSCLK=0", "disable slow clock (sleep) mode"},
      {NULL, NULL}
    };

    for (i=0; cmds[i][0] != NULL; i++) {
      snprintf(cmdbuf, sizeof(cmdbuf), "AT%s", cmds[i][0]);
      //LEAF_INFO("Set %s using %s", cmds[i][1], cmdbuf);
      modem->sendCheckReply(cmdbuf,"");
    }

    //LEAF_INFO("Set network APN to [%s]", apn.c_str());
    // fixme: need to re-merge the accelerando branch that allows RAM strings
    modem->setNetworkSettings(F("telstra.m2m")); // can add username and password here if required
    //modem->setNetworkSettings(apn.c_str()); // can add username and password here if required

    //
    // Confirm the expected value of a bunch of setttings
    // Items marked with '*' are only done when verbose=true
    //
    static const char *queries[][2] = {
      {"CMEE?", "*errors reporting mode"},
      {"CGMM", "*Module name"},
      {"CGMR", "*Firmware version"},
      {"CGSN", "*IMEI serial number"},
      {"CNUMM", "*Phone number"},
      {"CLTS?", "*Clock mode"},
      {"CCLK?", "*Clock"},
      {"CSCLK?", "*Slow Clock (sleep) mode status"},
      {"COPS?", "Operator status"},
      {"CSQ", "Signal strength"},
      {"CPSI?", "Signal info"},
      {"CBANDCFG?", "*Radio band config"},
      {"CBAND?", "*Radio band status"},
      {"CNMP?", "*Packet data"},
      {"CMNB?", "*LTE Mode"},
      {"CREG?", "*Registration status"},
      {"CGREG?", "*GSM Registration status"},
      {"CGATT?", "*Network attach status"},
      {"CGACT?", "*PDP context state"},
      {"CGPADDR", "PDP address"},
      {"CGDCONT?", "Network settings"},
      {"CGNAPN", "*NB-iot status"},
      {"CGNSINF", "*GPS fix status"},
      {"CIPSTATUS=0", "*IP slot 0 status"},
      {"CIPSTATUS=1", "*IP slot 1 status"},
      {"CIPSTATUS=2", "*IP slot 2 status"},
      {"CIPSTATUS=3", "*IP slot 3 status"},
      // {"COPS=?", "Available cell operators"}, // takes approx 2 mins
      {NULL,NULL}
    };

    for (i=0; queries[i][0] != NULL; i++) {
      const char *desc = queries[i][1];
      if (desc[0]=='*') {
	// Items marked with '*' are only done when verbose=true
	if (verbose) {
	  ++desc; // we are in verbose mode, do this item, but chop off the asterisk
	}
	else {
	  continue; // skip non-verbose item
	}
      }
      snprintf(cmdbuf, sizeof(cmdbuf), "AT+%s", queries[i][0]);
      //LEAF_INFO("Check %s with %s", queries[i][1], cmdbuf);
      modem->waitfor(cmdbuf,5000,replybuffer, sizeof(replybuffer));
      LEAF_NOTICE("Modem %s: %s", queries[i][1], replybuffer);
    }

    // check sim status

    //LEAF_INFO("Check Carrier status");
    modem->sendExpectStringReply("AT+CPIN?","+C", replybuffer, 500, sizeof(replybuffer));
    if (strstr(replybuffer, "ERROR")) {
      LEAF_ALERT("SIM ERROR: %s", replybuffer);
      if (autoconnect && reconnect_timeout) schedule_reconnect(reconnect_timeout);
      post_error(POST_ERROR_LTE, 3);
      post_error(POST_ERROR_LTE_NOSIM, 0);
      ERROR("NO SIM");
      LEAF_LEAVE;
      return false;
    }

    //LEAF_INFO("Set LED blinky modes");
    modem->setNetLED(true, 1, 100, 100); // 1=offline on/off, mode, timer_on, timer_off
    modem->setNetLED(true, 2, 40, 4960); // 2=online on/off, mode, timer_on, timer_off
    modem->setNetLED(true, 3, 40, 4960); // 3=PPP on/off, mode, timer_on, timer_off

    if (enableGPS) {
      //LEAF_INFO("Check GPS state");
      modem->sendExpectIntReply("AT+CGNSPWR?","+CGNSPWR: ", &i);
      retry = 1;
      i = modem->enableGPS(true);
      if (!i) {
	LEAF_ALERT("Failed to turn on GPS");
      } else {
	//LEAF_INFO("GPS is enabled.");
	gpsEnabled = true;
      }

      if (gpsEnabled) {
	if (modem->sendExpectStringReply("AT+CGNSINF", "+CGNSINF: ", replybuffer, 10000, sizeof(replybuffer))) {
	  // Expect +CGNSINF:1,1,20201001052749.000,-27.565964,152.937217,32.600,0.00,0.0,1,,3.5,3.6,1.0,,16,4,1,,,32,,
	  parseGPS(String(replybuffer));
	}
      }
    }

    if (enableSMS) {
      modem->sendCheckReply("AT+CNMI=2,1",""); // will send +CMTI on SMS Recv
    }
  }

  if (!modem->wirelessConnStatus()) {
    LEAF_INFO("Opening wireless connection");
    modem->openWirelessConnection(true);
    connected = modem->wirelessConnStatus();
    if (connected) {
      LEAF_NOTICE("Wireless connection is (now) established (connected=true)");
    }
  }
  else {
    //LEAF_INFO("Wireless connection is already established (set connected=true)");
    lteReconnectAt = 0;
    connected = true;
  }

  if (!connected) {

    //LEAF_INFO("Check Carrier status");
    modem->sendExpectStringReply("AT+CPSI?","+CPSI: ", replybuffer, 500, sizeof(replybuffer));
    if (abort_no_service && strstr(replybuffer, "NO SERVICE")) {
      LEAF_ALERT("NO LTE NETWORK: %s", replybuffer);
      //modem->sendCheckReply("AT+CFUN=1,1","OK");
      if (autoconnect && reconnect_timeout) schedule_reconnect(reconnect_timeout);
      post_error(POST_ERROR_LTE, 3);
      post_error(POST_ERROR_LTE_NOSERV, 0);
      ERROR("NO SERVICE");
      LEAF_RETURN(false);
    }

    //LEAF_INFO("Check signal strength");
    modem->sendExpectStringReply("AT+CSQ","+CSQ: ", replybuffer, 500, sizeof(replybuffer));
    if (abort_no_signal && atoi(replybuffer) == 99) {
      LEAF_ALERT("NO LTE SIGNAL");
      //modem->sendCheckReply("AT+CFUN=1,1","OK");
      post_error(POST_ERROR_LTE, 3);
      post_error(POST_ERROR_LTE_NOSIG, 0);
      ERROR("NO SIGNAL");
      if (autoconnect && reconnect_timeout) schedule_reconnect(reconnect_timeout);
      LEAF_RETURN(false);
    }

    //LEAF_INFO("Check task status");
    modem->sendExpectStringReply("AT+CSTT?","+CSTT: ", replybuffer, 180000, sizeof(replybuffer));

    if ((String(replybuffer).indexOf(apn) < 0) // no APN
	//||
	//(String(replybuffer).indexOf("\"\"") >0 ) // no IP address
      )
    {
      //LEAF_INFO("Start task");
      snprintf(cmdbuf, sizeof(cmdbuf), "AT+CSTT=\"%s\"", apn.c_str());
      if (!modem->sendCheckReply(cmdbuf,"OK", 30000)) {
	LEAF_ALERT("Modem LTE is not cooperating.  Retry later.");
	//modem->sendCheckReply("AT+CFUN=1,1","OK");
	post_error(POST_ERROR_LTE, 3);
	post_error(POST_ERROR_LTE_NOCONN, 0);
	ERROR("CONNECT FAILED");
	LEAF_LEAVE;
	return false;
      }
      //if (!modem->waitfor(cmdbuf,60000, replybuffer, sizeof(replybuffer))) {
      //  LEAF_ALERT("Network not connected");
      //}
    }


    //LEAF_INFO("Enable IP");
    modem->sendCheckReply("AT+CIICR","");
//  if (!modem->waitfor("AT+CIICR",60000,replybuffer, sizeof(replybuffer))) {
//    LEAF_ALERT("Error bringing up IP");
//  }

    //LEAF_INFO("Wait for IP address");
    modem->sendCheckReply("AT+CIFSR","");
//  if (!modem->waitfor("AT+CIFSR",60000,replybuffer, sizeof(replybuffer))) {
//    LEAF_ALERT("No IP address");
//  }

    /*
      while (!netStatus()) {
      LEAF_ALERT("Not connected cell network, retrying...");
      delay(2000); // Retry every 2s
      }
    */

    //LEAF_INFO("Checking connection status");
    if (!modem->wirelessConnStatus()) {
      //LEAF_INFO("Enabling wireless connection");
      modem->openWirelessConnection(true);
      retry = 1;
      while (!modem->wirelessConnStatus()) {
	int delay_sec = 1<<retry;
	LEAF_ALERT("Failed to enable connection, wait %ds...", delay_sec);
	delay(1000*delay_sec); // Retry every 2s, then back off
	++retry;
	if (retry > 5) {
	  LEAF_ALERT("Modem Connection is not cooperating.  Reset modem and retry later.");
	  modem->sendCheckReply("AT+CFUN=1,1","OK");
	  post_error(POST_ERROR_LTE, 3);
	  post_error(POST_ERROR_LTE_NOCONN, 0);
	  ERROR("LTE Connect fail");
	  LEAF_LEAVE;
	  return false;
	}

      }
      LEAF_NOTICE("Enabled data, set connected=true");
      connected = true;
      lteReconnectAt = 0;
    }
  }


  //LEAF_INFO("GET IP Address");
  if (modem->sendExpectStringReply("AT+CNACT?","+CNACT: 1,", replybuffer, 180000, sizeof(replybuffer))) {
    replybuffer[strlen(replybuffer)-1] = '\0'; //trim trailing quote
    ip_addr_str = String(replybuffer+1);
  }

  if (use_ssl && (wake_reason == "poweron")) {
    LEAF_INFO("Checking SSL certificate status");
    static char certbuf[10240];

    if (!modem->readFile("cacert.pem", certbuf, sizeof(certbuf))) {
      LEAF_INFO("No CA cert present, loading.");
      if (!install_cert()) {
	LEAF_ALERT("Failed to load CA cert.");
      }
    }
  }

  LEAF_NOTICE("Connection complete (IP=%s)", ip_addr_str.c_str());
  connected = true;
  connect_time = millis();
  ipCommsState(WAIT_PUBSUB, HERE);

  LEAF_LEAVE;
  return true;
}


bool IpSim7000Leaf::disconnect(bool with_reboot)
{
  LEAF_ENTER(L_NOTICE);

  publish("_ip_disconnect", "");
  ipConnectNotified = false;

  LEAF_NOTICE("Turn off GPS");
  modem->enableGPS(false);
  gpsEnabled = false;

  LEAF_NOTICE("Turn off LTE");
  modem->openWirelessConnection(false);
  connected = false;
  disconnect_time = millis();

  if (with_reboot) {
    LEAF_ALERT("Rebooting modem");
    modem->sendCheckReply("AT+CFUN=1,1","OK");
  }

  return true;
}


bool IpSim7000Leaf::install_cert()
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

  if (!modem->writeFileVerify("cacert.pem", cacert)) {
    Serial.println("CA cert write failed");
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


  if (!modem->writeFileVerify("client.crt", clientcert)) {
    Serial.println("Client cert write failed");
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

  if (!modem->writeFileVerify("client.key", clientkey)) {
    Serial.println("Client key write failed");
  }


  return false;
}

// Read the module's power supply voltage
float IpSim7000Leaf::readVcc() {
  // Read battery voltage
  if (!modem->getBattVoltage(&battLevel)) Serial.println("Failed to read batt");
  else Serial.printf("battery = %dmV", (int)battLevel);

  // Read LiPo battery percentage
  // Note: This will NOT work properly on the LTE shield because the voltage
  // is regulated to 3.6V so you will always read about the same value!
//  if (!modem->getBattPercent(&battLevel)) Serial.println(F("Failed to read batt"));
//  else Serial.print(F("BAT % = ")); Serial.print(battLevel); Serial.println(F("%"));

  return battLevel;
}

bool IpSim7000Leaf::netStatus() {
  int n = modem->getNetworkStatus();

  Serial.print(F("Network status ")); Serial.print(n); Serial.print(F(": "));
  if (n == 0) Serial.println(F("Not registered"));
  if (n == 1) Serial.println(F("Registered (home)"));
  if (n == 2) Serial.println(F("Not registered (searching)"));
  if (n == 3) Serial.println(F("Denied"));
  if (n == 4) Serial.println(F("Unknown"));
  if (n == 5) Serial.println(F("Registered roaming"));

  if (!(n == 1 || n == 5)) return false;
  else return true;
}

bool IpSim7000Leaf::connStatus() {
  return modem->wirelessConnStatus();
}


bool IpSim7000Leaf::postJpeg(char *url, const uint8_t *data, int len) {
  uint16_t status, resplen;

  modem->HTTP_POST_start(url, F("application/jpeg"), data, len, &status, &resplen);
  LEAF_NOTICE("POST result %d len %d", (int)status, (int)resplen);
  modem->HTTP_POST_end();
  return (status==200);
}

void IpSim7000Leaf::pull_update(String url)
{
  MD5Builder checksum;
  checksum.begin();

  const bool test = true;

  modem->httpGetWithCallback(
    url.c_str(),
    [test](int status, size_t len, const uint8_t *hdr) -> bool
    {
      NOTICE("HTTP header code=%d len=%lu hdr=%s", status, (unsigned long)len, (char *)hdr);
      if (!test) Update.begin(len, U_FLASH);
      return true;
    },
    [&checksum,test](const uint8_t *buf, size_t len) -> bool
    {
      NOTICE("HTTP chunk callback len=%lu", (unsigned long)len);
      checksum.add((uint8_t *)buf, len);
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
  checksum.calculate();

  LEAF_NOTICE("HTTP file digest [%s]", checksum.toString().c_str());
  if (!test) Update.end();
}

void IpSim7000Leaf::rollback_update(String url)
{
  if (Update.canRollBack()) {
    LEAF_ALERT("Rolling back to previous version");
    if (Update.rollBack()) {
      LEAF_NOTICE("Rollback succeeded.  Rebooting.");
      delay(1000);
      reboot();
    }
    else {
      LEAF_ALERT("Rollback failed");
    }
  }
  else {
    LEAF_ALERT("Rollback is not possible");
  }
}




// Local Variables:
// mode: C++
// c-basic-offset: 2
// End:
