#pragma once
//
/*@************************** class MAX3140Leaf *****************************/
//
// This class encapsulates an RS485 device that uses the MAX3140 SPI-uart-driver
// chip (which is equvalent to a MAX3100 UART hooked to a MAX3089 full duplex RS422
// driver).
//
// It is difficult to use hardware SPI to talk to this device because of its BONKERS
// bi-directional transactions.
// 
#include "leaf.h" // unnecessary but helps static code checker

class MAX3140Leaf : public Leaf
{
  //
  // Configuration
  //
  int sck = SCK;
  int miso = MISO;
  int mosi = MOSI;
  int ss = SS;
  int irq = -1;
  int freq = 100000; // 100khz = 10us per bit
  int send_timeout_ms = 500;

  bool tx_enable = true;
  bool rx_enable = false;
  
  bool use_fifo = true;
  bool shdn_mode = false;
  bool txint_enable = false;
  bool rxint_enable = false;
  bool print_enable = false;
  bool rfint_enable = false;
  bool irda_mode = false;
  bool use_2stop = false;
  bool parity_enable = false;
  bool use_7bits = false;
  int baud = 9600;

  //
  // Derived configuration
  //
  int byte_time_us = 1000;
  int half_bit_time_us = 500000/freq;

  //
  // Ephemeral state
  //
  uint8_t rx_buffer[64];
  int rx_len = 0;

  bool tx_empty = false;
  bool rx_avail = false;
  
  unsigned long config_write_count = 0;
  unsigned long config_read_count = 0;
  unsigned long send_count = 0;
  unsigned long recv_count = 0;
  unsigned long bytes_sent = 0;
  unsigned long bytes_rcvd = 0;

  unsigned long last_rx_poll = 0;
  unsigned long poll_interval_us = 10000;

  uint16_t transact(uint16_t tx) 
  {
    uint16_t rx = 0;

    // This SPI bit bang is slower than it needs to be, but chosen to be
    // readable on page and on logic analzyer
    
    digitalWrite(sck, LOW);
    digitalWrite(ss, LOW);
    delayMicroseconds(2*half_bit_time_us);
    
    for (int bit=15; bit>=0; --bit) {

      digitalWrite(mosi, !!(tx & 0x8000));
      tx <<= 1;

      delayMicroseconds(half_bit_time_us);
      digitalWrite(sck, HIGH);

      rx <<= 1;
      rx |= digitalRead(miso)?1:0;

      delayMicroseconds(half_bit_time_us);
      digitalWrite(sck, LOW);
    }
    delayMicroseconds(2*half_bit_time_us);
    digitalWrite(ss, HIGH);
    delayMicroseconds(4*half_bit_time_us);

    // Bit 15 of an input word is always RX fifo status
    rx_avail = (rx & 0x8000);

    // Bit 14 of an input word is always TX fifo status
    tx_empty = (rx & 0x4000);
    
    return rx;
  }
  
  uint16_t read_config() 
  {
    ++config_read_count;
    uint16_t result = transact(0x4000);
    LEAF_NOTICE("read_config returns 0x%04x", (int)result);
    return result;
  }

  uint16_t write_config(uint16_t config) 
  {
    ++config_write_count;
    return transact(config | 0xC000);
  }

  uint16_t write_data(uint8_t d, bool te=true, bool rts=LOW, bool pt=false) 
  {
    if (te) ++bytes_sent;
    uint16_t word = (0x8000 | (te?0x400:0) | (rts?0x200:0) | (pt?0x100:0) | d);
    LEAF_NOTICE("write_data d=0x%02x (%c), te=%s rts=%s pt=%s => 0x%04x",
		(int)d, ((d>=' ')&&(d<='~'))?(int)d:'x', TRUTH_lcj(te), HEIGHT_j(rts), TRUTH_lcj(pt), word);
    return transact(word);
  }
  
