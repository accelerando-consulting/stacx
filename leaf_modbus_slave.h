//
//@************************* class ModbusSlaveLeaf **************************
//
// This class encapsulates a generic modbus slave device
//
#include <HardwareSerial.h>
#include <ModbusSlave.h>

class ModbusSlaveLeaf : public Leaf
{
  //SimpleMap <String,ModbusReadRange*> *readRanges;
  int unit=-1;
  int uart=-1;
  Stream *port = NULL;
  Modbus *bus = NULL;
  StorageLeaf *registers = NULL;
  int bus_rx_pin=-1;
  int bus_tx_pin=-1;
  int bus_nre_pin=-1;
  int bus_de_pin=-1;
  int baud = 115200;
  int options = SERIAL_8N1;

public:
  ModbusSlaveLeaf(String name,
		  String target,
		  //ModbusReadRange **readRanges,
		  int unit=1,
		  Stream *port=NULL,
		  int uart=-1,
		  int baud = 115200,
		  int options = SERIAL_8N1,
		  int bus_rx_pin=-1,
		  int bus_tx_pin=-1,
		  int bus_nre_pin=-1,
		  int bus_de_pin=-1
    )
    : Leaf("modbusSlave", name, NO_PINS, target)
    , Debuggable(name)
  {
    //this->readRanges = new SimpleMap<String,ModbusReadRange*>(_compareStringKeys);
    //for (int i=0; readRanges[i]; i++) {
    //  this->readRanges->put(readRanges[i]->name, readRanges[i]);
    //}
    this->unit = unit;
    this->uart = uart;
    this->bus_rx_pin = bus_rx_pin;
    this->bus_tx_pin = bus_tx_pin;
    this->bus_nre_pin = bus_nre_pin;
    this->bus_de_pin = bus_de_pin;
    this->port = port;
  }

  static const char *fc_name(uint8_t code)
  {
    switch (code) {
    case FC_READ_COILS:
      return "FC_READ_COILS";
    case FC_READ_DISCRETE_INPUT:
      return "FC_READ_DISCRETE_INPUT";
    case FC_READ_HOLDING_REGISTERS:
      return "FC_READ_HOLDING_REGISTERS";
    case FC_READ_INPUT_REGISTERS:
      return "FC_READ_INPUT_REGISTERS";
    case FC_READ_EXCEPTION_STATUS:
      return "FC_READ_EXCEPTION_STATUS";
    case FC_WRITE_COIL:
      return "FC_WRITE_COIL";
    case FC_WRITE_MULTIPLE_COILS:
      return "FC_WRITE_MULTIPLE_COILS";
    case FC_WRITE_REGISTER:
      return "FC_WRITE_REGISTER";
    case FC_WRITE_MULTIPLE_REGISTERS:
      return "FC_WRITE_MULTIPLE_REGISTERS";
    default:
      return "INVALID";
    }
  }

  virtual void onCallback(uint8_t code, uint16_t addr, uint16_t len)
  {
    LEAF_NOTICE("%s callback 0x%02x:0x%x,%u", getNameStr(), (unsigned int)code, (unsigned int)addr, (unsigned int)len);
  }

  int getRegister(int code, int addr)
  {
    String key = String(code)+":"+String(addr,HEX);
    int result = registers->getInt(key);
    LEAF_INFO("getRegister %s <= 0x%04x", key.c_str(), result);
    return result;
  }

  int hasRegister(int code, int addr)
  {
    String key = String(code)+":"+String(addr,HEX);
    return registers->has(key);
  }

  void putRegister(int code, int addr, int value)
  {
    String key = String(code)+":"+String(addr,HEX);
    LEAF_NOTICE("putRegister %s => 0x%04x", key.c_str(), value);
    registers->putInt(key, value);
  }

  int readBits(int code, int addr, int len)
  {
    if (!hasRegister(code, addr)) return STATUS_ILLEGAL_DATA_ADDRESS;
    for (int i=0; i<len; i++) {
      int bit = getRegister(code, addr+i);
      LEAF_INFO("readBits %02x:%04x <= %d", code, addr+i, bit);
      bus->writeCoilToBuffer(i, bit);
    }
    return STATUS_OK;
  }

  int readWords(int code, int addr, int len)
  {
    if (!hasRegister(code, addr)) return STATUS_ILLEGAL_DATA_ADDRESS;
    for (int i=0; i<len; i++) {
      int word = getRegister(code, addr+i);
      LEAF_INFO("readWords %02x:%04x <= 0x%04x", code, addr+i, word);
      bus->writeRegisterToBuffer(i, word);
    }
    return STATUS_OK;
  }

  int writeBits(int code, int addr, int len)
  {
    for (int i=0; i<len; i++) {
      int bit = bus->readCoilFromBuffer(i);
      LEAF_INFO("writeBits %02x:%04x <= %d", code, addr+i, bit);
      putRegister(FC_READ_COILS, addr+i, bit);
    }
    return STATUS_OK;
  }

  int writeWords(int code, int addr, int len)
  {
    for (int i=0; i<len; i++) {
      int word = bus->readRegisterFromBuffer(i);
      LEAF_INFO("writeWords %02x:%04x <= %d", code, addr+i, word);
      putRegister(FC_READ_HOLDING_REGISTERS, addr+i, word);
    }
    return STATUS_OK;
  }

