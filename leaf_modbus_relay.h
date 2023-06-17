//
//@************************* class ModbusRelayLeaf **************************
//
// This class encapsulates a modbus device that operates in master mode
// but relays already packed commands received from another leaf (such as a modbus
// bridge)
//
#include <HardwareSerial.h>
//#include <ModbusSlave.h>

enum _busdir {READING,WRITING};

#ifndef MODBUS_RELAY_TEST_PAYLOAD
#define MODBUS_RELAY_TEST_PAYLOAD "0104310D0001AEF5"
#endif

class ModbusRelayLeaf : public Leaf
{
  Stream *relay_port;
  HardwareSerial *bus_port;
  int bus_rx_pin;
  int bus_tx_pin;
  int bus_nre_pin;
  int bus_de_pin;
  int baud = 115200;
  int options = SERIAL_8N1;
  enum _busdir bus_direction = READING;
  String test_payload = MODBUS_RELAY_TEST_PAYLOAD;

  unsigned long up_word_count = 0;
  unsigned long up_byte_count = 0;
  unsigned long down_word_count = 0;
  unsigned long down_byte_count = 0;
    
  //Modbus *bus = NULL;
  String target;
  //StorageLeaf *registers = NULL;

public:
  ModbusRelayLeaf(String name,
		  String target,
		  Stream *relay_port,
		  int bus_uart=1,
		  int baud = 115200,
		  int options = SERIAL_8N1,
		  int bus_rx_pin=-1,
		  int bus_tx_pin=-1,
		  int bus_nre_pin=-1,
		  int bus_de_pin=-1)
    : Leaf("modbusRelay", name, NO_PINS)
    , Debuggable(name)
  {
    LEAF_ENTER(L_INFO);
    this->target=target;
    this->relay_port = relay_port;
    this->bus_port = new HardwareSerial(bus_uart);
    this->baud = baud;
    this->options = options;
    this->bus_rx_pin = bus_rx_pin;
    this->bus_tx_pin = bus_tx_pin;
    this->bus_nre_pin = bus_nre_pin;
    this->bus_de_pin = bus_de_pin;

    LEAF_LEAVE;
  }

  void setup(void) {
    Leaf::setup();
    LEAF_ENTER(L_INFO);
    this->install_taps(target);
    //registers = (StorageLeaf *)get_tap("registers");
    bus_port->begin(baud, options, bus_rx_pin, bus_tx_pin);

    if (bus_nre_pin >= 0) {
      pinMode(bus_nre_pin, OUTPUT);
      digitalWrite(bus_nre_pin, LOW); // ~RE is inverted, active low
    }
    if (bus_de_pin >= 0) {
      pinMode(bus_de_pin, OUTPUT);
      digitalWrite(bus_de_pin, LOW); 
    }

    registerCommand(HERE,"modbus_relay_send", "send bytes (hex) to modbus");
    registerCommand(HERE,"modbus_relay_test", "send a test payload to modbus");
    registerLeafStrValue("test_payload", &test_payload, "test bytes (hex) to modbus");
    
    LEAF_LEAVE;
  }

  void status_pub() 
  {
    mqtt_publish("up_word_count", String(up_word_count));
    mqtt_publish("down_word_count", String(down_word_count));
    mqtt_publish("up_byte_count", String(up_byte_count));
    mqtt_publish("down_byte_count", String(down_byte_count));
  }

  void set_direction(enum _busdir dir) 
  {
    if (dir==READING) {
      if (bus_direction == READING) {
	// nothing to do
	return;
      }
      if (bus_nre_pin>=0) {
	digitalWrite(bus_nre_pin, LOW);
      }
      if (bus_de_pin>=0) {
	digitalWrite(bus_de_pin, LOW);
      }
      bus_direction = dir;
    }
    else if (dir==WRITING) {
      if (bus_direction == WRITING) {
	// nothing to do
	return;
      }
      if (bus_nre_pin>=0) {
	digitalWrite(bus_nre_pin, HIGH);
      }
      if (bus_de_pin>=0) {
	digitalWrite(bus_de_pin, HIGH);
      }
      bus_direction = dir;
    }
    else {
      LEAF_ALERT("set_direction canthappen invalid direction %d", (int)dir);
    }
  }

