//
//@**************************** class MotionLeaf ******************************
// 
// This class encapsulates a 433MHZ ASK Radio-control transmitter
// 
#include <RFM69.h>

class Rfm69Leaf : public Leaf
{
  RFM69 *radio=NULL;
  int sck_pin = SCK;
  int miso_pin = MISO;
  int mosi_pin = MOSI;
  int nss_pin = SS;
  int irq_pin = NOT_AN_INTERRUPT;
  int rst_pin = -1;

  int network_id=0;
  int gateway_id=1;
  int node_id=2;

  int frequency_mhz = 915;
  int frequency=RF69_915MHZ;
  bool is_rfm69hw=true;
  int power_level = -1; // -1 means default
  bool spymode=true;
  String encrypt_key = "";
  int send_retries = 2;
  int ack_timeout_ms = 250;

  
public:

  Rfm69Leaf(String name, String targets, pinmask_t pins, int irq_pin=NOT_AN_INTERRUPT, int rst_pin=-1, int node_id=-1, int gateway_id=-1, int network_id=-1)
    : Leaf("rfm69", name, pins)
    , Debuggable(name)
  {
    this->tap_targets = targets;
    if (irq_pin != NOT_AN_INTERRUPT) {
      this->irq_pin = irq_pin;
    }
    if (rst_pin >= 0) {
      this->rst_pin = rst_pin;
    }
    if (node_id >= 0) this->node_id = node_id;
    if (gateway_id >= 0) this->gateway_id = gateway_id;
    if (network_id >= 0) this->network_id = network_id;
  }

  void setup(void) {
    Leaf::setup();
    LEAF_ENTER_PRETTY(L_NOTICE);
    FOR_PINS(nss_pin=pin);

    registerLeafIntValue("sck_pin", &sck_pin, "RFM69 SPI clock pin");
    registerLeafIntValue("mosi_pin", &mosi_pin, "RFM69 SPI MOSI pin");
    registerLeafIntValue("miso_pin", &miso_pin, "RFM69 SPI MISO pin");
    registerLeafIntValue("nss_pin", &nss_pin, "RFM69 chip select pin");
    registerLeafIntValue("irq_pin", &irq_pin, "RFM69 interrupt pin");
    registerLeafIntValue("rst_pin", &rst_pin, "RFM69 reset pin");

    registerLeafIntValue("network_id", &network_id, "RFM69 network id");
    registerLeafIntValue("gateway_id", &gateway_id, "RFM69 gateway id");
    registerLeafIntValue("node_id", &node_id, "RFM69 node id");

    registerLeafIntValue("frequency", &frequency_mhz, "RFM69 frequency (433,868,915)");
    registerLeafIntValue("highpower", &is_rfm69hw, "RFM69 high-power variant");
    registerLeafIntValue("power_level", &power_level, "RFM69 fixed power level");
    registerLeafBoolValue("spymode", &spymode, "RFM69 spy mode");
    registerLeafStrValue("encrypt_key", &encrypt_key, "RFM69 encryption key");
    registerLeafIntValue("send_retries", &send_retries, "Number of retries");
    registerLeafIntValue("ack_timeout", &ack_timeout_ms, "Timeout (ms) before retry");

    registerLeafCommand(HERE, "send/", "send data to a nominated node");
    registerLeafCommand(HERE, "send", "send data to the default gateway");
    registerLeafCommand(HERE, "regs", "dump all registers");

    if (rst_pin >= 0) {
      LEAF_NOTICE("Radio reset (strobe pin %d, 10ms)", rst_pin);
      pinMode(rst_pin, OUTPUT);
      digitalWrite(rst_pin, HIGH);
      delay(10);
      digitalWrite(rst_pin, LOW);
    }

    LEAF_NOTICE("Radio SPI Enable: SCK=%d MOSI=%d MISO=%d NSS=%d",
		sck_pin, mosi_pin, miso_pin, nss_pin);
    SPI.begin(D5, D6, D7, D8);
    
    LEAF_NOTICE("Radio create: NSS=%d IRQ=%d HIGHPOWER %s", nss_pin, irq_pin, ABILITY(is_rfm69hw));
    radio = new RFM69(nss_pin, irq_pin, is_rfm69hw);
    LEAF_LEAVE;
  }

