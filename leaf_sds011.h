//
//@************************* class ModbusMasterLeaf **************************
//
// This class encapsulates a serial-port attached SDS011 dust sensor 
//

#include <HardwareSerial.h>

class SDS011Leaf : public Leaf
{
protected:
  int uart;
  int baud;
  int rxpin;
  int txpin=-1;
  HardwareSerial *port;
  uint32_t config;

  int delta = 5;
  int pm10;
  int pm2d5;


  uint8_t buf[10];
  int buflen = 0;
  int timeout = 100;
  unsigned long last_read = 0;
  
public:
  SDS011Leaf(String name, int uart=2, int baud=9600,uint32_t config=SERIAL_8N1,int rxpin=-1,int txpin=-1) :
    Leaf("sds011", name, NO_PINS) {
    LEAF_ENTER(L_INFO);

    this->uart = uart;
    this->baud = baud;
    this->rxpin = rxpin;
    this->txpin = txpin;
    this->port = new HardwareSerial(uart);

    LEAF_LEAVE;
  }

  void setup(void) {
    Leaf::setup();
    LEAF_ENTER(L_NOTICE);
    port->begin(baud, config, rxpin, txpin);
    LEAF_LEAVE;
  }

  void status_pub() 
  {
    publish("status/pm10", String(pm10));
    publish("status/pm2d5", String(pm2d5));
  }

  void loop(void) {

    Leaf::loop();
    //LEAF_ENTER(L_NOTICE);
    unsigned long now = millis();

    // The UART communication protocol
    //  RX of UART(TTL)@3.3V
    //  TX of UART(TTL)@3.3V
    // Bit rate :9600
    // Data bit :8
    // Parity bit:NO
    // Stop bit :1
    // Data Packet frequency: 1Hz
    // Packet is 10 bytes: 0xAA 0xC0 DATA1(PM25LO) DATA2(PM25HI) DATA3(PM10LO) DATA4(PM10HI) DATA5(ID1) DATA6(ID2) SUM
    // Check-sum: Check-sum=DATA1+DATA2+...+DATA6

    if (!port->available()) {
      return;
    }
      
    if (buflen && (now >= last_read+timeout)) {
      // Flush buffer
      LEAF_NOTICE("Flush buffer");
      buflen = 0;
    }

    int a,n;
    while (a=port->available()) {
      if ((buflen+a) > sizeof(buf)) {
	LEAF_ALERT("The %d available characters would overflow buffer, which has %d", a, buflen);
	a = sizeof(buf)-buflen;
      }
      n = port->read(buf+buflen, a);
      last_read = millis();
      //LEAF_NOTICE("Read %d/%d bytes", n, a);
      buflen+=n;
      //DumpHex(L_NOTICE, "Buffer content", buf, buflen);
    }
    
    if (buflen < 10) {
      return;
    }

    /*
    if ((buf[0] != 0xAA) || (buf[1] != 0xC0)) {
      DumpHex(L_ALERT, "Scrambled buffer", buf, buflen);
      return;
    }
    */

    int new_pm2d5 = buf[2]+(buf[3]<<8);
    int new_pm10 = buf[4]+(buf[5]<<8);
    bool changed = false;

    if (abs(new_pm2d5-pm2d5)>delta) {
      changed=true;
      pm2d5 = new_pm2d5;
    }
    if (abs(new_pm10-pm10)>delta) {
      changed=true;
      pm10 = new_pm10;
    }
    
    if (changed) {
      status_pub();
    }

    buflen=0; // consumed
     
    //LEAF_LEAVE;
  }

};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
