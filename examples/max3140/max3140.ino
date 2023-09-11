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
#include "leaf_max3140.h"

#include "leaf_pixel.h"
#include "abstract_app.h"

class FdxAppLeaf : public AbstractAppLeaf
{
protected: // configuration preferences

protected: // ephemeral state
  Leaf *shell_leaf = NULL;
  
  static const int buf_max = 80;
  char tx_buf[buf_max+1];
  char rx_buf[buf_max+1];
  int tx_len = 0;
  int rx_len = 0;
  int unit_id = 5;

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

    registerLeafIntValue("heartbeat_interval_sec", &heartbeat_interval_sec, "Heartbeat interval in seconds");
    
    registerCommand(HERE, "send", "Send data on RS485");

    LEAF_LEAVE;
  }

  virtual void start(void) {
    AbstractAppLeaf::start();

    message("pixel", "set/color/0", "green");
    send_buffer("boot", 4);
  }

  void send_buffer(const char *buf, int len) {
    LEAF_ENTER_INT(L_NOTICE, len);
    
    message("pixel", "cmd/flash/0", "blue");
    LEAF_NOTIDUMP_AT(HERE, "SEND", buf, len);

    // prepend unit number, append newline
    char send_buf[80];
    int pos = 0;
    pos += snprintf(send_buf, sizeof(send_buf)-pos, "%d ", unit_id);
    for (int i=0; (i<len) && (pos < (sizeof(send_buf)-2)); i++) {
      send_buf[pos++] = buf[i];
    }
    send_buf[pos++] = '\n';
    send_buf[pos] = '\0';
    
    LEAF_NOTIDUMP_AT(HERE, "RAW SEND", send_buf, pos);

    message("max3140", "cmd/send", String(buf,pos));

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

    unsigned long now = millis();
    if ((heartbeat_interval_sec > 0) && (now > (last_heartbeat + 1000*heartbeat_interval_sec))) {
      send_heartbeat();
      last_heartbeat = now;
    }
  }

  virtual bool mqtt_receive(String type, String name, String topic, String payload, bool direct=false) {
    LEAF_HANDLER(L_INFO);

    WHEN("recv", {
      message("pixel", "cmd/flash/0", "blue");
      Serial.print(payload);
    })
    else if (!handled) {
      handled = AbstractAppLeaf::mqtt_receive(type, name, topic, payload, direct);
    }
    
    LEAF_HANDLER_END;
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
  new ShellLeaf("shell", "Stacx CLI"),

  //(new IpEspLeaf("wifi","prefs"))->inhibit(),
  //(new PubsubEspAsyncMQTTLeaf("wifimqtt","prefs,wifi"))->inhibit(),
  (new IpNullLeaf("nullip", "fs")),
  (new PubsubNullLeaf("nullmqtt", "nullip,fs")),

  new     PixelLeaf("pixel",      LEAF_PIN(PIXEL_PIN), PIXEL_COUNT, 0),
  new     MAX3140Leaf("max3140", "app"),

  (new FdxAppLeaf("app", "fs,pixel,max3140"))->setTrace(L_NOTICE),

  NULL
};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