  void start() 
  {
    Leaf::start();
    LEAF_ENTER_PRETTY(L_NOTICE);

    LEAF_NOTICE("Radio init freq=%dMHz node=%d network=%d", frequency_mhz, node_id, network_id);
    if (!radio->initialize(frequency, node_id, network_id)) {
      LEAF_ALERT("RFM69 Radio not ready");
      stop();
      LEAF_VOID_RETURN;
    }
    
    if (is_rfm69hw) {
      LEAF_NOTICE("Radio enable high power mode");
      radio->setHighPower();
    }
    if (encrypt_key.length()>0) {
      LEAF_NOTICE("Radio enable encryption");
      radio->encrypt(encrypt_key.c_str());
    }
    else {
      LEAF_NOTICE("Radio disable encryption");
    }
      
    if (spymode) {
      LEAF_NOTICE("Radio enable spy mode");
      radio->spyMode(true);
    }
    if (power_level >= 0) {
      LEAF_NOTICE("Radio power level %d", power_level);
      radio->setPowerLevel(power_level);
    }
    LEAF_LEAVE;
  }

  virtual bool valueChangeHandler(String topic, Value *v) {
    LEAF_HANDLER(L_INFO);

    WHEN("frequency", {
      if (VALUE_AS_INT(v) == 315) {
	frequency = RF69_315MHZ;
      }
      else if (VALUE_AS_INT(v) == 433) {
	frequency = RF69_433MHZ;
      }
      else if (VALUE_AS_INT(v) == 868) {
	frequency = RF69_868MHZ;
      }
      else if (VALUE_AS_INT(v) == 915) {
	frequency = RF69_915MHZ;
      }
    })
    else {
      handled = Leaf::valueChangeHandler(topic, v);
    }
    
    LEAF_HANDLER_END;
  }
  
  virtual bool commandHandler(String type, String name, String topic, String payload) {
    LEAF_HANDLER(L_INFO);
    LEAF_NOTICE("%s/%s sent %s <= %s", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());

    WHENPREFIX("send/", {
      int dest = topic.toInt();
      int len = payload.length();
      LEAF_NOTICE("Transmit %d bytes to node %d: [%s]", len, dest, payload.c_str());
      if (dest == 0) {
	// can't ack a broadcast
	radio->send(dest, payload.c_str(), len, false);
      }
      else {
	// unicast uses retry-unless-ack
	if (!radio->sendWithRetry(dest, payload.c_str(), len, send_retries, ack_timeout_ms)) {
	  LEAF_ALERT("Transmit ack timeout");
	}
      }
    })
    ELSEWHEN("send", {
      int len = payload.length();
      LEAF_NOTICE("Transmit %d bytes to gateway (node %d)", len, gateway_id);
      LEAF_NOTIDUMP("  send", payload.c_str(), len);

      unsigned long then=millis();
      if (radio->sendWithRetry(gateway_id, payload.c_str(), len, send_retries, ack_timeout_ms)) {
	unsigned long elapsed = millis() - then;
	LEAF_NOTICE("  acked %lums", elapsed);
      }
      else {
	LEAF_ALERT("Transmit ack timeout");
      }
	
    })
    ELSEWHEN("regs", radio->readAllRegs())
    else {
      handled = Leaf::commandHandler(type, name, topic, payload);
    }
      
    LEAF_HANDLER_END;
  }

  void status_pub() 
  {
  }
  

  void loop(void) {
    static unsigned long last_tx = 0;
    unsigned long now = millis();

    Leaf::loop();

    if (radio->receiveDone()) {
      int len = radio->DATALEN;
      int from = radio->SENDERID;
      int rssi = radio->RSSI;
      bool ack = radio->ACKRequested();
      LEAF_NOTICE("Received data from node %d, len=%d RSSI=%d%s",
		  from, len, rssi, ack?" (ACK reqd)":"");
      char rx_buf[257];
      if (len >= sizeof(rx_buf)) {
	LEAF_WARN("Truncated received message");
	len = sizeof(rx_buf)-1;
      }
      for (int i=0; i<len; i++) {
	rx_buf[i] = radio->DATA[i];
      }
      rx_buf[len]=0;
      LEAF_NOTIDUMP("  rcvd", rx_buf, len);
      if (ack) {
	radio->sendACK();
	LEAF_NOTICE("  ack sent");
      }
      mqtt_publish(String("event/recv/")+from, String(rx_buf, len));
      mqtt_publish(String("event/rssi/")+from, String(rssi));
    }
    
    //LEAF_LEAVE;
  }

};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
