#pragma STACX_BOARD espressif:esp32:corinda

#include "variant_pins.h"
#undef HELLO_PIXEL

#include "defaults.h"
#include "config.h"
#include "stacx.h"

//
// Example stack: A bidirectional uart sniffer using two hardware serial ports
//
//
#include "leaf_fs.h"
#include "leaf_fs_preferences.h"
#include "leaf_ip_esp.h"
#include "leaf_pubsub_mqtt_esp.h"
#include "leaf_shell.h"
#include "leaf_ip_null.h"
#include "leaf_pubsub_null.h"

#include "leaf_pixel.h"
#include "abstract_app.h"


#define PIN_TX_DI  -1
#define PIN_TX_RO  -1
#define PIN_TX_NRE 0
#define PIN_TX_DE  10

#define PIN_RX_DI  D3
#define PIN_RX_RO  D4
#define PIN_RX_DE  D2
#define PIN_RX_NRE D1
wn  k


class FdxAppLeaf : public AbstractAppLeaf
{
protected: // configuration preferences

protected: // ephemeral state
  HardwareSerial *port_rx=NULL;
  HardwareSerial *port_tx=NULL;
  Leaf *shell_leaf = NULL;


  static const int buf_max = 80;
  char tx_buf[buf_max+1];
  char rx_buf[buf_max+1];
  int tx_len = 0;
  int rx_len = 0;
  bool enable_tx = true;
  bool enable_rx = true;
  int unit_id = 4;

  unsigned long last_heartbeat = 0;
  int heartbeat_interval_sec = 67;

public:
  FdxAppLeaf(String name, String target)
    : AbstractAppLeaf(name,target)
    , Debuggable(name)
  {
  }

  virtual void setup(void) {
    AbstractAppLeaf::setup();
    LEAF_ENTER(L_NOTICE);

    shell_leaf = Leaf::find("shell");

    registerLeafBoolValue("tx", &enable_tx, "Enable transmit");
    registerLeafBoolValue("rx", &enable_rx, "Enable receive");
    registerLeafIntValue("heartbeat_interval_sec", &heartbeat_interval_sec, "Heartbeat interval in seconds");

    registerCommand(HERE, "send", "Send data on RS485");

    if (enable_tx) {
      LEAF_NOTICE("Port Tx (A/B) transmit on uart 0 (tx=%d,rx=%d), uses %d for DE, %d for /RE",
		  PIN_TX_DI,PIN_TX_RO,PIN_TX_DE,PIN_TX_NRE);

      port_tx = new HardwareSerial(0);
      port_tx->begin(9600, SERIAL_8N1, PIN_TX_DI, PIN_TX_RO);
      pinMode(PIN_TX_DE, OUTPUT);
      digitalWrite(PIN_TX_DE, LOW); // off initially
      pinMode(PIN_TX_NRE, OUTPUT);
      digitalWrite(PIN_TX_NRE, HIGH); // do not receive our own spew
    }

    if (enable_rx) {
      LEAF_NOTICE("Port Rx (A/B) receives on uart 1 (tx=%d,rx=%d), uses %d for DE, %d for /RE", PIN_RX_DI, PIN_RX_RO, PIN_RX_DE, PIN_RX_NRE);
      port_rx = new HardwareSerial(1);

      port_rx->begin(9600, SERIAL_8N1, PIN_RX_DI, PIN_RX_RO);
      pinMode(PIN_RX_DE, OUTPUT);
      digitalWrite(PIN_RX_DE, LOW);  // do not transmit on this pair
      pinMode(PIN_RX_NRE, OUTPUT);
      digitalWrite(PIN_RX_NRE, LOW); // always remain in receive mode
    }


    send_buffer("boot", 4);
    LEAF_LEAVE;
  }

  virtual void start(void) {
    AbstractAppLeaf::start();

    message("pixel", "set/color/0", "black");
  }

  void send_buffer(const char *buf, int len) {
    LEAF_ENTER_INT(L_NOTICE, len);

    message("pixel", "cmd/flash/0", "blue");
    LEAF_NOTIDUMP("SEND", buf, len);
    LEAF_NOTICE("TX_DE pin %d HIGH", PIN_TX_DE);

    // prepend unit number, append newline
    char send_buf[80];
    int pos = 0;
    pos += snprintf(send_buf, sizeof(send_buf)-pos, "%d ", unit_id);
    for (int i=0; (i<len) && (pos < (sizeof(send_buf)-2)); i++) {
      send_buf[pos++] = buf[i];
    }
    send_buf[pos++] = '\n';
    send_buf[pos] = '\0';

    LEAF_NOTIDUMP("RAW SEND", send_buf, pos);

    // Raise the DE line, send and flush the data, lower DE
    digitalWrite(PIN_TX_DE, HIGH);
    digitalWrite(PIN_TX_NRE, HIGH);
    delayMicroseconds(250);
    for (int i=0; i<pos; i++) {
      port_tx->write(send_buf[i]);
    }
    port_tx->flush();
    delayMicroseconds(250);
    digitalWrite(PIN_TX_DE, LOW);
    digitalWrite(PIN_TX_NRE, LOW);
    LEAF_NOTICE("TX_DE pin %d LOW", PIN_TX_DE);

    LEAF_LEAVE;
  }