  virtual bool valueChangeHandler(String topic, Value *v) {
    LEAF_HANDLER(L_INFO);

    WHEN("unit", {
      bus->setUnitAddress(VALUE_AS_INT(v));
    })
    else if (!handled) {
      handled = Leaf::valueChangeHandler(topic, v);
    }

    LEAF_HANDLER_END;
  }



  virtual bool commandHandler(String type, String name, String topic, String payload) {
    LEAF_HANDLER(L_INFO);

    WHEN("enable", bus->enable())
    ELSEWHEN("disable", bus->disable())
    ELSEWHEN("sendmode", digitalWrite(bus_de_pin, HIGH))
    ELSEWHEN("recvmode", digitalWrite(bus_de_pin, LOW))
    ELSEWHEN("send",{
      int pos = 0;
      while (pos < payload.length()) {
	long int bite = strtol(payload.substring(pos,pos+1).c_str(), NULL, 16);
	LEAF_NOTICE("Send char 0x%02x", (int)bite);
	port->write((uint8_t)bite);
	pos+=2;
      }
    })
    ELSEWHEN("recv",{
      String got="";
      int timeout_ms = payload.toInt();
      if (timeout_ms <= 1) timeout_ms=5000;
      unsigned long timeout = millis() + timeout_ms;
      do {
	while (port->available()) {
	  char c = port->read();
	  char hex[3];
	  snprintf(hex, sizeof(hex), "%02X", c);
	  LEAF_NOTICE("Rcvd char 0x%s", hex);
	  got += hex;
	  if (c) timeout = millis()+500;
	}
      } while (millis() < timeout);
      if (got.length()==0) got="TIMEOUT";
      mqtt_publish("recv", got);
    })
    else {
      handled = Leaf::commandHandler(type, name, topic, payload);
    }

    LEAF_HANDLER_END;
  }

  void setup(void) {
    Leaf::setup();
    LEAF_ENTER(L_NOTICE);

    if (port == NULL) {
      LEAF_NOTICE("Create hardware serial unit %d with pins rx=%d tx=%d",
		  uart, bus_rx_pin, bus_tx_pin);
      HardwareSerial *hs = new HardwareSerial(uart);;
      hs->begin(baud, options, this->bus_rx_pin, this->bus_tx_pin);
      port = hs;
    }
    if (!bus) {
      LEAF_NOTICE("Create modbus peripheral with unit number %d and DE pin %d", unit, bus_de_pin);
      bus = new Modbus(*port, unit, this->bus_de_pin);
    }

    if (bus_nre_pin >= 0) {
      LEAF_NOTICE("Set modbus ~RE pin to %d", bus_nre_pin);
      pinMode(this->bus_nre_pin, OUTPUT);
      digitalWrite(this->bus_nre_pin, LOW);
    }

    for (int i = 0; i<CB_MAX; i++) {
      bus->cbVector[i]=
	[](uint8_t code, uint16_t addr, uint16_t len)->uint8_t
	{
	  NOTICE("modbus callback 0x%02x:0x%x,%u", (unsigned int)code, (unsigned int)addr, (unsigned int)len);
	  ModbusSlaveLeaf *that= (ModbusSlaveLeaf *)Leaf::get_leaf_by_type(leaves, String("modbusSlave"));
	  if (!that) {
	    ALERT("modbus slave: I don't know who I am!");
	    return STATUS_SLAVE_DEVICE_FAILURE;
	  }
	  that->onCallback(code,addr,len);
	  switch (code) {
	  case FC_READ_COILS:
	  case FC_READ_DISCRETE_INPUT:
	    return that->readBits(code,addr,len);
	  case FC_READ_HOLDING_REGISTERS:
	  case FC_READ_INPUT_REGISTERS:
	    return that->readWords(code,addr,len);
	  case FC_WRITE_COIL:
	    return that->writeBits(code, addr, 1);
	  case FC_WRITE_REGISTER:
	    return that->writeWords(code, addr, 1);
	  case FC_WRITE_MULTIPLE_COILS:
	    return that->writeBits(code, addr, len);
	  case FC_WRITE_MULTIPLE_REGISTERS:
	    return that->writeWords(code, addr, len);
	  }
	  return STATUS_ILLEGAL_FUNCTION;
	};
    }
    registers = (StorageLeaf *)get_tap("registers");


    registerLeafCommand(HERE, "send", "Send a (hex) string to the modbus port");
    registerLeafCommand(HERE, "recv", "Receive and print (in hex) a string from the modbus port (payload=timeout_ms");
    registerLeafCommand(HERE, "enable");
    registerLeafCommand(HERE, "disable");
    registerLeafCommand(HERE, "sendmode", "Configure enable pins for sending");
    registerLeafCommand(HERE, "recvmode", "Configure enable pins for receiving");

    registerLeafIntValue("unit", &unit, "Modbus unit number");

    LEAF_LEAVE;
  }

  void start(void)
  {
    Leaf::start();
    LEAF_ENTER(L_NOTICE);
    bus->begin(baud);
    bus->setHalfDuplex(true);
    if (getDebugLevel() >= L_INFO) {
      LEAF_NOTICE("Enabling debug stream in modbus library");
      bus->setDbg(&DBG);
    }
    LEAF_LEAVE;
  }

  void loop(void) {
    char buf[160];

    Leaf::loop();
    //LEAF_ENTER(L_NOTICE);
    bus->poll();
    //LEAF_LEAVE;
  }

};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
