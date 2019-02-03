//
//@**************************** class MotionPod ******************************
// 
// This class encapsulates a 433MHZ ASK Radio-control transmitter
// 
#include <RCSwitch.h>

class RcTxPod : public Pod
{
  RCSwitch transmitter;
  int tx_interval;
  String code;
  int sending;
  
public:

  RcTxPod(String name, pinmask_t pins) : Pod("rctx", name, pins) {
    ENTER(L_INFO);
    transmitter = RCSwitch();
    tx_interval = -1;
    sending = -1;
    code = "-";
    LEAVE;
  }

  void setup(void) {
    Pod::setup();
    ENTER(L_NOTICE);
    FOR_PINS({transmitter.enableTransmit(pin);});
  }

  void mqtt_subscribe() {
    ENTER(L_NOTICE);
    Pod::mqtt_subscribe();
    _mqtt_subscribe(base_topic+"/set/interval");
    _mqtt_subscribe(base_topic+"/set/code");
    _mqtt_subscribe(base_topic+"/set/sending");
    _mqtt_subscribe(base_topic+"/cmd/send");
    _mqtt_subscribe(base_topic+"/cmd/start");
    _mqtt_subscribe(base_topic+"/cmd/stop");
    LEAVE;
  }

  void status_pub() 
  {
    if (sending >= 0) mqtt_publish("status/sending", truth(sending), true);
    if (tx_interval >= 0) mqtt_publish("status/interval", String(tx_interval, DEC), true);
    if (!code.equals("-")) mqtt_publish("status/code", code, true);
  }
  
  bool mqtt_receive(String type, String name, String topic, String payload) {
    ENTER(L_INFO);
    bool handled = Pod::mqtt_receive(type, name, topic, payload);
    bool new_sending = false;
    if (payload == "on") new_sending=true;
    else if (payload == "true") new_sending=true;
    else if (payload == "new_sending") new_sending=true;
    else if (payload == "high") new_sending=true;
    else if (payload == "1") new_sending=true;

    WHEN("set/interval",{
      INFO("Updating interval via set operation");
      tx_interval = payload.toInt();
      status_pub();
    })
    ELSEWHEN("set/code",{
      INFO("Updating code via set operation");
      code = payload;
      status_pub();
    })
    ELSEWHEN("set/sending",{
      INFO("Updating sending status via set operation");
      sending = new_sending;
      status_pub();
    })
    ELSEWHEN("cmd/send",{
      INFO("Immediate send of code [%s]", payload.c_str());
      transmitter.send(payload.c_str());
    })
    ELSEWHEN("status/sending",{
      // This is normally irrelevant, except at startup where we
      // recover any previously retained status of the light.
      if (sending != new_sending) {
	INFO("Restoring previously retained send status");
	sending = new_sending;
      }
    })
    ELSEWHEN("status/intrval",{
      // This is normally irrelevant, except at startup where we
      // recover any previously retained status of the light.
      int value = payload.toInt();
      if (value != tx_interval) {
        INFO("Restoring previously retained send interval (%dms)", value);
        tx_interval = value;
      }
    })
    ELSEWHEN("status/code",{
      // This is normally irrelevant, except at startup where we
      // recover any previously retained status of the light.
      if (!code.equals(payload)) {
	INFO("Restoring previously retained flash code (%s)", payload.c_str());
	code = payload;
      }
    })

    LEAVE;
    return handled;
  };

      

  void loop(void) {
    static unsigned long last_tx = 0;
    unsigned long now = millis();

    Pod::loop();

    if( sending && (now >= (last_tx + tx_interval)) ) {
      DEBUG("RC TX: %s", code.c_str());
      transmitter.send(code.c_str());
      last_tx = now;
    }
    //LEAVE;
  }

};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