  void send_tx_buf() {
    LEAF_ENTER(L_INFO);

    send_buffer(tx_buf, tx_len);
    tx_len = 0;
    LEAF_LEAVE;
  }

  void send_heartbeat() {
    LEAF_ENTER(L_INFO);
    char buf[80];

    snprintf(buf, sizeof(buf), "uptime %lu", millis()/1000);
    send_buffer(buf, strlen(buf));
    LEAF_LEAVE;
  }

  virtual void loop(void)
  {
    Leaf::loop();
    static int dir = 0;
    bool newline=false;
    bool newlineA=false;
    bool newlineB=false;
    bool unprintableA=false;
    bool unprintableB=false;
    int rcvd = 0;

    while (enable_rx && port_rx->available()) {
      char c = port_rx->read();

      if (rx_len >= buf_max) {
	rx_buf[buf_max+1]=0;
	LEAF_ALERT("Rx buffer overflow [%s]", rx_buf);
	rx_len=0;
      }

      if ((c < 32) && (c!='\r') && (c!='\n')) {
	// unprintable characters in red
	Serial.printf("\033[37;41m%02X\033[39m\033[49m", (int)c);
      }
      else if (c=='\n') {
	rx_buf[rx_len] = '\0';
	LEAF_NOTICE("RECV %d[%s]", rx_len, rx_buf);
	Serial.print("\r\n");
	rx_len = 0;
	rcvd = 0;
	continue;
      }
      else {
	// printable characters as is
	Serial.printf("\033[37;43m%c\033[39m\033[49m", (int)c);
      }
      rx_buf[rx_len++]=c;
      ++rcvd;
    }
    if (rcvd) {
      //message("pixel", "cmd/flash/0", "red");
      LEAF_NOTICE("Received %d bytes", rcvd);
      LEAF_NOTIDUMP("RX BUF NOW", rx_buf, rx_len);
    }

    if (!enable_tx) {
      // tx side is disabled
      return;
    }

    unsigned long now = millis();
    if ((heartbeat_interval_sec > 0) && (now > (last_heartbeat + 1000*heartbeat_interval_sec))) {
      send_heartbeat();
      last_heartbeat = now;
    }

    if (shell_leaf && shell_leaf->isStarted()) {
      // Don't read from console while shell active
      return;
    }

    while (Serial.available()) {
      char c = Serial.read();
      if (c=='\r') continue;
      if (c=='\b') {
	if (tx_len>0) {
	  tx_len--;
	  Serial.print("\b \b");
	}
      }
      if (c=='\n') {
	if ((tx_len >= 2) && tx_buf[0] == '~') {
	  // process a tilde code
	  switch (tx_buf[1]) {
	  case '.':
	    shell_leaf->start();
	    tx_len = 0;
	    break;
	  default:
	    Serial.println("Unhanded escape code");
	  }
	  return;
	}
	// input line is not escape, transmit it
	Serial.println();
	if (tx_len == 0) {
	  LEAF_INFO("Skip blank line");
	  return;
	}
	send_tx_buf();
      }
      else {
	if (tx_len >= buf_max) {
	  Serial.println();
	  LEAF_ALERT("Tx buffer overflow");
	  tx_len=0;
	}
	Serial.print(c);
	tx_buf[tx_len++] = c;
	tx_buf[tx_len] = 0;
      }
    }


  }

  virtual bool commandHandler(String type, String name, String topic, String payload) {
    LEAF_HANDLER(L_INFO);

    WHEN("send", {
	send_buffer(payload.c_str(), payload.length());
    })
    else if (!handled) {
      handled=Leaf::commandHandler(type, name, topic, payload);
    }

    LEAF_HANDLER_END;
  }

};


Leaf *leaves[] = {
  new FSLeaf("fs", FS_DEFAULT, FS_ALLOW_FORMAT),
  new FSPreferencesLeaf("prefs"),
  (new ShellLeaf("shell", "Stacx CLI"))->inhibit(),

  //(new IpEspLeaf("wifi","prefs"))->inhibit(),
  //(new PubsubEspAsyncMQTTLeaf("wifimqtt","prefs,wifi"))->inhibit(),
  (new IpNullLeaf("nullip", "fs")),
  (new PubsubNullLeaf("nullmqtt", "nullip,fs")),

  new     PixelLeaf("pixel",      LEAF_PIN(PIXEL_PIN), PIXEL_COUNT, 0),

  (new FdxAppLeaf("app", "fs,pixel"))->setTrace(L_NOTICE),

  NULL
};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
