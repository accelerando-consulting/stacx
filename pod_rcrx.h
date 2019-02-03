//
//@**************************** class RcRxPod ******************************
// 
// This class encapsulates a 433MHZ ASK Radio-control receiver
// 
#include <RCSwitch.h>

class RcRxPod : public Pod
{
  RCSwitch receiver;
  unsigned long rxc;
  
public:

  RcRxPod(String name, pinmask_t pins) : Pod("rcrx", name, pins) {
    ENTER(L_INFO);
    receiver = RCSwitch();
    rxc = 0;
    
    LEAVE;
  }

  void setup(void) {
    Pod::setup();
    ENTER(L_NOTICE);
    FOR_PINS({receiver.enableReceive(pin);});
  }

  void loop(void) {
    char buf[160];

    Pod::loop();

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
      DEBUG("RC RX: %s", buf);
      Serial.println(buf);
      mqtt_publish("event/receive", buf);
      
      receiver.resetAvailable();

    }
      
    //LEAVE;
  }

};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
