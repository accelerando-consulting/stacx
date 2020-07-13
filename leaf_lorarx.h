//
//@**************************** class LoRaRxLeaf ******************************
// 
// This class encapsulates a LoRa receiver gateway
// 
#include <LoRa.h>

class LoRaRxLeaf : public Leaf
{
  unsigned long freq;
  unsigned long rxc;
  int ss,rst,di0;
  
public:

  LoRaRxLeaf(String name, pinmask_t pins,
	     unsigned long freq=915E6, int ss=18, int rst=14, int di0=26)
    : Leaf("rcrx", name, pins) {
    LEAF_ENTER(L_INFO);
    this->freq = freq;
    this->ss = ss;
    this->rst = rst;
    this->di0 = di0;
    
    
    LEAF_LEAVE;
  }

  void setup(void) {
    Leaf::setup();
    LEAF_ENTER(L_NOTICE);
    SPI.begin(5, 19, 27, 18);
    LoRa.setPins(ss, rst, di0);

    if (!LoRa.begin(freq)) {
      ALERT("Starting LoRa failed!");
    }
  }

  void loop(void) {
    char buf[160];

    Leaf::loop();

    int packetSize = LoRa.parsePacket();
    
    if (packetSize) {
      int rssi = LoRa.packetRssi();

      int p = 0;
      while (LoRa.available()) {
	char c = LoRa.read();
	buf[(p++)%sizeof(buf)] = c;
      }
      buf[p%sizeof(buf)] = '\0';

      NOTICE("LoRa packet recieved size=%d rssi=%d", packetSize, rssi);
      mqtt_publish("event/receive", buf);

    }
      
    //LEAF_LEAVE;
  }

};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
