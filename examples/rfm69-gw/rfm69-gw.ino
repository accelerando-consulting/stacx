#pragma STACX_BOARD espressif:esp32:ttgo-t7-v13-mini32

#include "variant_pins.h"

#undef HELLO_PIXEL
#define HELLO_PIN 22
#include "defaults.h"
#include "config.h"
#include "stacx.h"

//
// Example stack: A modbus satellite (aka sl*v*) device
//
//

#include "leaf_fs.h"
#include "leaf_fs_preferences.h"
#include "leaf_ip_esp.h"
#include "leaf_pubsub_mqtt_esp.h"
#include "leaf_shell.h"
#include "leaf_ip_null.h"
#include "leaf_pubsub_null.h"
#include "leaf_ground.h"

#include "leaf_rfm69.h"
#include "abstract_app.h"


class RfmGatewayAppLeaf : public AbstractAppLeaf
{
protected: // configuration preferences

protected: // ephemeral state
  Leaf *shell_leaf = NULL;
  unsigned long last_heartbeat = 0;
  static const int buf_max = 80;
  char tx_buf[buf_max+1];
  int tx_len = 0;
  int target_node=2;
  
  
public:
  RfmGatewayAppLeaf(String name, String target)
    : AbstractAppLeaf(name,target)
    , Debuggable(name)
  {
    do_heartbeat=true;
  }

  virtual void setup(void) {
    AbstractAppLeaf::setup();
    LEAF_ENTER(L_NOTICE);
    shell_leaf = Leaf::find("shell");

    registerLeafIntValue("heartbeat_interval", &heartbeat_interval_seconds, "Heartbeat interval in seconds");
    registerLeafIntValue("target_node", &target_node, "Default target node");
    
    LEAF_LEAVE;
  }

  virtual void start(void) {
    AbstractAppLeaf::start();

  }

  virtual void heartbeat(unsigned long uptime) {
    LEAF_ENTER(L_NOTICE);
    char buf[30];
    snprintf(buf, sizeof(buf), "gateway %lu", uptime);
    message("radio", "cmd/radio_send/255", buf);
    LEAF_LEAVE;
  }

  virtual bool mqtt_receive(String type, String name, String topic, String payload, bool direct=false) {
    LEAF_HANDLER(L_NOTICE);

    WHENPREFIX("event/recv/", {
	Serial.printf("RECV %s %s\n", topic.c_str(), payload.c_str());
    })
    else {
      handled = AbstractAppLeaf::mqtt_receive(type, name, topic, payload, direct);
    }
    LEAF_HANDLER_END;    
  }

  virtual void loop(void)
  {
    Leaf::loop();

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
	continue;
      }
      if (c=='\n') {
	Serial.println();
	if ((tx_len >= 2) && tx_buf[0] == '#') {
	  // process an escape code
	  LEAF_WARN("Escape code recognised: [%s]", tx_buf);
	  switch (tx_buf[1]) {
	  case '.':
	    LEAF_WARN("Launching command interpreter");
	    shell_leaf->start();
	    break;
	  case 'n': 
	  {
	    int n = atoi(tx_buf+1);
	    if ((n > 0) && (n < 256)) {
	      target_node = n;
	      LEAF_NOTICE("Target node %d", target_node);
	    }
	  }
	    break;
	  default:
	    LEAF_ALERT("Unhandled escape code");
	  }
	  tx_len=0; // discard the input line
	  tx_buf[0]=0;
	  return;
	}
	// input line is not escape, transmit it
	if (tx_len == 0) {
	  LEAF_INFO("Skip blank line");
	  return;
	}
	LEAF_NOTICE("SEND %d %s", target_node, tx_buf);
	message("radio", String("cmd/radio_send/")+String(target_node), String(tx_buf));
	tx_len = 0;
      }
      else {
	// An ordinarcy character to be accumulated in the buffer
	
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

};


Leaf *leaves[] = {
  new FSLeaf("fs", FS_DEFAULT, FS_ALLOW_FORMAT),
  new FSPreferencesLeaf("prefs"),
  (new ShellLeaf("shell", "# Stacx CLI"))->inhibit(),

  new IpNullLeaf("nullip", "fs"),
  new PubsubNullLeaf("nullmqtt", "nullip,fs"),

  new GroundLeaf("ground", LEAF_PIN(D0)),
  new Rfm69Leaf("radio", "app", LEAF_PIN(D8), /*IRQ=*/D4, /*RST=*/D3),
  new RfmGatewayAppLeaf("radio", "radio,fs"),
  NULL
};
