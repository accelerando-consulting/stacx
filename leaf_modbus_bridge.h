//
//@************************* class ModbusBridgeLeaf **************************
//
// This class encapsulates a generic modbus slave device
//

#include "pseudo_stream.h"

class ModbusBridgeLeaf : public Leaf
{
  PseudoStream *port_master;
  String target;
  String bridge_id;
  unsigned long ping_interval_sec = 1*60;
  unsigned long ping_timeout_sec = 10;
  unsigned long command_watchdog_sec = 20*60;
  unsigned long pingsent=0;
  unsigned long ackrecvd=0;
  unsigned long cmdrecvd=0;
  bool connected = false;

public:
  ModbusBridgeLeaf(String name,
		   String target,
		   PseudoStream *port_master)
    : Leaf("modbusBridge", name, NO_PINS)
    , Debuggable(name)
  {
    LEAF_ENTER(L_INFO);
    this->target=target;
    this->port_master = port_master;
    LEAF_LEAVE;
  }

  void setup(void) {
    Leaf::setup();
    LEAF_ENTER(L_NOTICE);
    this->install_taps(target);
    bridge_id = device_id;
    getPref("bridge_id", &bridge_id, "Identifying string to send to modbus cloud agent");
    getULongPref("modbus_bridge_ping_timeout_sec", &ping_timeout_sec, "Time to wait for response to a ping");
    getULongPref("modbus_bridge_ping_interval_sec", &ping_interval_sec, "Number of seconds of inactivity after which to senda  ping");
    getULongPref("modbus_bridge_command_watchdog_sec", &command_watchdog_sec, "Hang up if no commands in this interval");
    
    
    LEAF_LEAVE;
  }

  void start(void) 
  {
    Leaf::start();
    publish("status/bridge_id", bridge_id);
  }

  void status_pub() 
  {
    unsigned long now = millis();
    unsigned long last_rx = max(ackrecvd, cmdrecvd);
    unsigned long last_activity = max(pingsent, last_rx);
    int inactivity_sec = (now - last_activity)/1000;

    publish("status/bridge_connected", TRUTH_lc(connected));
    publish("status/bridge_last_activity", String(inactivity_sec));
  }

  void loop(void) {
    Leaf::loop();
    static int to_slave_len = 0;
    static int from_slave_len = 0;

    if (port_master->fromSlave.length()) {
      LEAF_NOTICE("%d bytes in the buffer from slave", port_master->fromSlave.length());
    }

    if (to_slave_len != port_master->toSlave.length()) {
      LEAF_NOTICE("to_slave queue length %d", port_master->toSlave.length());
      to_slave_len = port_master->toSlave.length();
    }
    if (from_slave_len != port_master->fromSlave.length()) {
      LEAF_NOTICE("from_slave queue length %d", port_master->fromSlave.length());
      from_slave_len = port_master->fromSlave.length();
    }

    if (connected) {
      unsigned long now = millis();
      unsigned long last_rx = max(ackrecvd, cmdrecvd);

      if ((pingsent > ackrecvd) &&
	  (now > (pingsent + ping_timeout_sec*1000))) {
	LEAF_ALERT("PING sent at %lu was not ACKnowledged within limit of %d seconds", pingsent, ping_timeout_sec);
	message("tcp", "cmd/disconnect", "5");
	ackrecvd = now; // clear the failure condition
      }

      unsigned long last_activity = max(pingsent, last_rx);
      unsigned long inactivity_sec = (now - last_activity)/1000;
      if (inactivity_sec >= ping_interval_sec) {
	LEAF_NOTICE("Sending a keepalive/ping (have been inactive for %lu sec)", inactivity_sec);
	message("tcp", "cmd/send", "PING");
	pingsent=millis();
      }

      unsigned long command_inactivity_sec = (now - cmdrecvd)/1000;
      if (command_inactivity_sec >= command_watchdog_sec) {
	LEAF_ALERT("Command watchdog expired (no commands for %lu sec), disconnecting socket", command_inactivity_sec);
	message("tcp", "cmd/disconnect", "5"); // disconnect with near-immediate retry
	cmdrecvd = now; // clear the failure condition
      }
    } // end if(connected)

    if (port_master->fromSlave.length()) {
      // get data from modbus, write onward to TCP
      int send_len = port_master->fromSlave.length();
      LEAF_NOTICE("Enqueuing %d bytes from slave to TCP", send_len);
      DumpHex(L_NOTICE, "SEND", port_master->fromSlave.c_str(), send_len);
      message("tcp", "cmd/send", port_master->fromSlave);
      port_master->fromSlave.remove(0, send_len);
    }
  }

  void mqtt_do_subscribe() {
    LEAF_ENTER(L_DEBUG);
    Leaf::mqtt_do_subscribe();
    LEAF_LEAVE;
  }

  bool mqtt_receive(String type, String name, String topic, String payload) {
    LEAF_ENTER(L_INFO);
    bool handled=false;

    if (topic == "_comms_state") {
      // do not log
    }
    else {
      LEAF_INFO("RECV %s %s %s len=%d", type.c_str(), name.c_str(), topic.c_str(), payload.length());
    }

    WHEN("_tcp_connect", {
	LEAF_NOTICE("Modbus bridge TCP connected, our ID is [%s]", bridge_id);
	if (bridge_id.length()) {
	  message("tcp", "cmd/sendline", bridge_id);
	}
	connected=true;
      })
    WHEN("_tcp_disconnect", {
	LEAF_NOTICE("Modbus bridge TCP disconnected");
	connected = false;
      })
    ELSEWHEN("cmd/ping", {
	LEAF_NOTICE("Sending a keepalive/ping (manual)");
	message("tcp", "cmd/send", "PING");
	pingsent=millis();
    })
    ELSEWHEN("rcvd", {
	if (payload == "ACK") {
	  NOTICE("Received ACK");
	  ackrecvd = millis();
	}
	else {
	  cmdrecvd = millis();
	  port_master->toSlave+=payload;
	  LEAF_NOTICE("Received msg of %d.  Now %d bytes queued for modbus", payload.length(), port_master->toSlave.length());
	  DumpHex(L_NOTICE, "RCVD", payload.c_str(), payload.length());
	}
      });

    if (!handled) {
      handled = Leaf::mqtt_receive(type, name, topic, payload);
    }
    LEAF_RETURN(handled);
  }

};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