  virtual String test_send(String payload) 
  {
    LEAF_ENTER_STR(L_NOTICE, payload);
    char buf[3];
    set_direction(WRITING);
    while (payload.length()>=2) {
      buf[0] = payload[0];
      buf[1] = payload[1];
      buf[2] = '\0';
      unsigned char c = strtol(buf, NULL, 16);
      LEAF_NOTICE("Transmit character 0x%02X", c);
      int wrote = bus_port->write(c);
      if (wrote != 1) {
	LEAF_ALERT("Write error %d", wrote);
      }
      payload.remove(0,2);
    }
    bus_port->flush();
    set_direction(READING);
    delay(100);

    // wait for up to 5sec for a first response, then stop listening 1sec after the most recent character
    unsigned long now = millis();
    unsigned long start = now;
    unsigned long timeout = now+5000;
    unsigned long word_timeout = 0;
    unsigned long last_input = 0;
    const int max_response = 32;
    char response[max_response*2+1]="";
    int pos = 0;
    int count = 0;
    int waits = 0;
    while ((now < timeout) && ((word_timeout==0) || (now < word_timeout))) {
      delay(1);
      ++waits;
      while (bus_port->available()) {
	char c = bus_port->read();
	LEAF_NOTICE("Read character %02x", (int)c);
	word_timeout = millis()+1000;
	pos += snprintf(response+pos,sizeof(response)-pos, "%02X", (int)c);
	++count;
	if (count >= max_response) break;
      }
      
      // TODO: once we have a length byte we can stop waiting once we have the whole response
      now = millis();
    }
    if (count == 0) {
      LEAF_WARN("Timeout: no characters seen in %d checks over %dms", waits, (int)(now-start));
      snprintf(response, sizeof(response), "TIMEOUT");
    }
    LEAF_STR_RETURN_SLOW(2000, String(response));
  }
  
  virtual bool commandHandler(String type, String name, String topic, String payload) {
    LEAF_HANDLER(L_INFO);

    WHEN("modbus_relay_test",{
	String response = test_send(test_payload);
	mqtt_publish("status/modbus_relay_test", response);
    })
    ELSEWHEN("modbus_relay_send",{
	String response = test_send(payload);
	mqtt_publish("status/modubus_relay_send", response);
    })
    else {
      handled = Leaf::commandHandler(type, name, topic, payload);
    }

    LEAF_HANDLER_END;
  }

  void loop(void) {
    uint8_t buf[32];
    
    Leaf::loop();
    //LEAF_ENTER(L_NOTICE);

    // Look for input from the bus
    while (true) {
      int count=0;
      uint8_t c;

      if (relay_port->available()) {
	set_direction(WRITING);
	delay(50);
	while (relay_port->available()) {
	  c = relay_port->read();
	  if (count < sizeof(buf)) buf[count]=c;
	  ++count;
	  int wrote = bus_port->write(c);
	  if (wrote != 1) {
	    LEAF_ALERT("Downstream relay write error %d", wrote);
	  }
	  else {
	    ++down_byte_count;
	  }
	}
	bus_port->flush();
	LEAF_INFO("Relayed %d bytes to physical bus", count);
	LEAF_INFODUMP("RelayDown", buf, count);
	++down_word_count;
      }
      else {
	set_direction(READING);
	if (bus_port->available()) {
	  while (bus_port->available()) {
	    c = bus_port->read();
	    if (count < sizeof(buf)) buf[count]=c;
	    ++count;
	    int wrote = relay_port->write(c);
	    if (wrote != 1) {
	      LEAF_ALERT("Upstream relay write error %d", wrote);
	    }
	    else {
	      ++up_byte_count;
	    }
	  }
	  relay_port->flush();
	  LEAF_INFO("Relayed %d bytes from physical bus", count);
	  LEAF_INFODUMP("RelayUp", buf, count);
	  ++up_word_count;
	}
	else {
	  // nothing to relay in either direction, leave the loop
	  break;
	}
      }
#ifndef ESP8266
      yield();
#endif
    }
  }

};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
