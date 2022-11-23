//
//@**************************** class MotionLeaf ******************************
// 
// This class encapsulates a 433MHZ ASK Radio-control transmitter
// 
#include <RCSwitch.h>

class RcTxLeaf : public Leaf
{
  RCSwitch transmitter;
  int tx_interval;
  String code;
  int sending;
  
public:

  RcTxLeaf(String name, pinmask_t pins)
    : Leaf("rctx", name, pins)
    , TraitDebuggable(name)
  {
    LEAF_ENTER(L_INFO);
    transmitter = RCSwitch();
    tx_interval = -1;
    sending = -1;
    code = "-";
    LEAF_LEAVE;
  }

  void setup(void) {
    Leaf::setup();
    LEAF_ENTER(L_NOTICE);
    FOR_PINS({transmitter.enableTransmit(pin);});
  }

  void mqtt_do_subscribe() {
    LEAF_ENTER(L_NOTICE);
    Leaf::mqtt_do_subscribe();
    mqtt_subscribe("set/interval", HERE);
    mqtt_subscribe("set/code", HERE);
    mqtt_subscribe("set/sending", HERE);
    mqtt_subscribe("cmd/send", HERE);
    mqtt_subscribe("cmd/start", HERE);
    mqtt_subscribe("cmd/stop", HERE);
    LEAF_LEAVE;
  }

  void status_pub() 
  {
    if (sending >= 0) mqtt_publish("status/sending", truth(sending), true);
    if (tx_interval >= 0) mqtt_publish("status/interval", String(tx_interval, DEC), true);
    if (!code.equals("-")) mqtt_publish("status/code", code, true);
  }
  
  bool mqtt_receive(String type, String name, String topic, String payload) {
    LEAF_ENTER(L_INFO);
    bool handled = Leaf::mqtt_receive(type, name, topic, payload);
    bool new_sending = false;
    if (payload == "on") new_sending=true;
    else if (payload == "true") new_sending=true;
    else if (payload == "new_sending") new_sending=true;
    else if (payload == "high") new_sending=true;
    else if (payload == "1") new_sending=true;

    WHEN("set/interval",{
      LEAF_INFO("Updating interval via set operation");
      tx_interval = payload.toInt();
      status_pub();
    })
    ELSEWHEN("set/code",{
      LEAF_INFO("Updating code via set operation");
      code = payload;
      status_pub();
    })
    ELSEWHEN("set/sending",{
      LEAF_INFO("Updating sending status via set operation");
      sending = new_sending;
      status_pub();
    })
    ELSEWHEN("cmd/send",{
      LEAF_INFO("Immediate send of code [%s]", payload.c_str());
      transmitter.send(payload.c_str());
    })
    ELSEWHEN("status/sending",{
      // This is normally irrelevant, except at startup where we
      // recover any previously retained status of the light.
      if (sending != new_sending) {
	LEAF_INFO("Restoring previously retained send status");
	sending = new_sending;
      }
    })
    ELSEWHEN("status/intrval",{
      // This is normally irrelevant, except at startup where we
      // recover any previously retained status of the light.
      int value = payload.toInt();
      if (value != tx_interval) {
        LEAF_INFO("Restoring previously retained send interval (%dms)", value);
        tx_interval = value;
      }
    })
    ELSEWHEN("status/code",{
      // This is normally irrelevant, except at startup where we
      // recover any previously retained status of the light.
      if (!code.equals(payload)) {
	LEAF_INFO("Restoring previously retained flash code (%s)", payload.c_str());
	code = payload;
      }
    })

    LEAF_LEAVE;
    return handled;
  };

      

  void loop(void) {
    static unsigned long last_tx = 0;
    unsigned long now = millis();

    Leaf::loop();

    if( sending && (now >= (last_tx + tx_interval)) ) {
      LEAF_DEBUG("RC TX: %s", code.c_str());
      transmitter.send(code.c_str());
      last_tx = now;
    }
    //LEAF_LEAVE;
  }

};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