  bool read_data(uint8_t *d=NULL, bool *txempty=NULL, bool *rafe=NULL, bool *cts=NULL, bool *pr=NULL) 
  {
    uint16_t rx = transact(0);
    if (!(rx & 0x8000)) {
      return false;
    }
    ++bytes_rcvd;
    if (d) *d = (rx&0xFF);
    if (txempty) *txempty = (rx&0x4000);
    if (rafe) *rafe = (rx&0x400);
    if (cts) *cts = (rx&0x200);
    if (pr) *pr = (rx & 0x100);
    return true;
  }

  bool send_config() 
  {
    LEAF_ENTER(L_NOTICE);
    
    uint16_t config = 0;

    config |= use_fifo?0:0x2000;
    config |= shdn_mode?0x1000:0;
    config |= txint_enable?0x800:0;
    config |= rxint_enable?0x400:0;
    config |= print_enable?0x200:0;
    config |= rfint_enable?0x100:0;
    config |= irda_mode?0x80:0;
    config |= use_2stop?0x40:0;
    config |= parity_enable?0x20:0;
    config |= use_7bits?0x10:0;

    switch (baud) {
    case 115200:
      // config |= 0;
      break;
    case 57600:
      config |= 0x1;
      break;
    case 28800:
      config |= 0x2;
      break;
    case 14400:
      config |= 0x3;
      break;
    case 7200:
      config |= 0x4;
      break;
    case 3600:
      config |= 0x5;
      break;
    case 1800:
      config |= 0x6;
      break;
    case 900:
      config |= 0x7;
      break;
    case 38400:
      config |= 0x8;
      break;
    case 19200:
      config |= 0x9;
      break;
    case 9600:
      config |= 0xA;
      break;
    case 4800:
      config = 0xB;
      break;
    case 2400:
      config = 0xC;
      break;
    case 1200:
      config = 0xD;
      break;
    case 600:
      config = 0xE;
      break;
    case 300:
      config = 0xF;
      break;
    }

    byte_time_us = 1000000 / (baud/10); // one byte time presuming ten bits per byte

    // this is time for SPI transmission, not uart
    half_bit_time_us = 500000/freq;

    LEAF_NOTICE("send_config 0x%04X baud=%d => byte_time_us=%d half_bit_time_us=%d", (int)config, baud, byte_time_us, half_bit_time_us);
    write_config(config);

    uint16_t readback = read_config() & 0x3FFF;
    
    LEAF_BOOL_RETURN(config==readback);
  }
  
public:
  MAX3140Leaf(String name,
	      String target,
	      int irq = -1,
	      int ss = -1,
	      int sck = -1,
	      int miso = -1,
	      int mosi = -1)
    : Leaf("max3140", name, NO_PINS, target)
    , Debuggable(name, L_NOTICE)
  {
    if (irq >= 0) this->irq = irq;
    if (ss >= 0) this->ss = ss;
    if (sck >= 0) this->sck = sck;
    if (miso >= 0) this->miso = miso;
    if (mosi >= 0) this->mosi = mosi;
  }

  void setup(void) {
    Leaf::setup();
    LEAF_ENTER(L_NOTICE);

    registerCommand(HERE, "send");

    registerLeafIntValue("sck_pin", &sck);
    registerLeafIntValue("miso_pin", &miso);
    registerLeafIntValue("mosi_pin", &mosi);
    registerLeafIntValue("ss_pin", &ss);
    registerLeafIntValue("irq_pin", &irq);
    registerLeafIntValue("bus_freq", &freq);
    registerLeafBoolValue("tx_enable", &tx_enable);
    registerLeafBoolValue("rx_enable", &rx_enable);
    registerLeafBoolValue("use_fifo", &use_fifo);
    registerLeafBoolValue("shdn_mode", &shdn_mode);
    registerLeafBoolValue("txint_enable", &txint_enable);
    registerLeafBoolValue("rxint_enable", &rxint_enable);
    registerLeafBoolValue("print_enable", &print_enable);
    registerLeafBoolValue("rfint_enable", &rfint_enable);
    registerLeafBoolValue("irda_mode", &irda_mode);
    registerLeafBoolValue("use_2stop", &use_2stop);
    registerLeafBoolValue("parity_enable", &parity_enable);
    registerLeafBoolValue("use_7bits", &use_7bits);
    registerLeafIntValue("baud", &baud);
    
    LEAF_NOTICE("MAX3140 with pins SCK/MISO/MOSI/SS=%d/%d/%d/%d IRQ=%d",
		sck, miso, mosi, ss, irq);
    digitalWrite(sck, LOW);
    pinMode(sck, OUTPUT);
    pinMode(miso, INPUT);
    digitalWrite(mosi, LOW);
    pinMode(mosi, OUTPUT);
    digitalWrite(ss, HIGH);
    pinMode(ss, OUTPUT);
    if (irq > 0) {
      pinMode(irq, INPUT_PULLUP);
    }
    
    LEAF_LEAVE;
  }

