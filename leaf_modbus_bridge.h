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
  unsigned long pingsent=0;
  unsigned long ackrecvd=0;

public:
  ModbusBridgeLeaf(String name,
		   String target,
		   PseudoStream *port_master
    ): Leaf("modbusBridge", name, NO_PINS) {
    LEAF_ENTER(L_INFO);
    this->target=target;
    this->port_master = port_master;
    this->do_heartbeat=true;
    this->heartbeat_interval_seconds = 10*60;
    LEAF_LEAVE;
  }

  void setup(void) {
    Leaf::setup();
    LEAF_ENTER(L_INFO);
    this->install_taps(target);
    bridge_id = getPref("bridge_id", bridge_id, "Identifying string to send to modbus cloud agent");
    LEAF_LEAVE;
  }

  void start(void) 
  {
    publish("status/bridge_id", bridge_id);
  }
  
  void heartbeat(unsigned long uptime) 
  {
    LEAF_ENTER(L_DEBUG);
    message("tcp", "send", "PING");
    pingsent=uptime;
    LEAF_LEAVE;
  }

  void loop(void) {
    Leaf::loop();
    static int to_slave_len = 0;
    static int from_slave_len = 0;

    if (to_slave_len != port_master->toSlave.length()) {
      LEAF_INFO("to_slave queue length %d", port_master->toSlave.length());
      to_slave_len = port_master->toSlave.length();
    }
    if (from_slave_len != port_master->fromSlave.length()) {
      LEAF_INFO("from_slave queue length %d", port_master->fromSlave.length());
      from_slave_len = port_master->fromSlave.length();
    }

    unsigned long now = millis();
    
    if ((pingsent > ackrecvd) &&
	(now > (pingsent + 10*10000))) {
      ALERT("PING was not ACKnowledged");
      message("tcp", "disconnect", "5");
      ackrecvd = now; // clear the failure condition
    }

    //LEAF_ENTER(L_NOTICE);
    //uint32_t now = millis();
    //LEAF_LEAVE;
    if (port_master->fromSlave.length()) {
      // get data from modbus, write onward to TCP
      int send_len = port_master->fromSlave.length();
      LEAF_DEBUG("Enqueuing %d bytes from slave to TCP", send_len);
      DumpHex(L_DEBUG, "send", port_master->fromSlave.c_str(), send_len);
      message("tcp", "send", port_master->fromSlave);
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

    LEAF_INFO("%s %s %s len=%d", type.c_str(), name.c_str(), topic.c_str(), payload.length());

    WHEN("_tcp_connect", {
	LEAF_NOTICE("Modbus bridge TCP connected, our ID is [%s]", bridge_id);
	if (bridge_id.length()) {
	  message("tcp", "sendline", bridge_id);
	}
	handled=true;
      })
    ELSEWHEN("rcvd", {
	if (payload == "ACK") {
	  NOTICE("Received ACK");
	  ackrecvd = millis();
	}
	else {
	  port_master->toSlave+=payload;
	  LEAF_NOTICE("Received msg of %d.  Now %d bytes queued for modbus", payload.length(), port_master->toSlave.length());
	  DumpHex(L_INFO, "rcvd", payload.c_str(), payload.length());
	}
	handled=true;
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
