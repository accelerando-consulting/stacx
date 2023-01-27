//
//@**************************** class RcRxLeaf ******************************
// 
// This class encapsulates a 433MHZ ASK Radio-control receiver
// 
#include <RCSwitch.h>

class RcRxLeaf : public Leaf
{
  RCSwitch receiver;
  unsigned long rxc;
  
public:

  RcRxLeaf(String name, pinmask_t pins)
    : Leaf("rcrx", name, pins)
    , Debuggable(name)
  {
    LEAF_ENTER(L_INFO);
    receiver = RCSwitch();
    rxc = 0;
    
    LEAF_LEAVE;
  }

  void setup(void) {
    Leaf::setup();
    LEAF_ENTER(L_INFO);
    FOR_PINS({receiver.enableReceive(pin);});
  }

  void loop(void) {
    char buf[160];

    Leaf::loop();

    if (receiver.available()) {
      ++rxc;
      unsigned long value = receiver.getReceivedValue();
      unsigned int bitLength = receiver.getReceivedBitlength();
      unsigned int rxdelay = receiver.getReceivedDelay();
      unsigned int *raw = receiver.getReceivedRawdata();
      unsigned int protocol = receiver.getReceivedProtocol();
      unsigned long uptime = millis()/1000;

      snprintf(buf, sizeof(buf),
         "{\"proto\":%d, \"len\":%d, \"value\":\"%lx\", \"delay\": %lu, \"rxc\":%lu, \"uptime\":%lu}",
	       protocol, bitLength, value, rxdelay, rxc, uptime);
      LEAF_DEBUG("RC RX: %s", buf);
      DBGPRINTLN(buf);
      mqtt_publish("event/receive", buf);
      
      receiver.resetAvailable();

    }
      
    //LEAF_LEAVE;
  }

};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