  void start(void) 
  {
    Leaf::start();
    LEAF_ENTER(L_NOTICE);
    send_config();
    LEAF_LEAVE;
  }

  void status_pub() 
  {
    Leaf::status_pub();
    LEAF_ENTER(L_NOTICE);
    mqtt_publish("config_write_count", String(config_write_count));
    mqtt_publish("config_read_count" , String(config_read_count));
    mqtt_publish("send_count" , String(send_count));
    mqtt_publish("recv_count" , String(recv_count));
    mqtt_publish("bytes_sent" , String(bytes_sent));
    mqtt_publish("bytes_rcvd" , String(bytes_rcvd));
    LEAF_LEAVE;
  }

  bool canTransmit() 
  {
    if (tx_empty) return true; // we already know the TX fifo is empty
    
    // write a dummy byte with tx disabled
    // This has the side effect of receiving any queued data, and setting tx_empty/rx_avail
    uint16_t rx = write_data(0, false);  

    if (rx & 0x8000) {
      uint8_t d = (rx&0xFF);
      LEAF_NOTICE("received a byte as a side-effect of status-check: 0x%02x (%c)", d,((d>=' ')&&(d<='~'))?(int)d:'x');
      rx_buffer[rx_len++] = rx&0xFF;
      if (rx_len == sizeof(rx_buffer)) {
	// buffer overflow
	LEAF_WARN("RX buffer overflow");
	publish("recv", String(rx_buffer, sizeof(rx_buffer)), L_NOTICE, HERE);
	rx_len = 0;
      }
    }

    return (tx_empty);
  }
  
  virtual bool commandHandler(String type, String name, String topic, String payload) {
    LEAF_HANDLER(L_INFO);

    WHENAND("send", tx_enable, {
      int len = payload.length();
      LEAF_NOTIDUMP_AT(HERE, "SPIUART SEND", payload.c_str(), len);
      unsigned long start = millis();
      bool ok2go=false;
      for (int i=0; i<len; i++) {
	while (!(ok2go=canTransmit()) && (millis() <= (start+send_timeout_ms))) {
	  delay(1);
	}
	if (!ok2go) {
	  LEAF_WARN("Send timeout");
	  break;
	}
	uint16_t status = write_data(payload[i]);
      }
      // may have data in rx_buffer here, but wait for loop
    })
    if (!handled) {
      handled = Leaf::commandHandler(type, name, topic, payload);
    }

    LEAF_HANDLER_END;
  }

  void loop(void) {
    Leaf::loop();
    //LEAF_ENTER(L_NOTICE);

    if (!rx_enable) return;
    
    if (micros() > (last_rx_poll + poll_interval_us)) {
      uint8_t d;
      while (read_data(&d)) {
	rx_buffer[rx_len++] = (char)d;
	if (rx_len >= sizeof(rx_buffer)) {
	  // buffer overflow
	  publish("recv", String(rx_buffer, rx_len), L_NOTICE, HERE);
	  rx_len=0;
	}
      }
      if (rx_len) {
	publish("recv", String(rx_buffer, rx_len), L_NOTICE, HERE);
	rx_len = 0;
      }
      last_rx_poll = micros();
    }
  }
};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
