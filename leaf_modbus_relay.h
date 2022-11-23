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
    , TraitDebuggable(name)
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

  void mqtt_do_subscribe() {
    LEAF_ENTER(L_DEBUG);
    Leaf::mqtt_do_subscribe();
    register_mqtt_cmd("send_hex", "send bytes (hex) to modbus", HERE);
  }

  bool mqtt_receive(String type, String name, String topic, String payload) {
    LEAF_ENTER(L_INFO);
    bool handled = false;

    LEAF_INFO("%s %s %s %s", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());

    WHEN("cmd/send_hex",{
	char buf[3];
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
      })
    else {
      handled = Leaf::mqtt_receive(type, name, topic, payload);
    }
    return handled;
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
	DumpHex(L_INFO, "RelayDown", buf, count);
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
	  DumpHex(L_INFO, "RelayUp", buf, count);
	  ++up_word_count;
	}
	else {
	  // nothing to relay in either direction, leave the loop
	  break;
	}
      }
      yield();
    }
  }

};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
