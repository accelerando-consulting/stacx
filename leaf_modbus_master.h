#pragma once
#pragma STACX_LIB ModbusMaster
#include <regex.h>
//
//@************************* class ModbusMasterLeaf **************************
//
// This class encapsulates a generic modbus transciever
//

#include <HardwareSerial.h>
#include <ModbusMaster.h>

#define RANGE_MAX 64
#define RANGE_ADDRESS_DEFAULT -1

#define FC_READ_COIL      0x01
#define FC_READ_INP       0x02
#define FC_READ_HOLD_REG  0x03
#define FC_READ_INP_REG   0x04
#define FC_WRITE_COIL     0x05
#define FC_WRITE_REG      0x06
#define FC_WRITE_COILS    0x0F
#define FC_WRITE_REGS     0x10

#define MODBUS_NO_POLL   0x00000000
#define MODBUS_POLL_ONCE 0xFFFFFFFF

class ModbusReadRange : public Debuggable
{
public:
  String name;
  int unit=0;// zero means use the bus-object's unit 
  int fc;
  int address;
  int quantity;
  uint32_t poll_interval;
  uint16_t *values = NULL;
  uint32_t last_poll = 0;
  uint32_t dedupe_interval = 60*1000;
  uint32_t last_publish_time = 0;
  String last_publish_value = "";
  
  ModbusReadRange(String name, int address, int quantity=1, int fc=FC_READ_HOLD_REG, int unit = 0, uint32_t poll_interval = 5000, int dedupe_interval=60*1000)
    : Debuggable(name)
  {
    this->name = name;
    this->fc = fc;
    this->address = address;
    this->quantity = quantity;
    this->unit=unit;
    this->poll_interval = poll_interval;
    this->dedupe_interval = 60*1000;
    this->values = (uint16_t *)calloc(quantity, sizeof(uint16_t));
  }

  ModbusReadRange *setUnit(int i){this->unit=i;return this;};

  bool needsPoll(void)
  {
    LEAF_ENTER_STR(L_TRACE, getName());

    // Special cases
    if (this->poll_interval == MODBUS_NO_POLL) return false;
    if (this->poll_interval == MODBUS_POLL_ONCE) {
      if (this->last_poll==0) {
	LEAF_INFO("%s MODBUS_POLL_ONCE", name.c_str());
	return true;
      }
      return false;
    }
    
    // Periodic polling
    bool result = false;

    uint32_t now = millis();
    uint32_t next_poll = this->last_poll + this->poll_interval;
    if (next_poll <= now) {
      LEAF_DEBUG("needsPoll %s YES (interval=%lu last=%lu now=%lu", name.c_str(),
		 this->poll_interval, this->last_poll, now);
      result = true;
    }
    else {
      int until_next = next_poll - now;
      LEAF_DEBUG("Not time to poll yet (wait %d)", (int)until_next);
    }
    LEAF_LEAVE;
    return result;
  }
};


int modbus_master_pin_de = -1;
int modbus_master_de_assert = -1;
int modbus_master_pin_re = -1;
int modbus_master_re_assert = -1;


class ModbusMasterLeaf : public Leaf
{
protected:
  int unit;
  int uart;
  int baud;
  uint32_t config;
  int bus_rx_bin;
  int bus_tx_pin;
  int bus_re_pin;
  int bus_de_pin;
  bool bus_re_invert;
  bool bus_de_invert;
  Stream *bus_port;
  ModbusMaster *bus = NULL;
  SimpleMap <String,ModbusReadRange*> *readRanges;
  bool fake_writes = false;
  bool poll_enable=true;
  int response_timeout_ms = 1000;
  int retry_delay_ms = 200;
  int retry_count = 2;

  uint32_t last_read = 0;
  uint32_t read_throttle = 50;
  int last_unit = 0;

public:
  ModbusMasterLeaf(String name,
		   String target,
		   ModbusReadRange **readRanges=NULL,
		   int unit=0,
		   int uart=-1,
		   int baud=9600,
		   uint32_t config=SERIAL_8N1,
		   int rxpin=-1,
		   int txpin=-1,
		   int repin=-1,
		   int depin=-1,
		   bool re_invert=true,
		   bool de_invert=false,
		   Stream *stream=NULL)
    : Leaf("modbusMaster", name, NO_PINS, target)
    , Debuggable(name)
  {
    LEAF_ENTER(L_INFO);
    this->readRanges = new SimpleMap<String,ModbusReadRange*>(_compareStringKeys);
    if (readRanges) {
      for (int i=0; readRanges[i]; i++) {
	this->readRanges->put(readRanges[i]->name, readRanges[i]);
      }
    }
    this->unit = this->last_unit = unit;
    this->uart = uart;
    this->baud = baud;
    this->bus_rx_bin = rxpin;
    this->bus_tx_pin = txpin;
    this->bus_re_pin = repin;
    this->bus_de_pin = depin;
    this->bus_re_invert=re_invert;
    this->bus_de_invert=de_invert;
    this->config = config;
    if (stream) {
      this->bus_port = stream;
    }
    else {
      this->bus_port = (Stream *)new HardwareSerial(uart);
    }
    this->bus = new ModbusMaster();

    LEAF_LEAVE;
  }

  static const char *exceptionName(uint8_t code) 
  {
    switch (code) {
    case 0:
      return "Success";
    case 1:
      return "Illegal Function";
    case 2:
      return "Illegal Data Address";
      break;
    case 3:
      return "Illegal Data VAlue";
      break;
    case 4:
      return "Slave Device Failure";
      break;
    case 0xE0:
      return "Invalid Slave ID";
      break;
    case 0xE1:
      return "Invalid Function";
      break;
    case 0xE2:
      return "Timeout";
      break;
    case 0xE3:
      return "Invalid CRC";
      break;
    default:
      break;
    }
    return "Unknown";
  }

  void setup(void) {
    Leaf::setup();
    LEAF_ENTER(L_NOTICE);

    registerLeafBoolValue("poll_enable", &poll_enable, "Enable automatic polling of modbus read ranges");
    registerLeafIntValue("reponse_timeout_ms", &response_timeout_ms);
    registerLeafIntValue("retry_delay_ms", &retry_delay_ms);
    registerLeafIntValue("retry_count", &retry_count);
    registerCommand(HERE, "write-register", "Write to a modbus holding register");
    registerCommand(HERE, "write-coil", "Write to a modbus coil");
    registerCommand(HERE, "write-unit-register/", "Write to a modbus unit's holding register");
    registerCommand(HERE, "read-register", "Read from a modbus holding register");
    registerCommand(HERE, "read-input", "Read from a modbus input register");
    registerCommand(HERE, "read-coil", "Read from a modbus coil");
    registerCommand(HERE, "read-discrete-input", "Read from a modbus discrete input");
    registerCommand(HERE, "read-unit-register/", "Read from a modbus holding register on a particular unit");
    registerCommand(HERE, "read-register-hex", "Read from a modbus holding register specified in hexadecimal");
    registerCommand(HERE, "read-input-hex", "Read from a modbus input register specified in hexadecimal");
    registerCommand(HERE, "poll", "Immediately poll a nominated modbus read-range");
    registerCommand(HERE, "list", "List ranges");
    registerCommand(HERE, "setDbg");
    
    if (uart >= 0) {
      LEAF_NOTICE("Hardware serial setup baud=%d rx=%d tx=%d", (int)baud, bus_rx_bin, bus_tx_pin);
      ((HardwareSerial *)bus_port)->begin(baud, config, bus_rx_bin, bus_tx_pin);
    }
    LEAF_NOTICE("Modbus begin unit=%d", unit);
    if (getDebugLevel() >= L_INFO) {
      LEAF_INFO("Enabling debug ouput in modbus master library");
      bus->setDbg(&DBG);
    }
    bus->begin(unit, *bus_port);
    bus->setResponseTimeout(response_timeout_ms);

    LEAF_NOTICE("%s claims pins rx/e=%d/%d tx/e=%d/%d", describe().c_str(), bus_rx_bin,bus_re_pin,bus_tx_pin,bus_de_pin);
    if (bus_re_pin>=0) {
      // Set up the Receive Enable pin as output
      LEAF_NOTICE("  enabling RE pin %d as output", bus_re_pin);
      modbus_master_pin_re = bus_re_pin;
      modbus_master_re_assert = !bus_re_invert;
      pinMode(bus_re_pin,OUTPUT);
      digitalWrite(modbus_master_pin_re, modbus_master_re_assert);
    }
    if (bus_de_pin>=0) {
      // Setup the transmit enable (DE) pin as output
      LEAF_NOTICE("  enabling DE pin %d as output", bus_de_pin);
      modbus_master_pin_de = bus_de_pin;
      modbus_master_de_assert = !bus_de_invert;
      pinMode(bus_de_pin,OUTPUT);
      digitalWrite(modbus_master_pin_re, !modbus_master_de_assert);
    }
	
    if ((bus_de_pin>=0) || (bus_re_pin>=0)) {
      // Install a pre-transmission hook to set the state of RE and DE
      LEAF_NOTICE("  installing pre-transmission hook");
      bus->preTransmission([](){
	if (modbus_master_pin_re >= 0) {
	  DEBUG("modbus preTransmission: deassert RE/%d", modbus_master_pin_re);
	  digitalWrite(modbus_master_pin_re, !modbus_master_re_assert);
	}
	if (modbus_master_pin_de >= 0) {
	  DEBUG("modbus preTransmission: assert DE/%d", modbus_master_pin_de);
	  digitalWrite(modbus_master_pin_de, modbus_master_de_assert);
	}
      });
    }
    if ((bus_de_pin>=0) || (bus_re_pin>=0)) {
      // Install a post-transmission hook to return the bus to receive mode
      LEAF_NOTICE("  installing post-transmission hook");
      bus->postTransmission([]() {
	if (modbus_master_pin_de >= 0) {
	  DEBUG("modbus postTransmission: deassert DE/%d", modbus_master_pin_de);
	  digitalWrite(modbus_master_pin_de, !modbus_master_de_assert);
	}
	if (modbus_master_pin_re >= 0) {
	  DEBUG("modbus postTransmission: assert RE/%d", modbus_master_pin_re);
	  digitalWrite(modbus_master_pin_re, modbus_master_re_assert);
	}
      });
    }

    if (readRanges) {
      for (int i=0; i<readRanges->size(); i++) {
	ModbusReadRange *r = readRanges->getData(i);
	String poll_desc = (r->poll_interval==MODBUS_NO_POLL)?
	  "none":
	  (r->poll_interval==MODBUS_POLL_ONCE)?"once":String(r->poll_interval);
	r->setName(getName()+"/"+r->name);
	r->setDebugLevel(getDebugLevel());
	LEAF_NOTICE("   read range %d: %s unit=%d fc=%d addr=%d qty=%d poll=%s",
		    i, r->name, r->unit, r->fc, r->address, r->quantity, poll_desc.c_str());
      }
    }
    
    LEAF_LEAVE;
  }

  virtual void start(void) 
  {
    LEAF_ENTER(L_NOTICE);

    // flush any spam in the rx buffer
    int n = 0;
    while (bus_port->read() != -1) n++;
    if (n) {
      LEAF_WARN("Flushed %d bytes from input buffer", n);
    }

    LEAF_LEAVE;
    Leaf::start();
  }

  void publishRange(ModbusReadRange *range, bool force_publish = false) 
  {
    const int capacity = JSON_ARRAY_SIZE(RANGE_MAX);

    StaticJsonDocument<capacity> doc;
    for (int item = 0; item < range->quantity; item++) {
      if ((range->fc == FC_READ_COIL) || (range->fc == FC_READ_INP)) {
	bool value = range->values[item];
	doc.add(value);
      }
      else {
	uint16_t value = range->values[item];
	doc.add(value);
      }
    }
    String jsonString;
    serializeJson(doc,jsonString);

    //LEAF_DEBUG("%s:%s (fc%d unit%d @%d:%d) <= %s", this->leaf_name.c_str(), range->name.c_str(), range->fc, unit, range->address, range->quantity, jsonString.c_str());

    // If a value is unchanged, do not publish, except do an unconditional
    // send every {dedupe_interval} milliseconds (in case the MQTT server
    // loses a message).
    //
    if (!force_publish) {
      uint32_t unconditional_publish_time = range->last_publish_time + range->dedupe_interval;
      uint32_t now = millis();
      if ( (unconditional_publish_time <= now) || (jsonString != range->last_publish_value)) {
	range->last_publish_time = now;
	range->last_publish_value = jsonString;
	force_publish = true;
      }
    }
    if (force_publish) {
      //LEAF_INFO("%s:%s (fc%d unit%d @%d:%d) <= %s", this->leaf_name.c_str(), range->name.c_str(), range->fc, unit, range->address, range->quantity, jsonString.c_str());
      mqtt_publish(String("status/")+range->getName(), jsonString, 0, false, L_NOTICE, HERE);
    }
  }
  
  void loop(void) {
    char buf[160];

    Leaf::loop();
    //LEAF_ENTER(L_NOTICE);
    uint32_t now = millis();

    if (now >= last_read + read_throttle) {
      for (int range_idx = 0; range_idx < readRanges->size(); range_idx++) {
	ModbusReadRange *range = this->readRanges->getData(range_idx);
	//LEAF_NOTICE("Checking whether to poll range %d (%s)", range_idx, range->name.c_str());
	if (range->needsPoll()) {
	  //LEAF_NOTICE("Doing poll of range %d (%s)", range_idx, range->name.c_str());
	  this->pollRange
	    (range, RANGE_ADDRESS_DEFAULT, false);
	  //LEAF_NOTICE("  poll range done");
	  break; // poll only one range per loop to give better round robin
	}
      }
    }
    else {
      LEAF_INFO("Suppress read for rate throttling");
    }

    //LEAF_LEAVE;
  }

  virtual void range_pub(String filter="") 
  {
    for (int range_idx = 0; range_idx < readRanges->size(); range_idx++) {
      ModbusReadRange *range = this->readRanges->getData(range_idx);
      if ((filter.length()>0) && (range->name.indexOf(filter)<0)) {
	// does not match filter
	continue;
      }
      LEAF_NOTICE("Range %d: %s", range_idx, range->name.c_str());
      DumpHex(L_NOTICE, "  range values", range->values, range->quantity*sizeof(uint16_t));
      publishRange(range, true);
    }
  }

  virtual bool commandHandler(String type, String name, String topic, String payload) {
    LEAF_HANDLER(L_INFO);

    WHEN("write-register",{
      String a;
      String v;
      int pos;
      uint16_t address;
      uint16_t value;
      //LEAF_INFO("Writing to register %s", payload.c_str());
      if ((pos = payload.indexOf('=')) > 0) {
	a = payload.substring(0, pos);
	v = payload.substring(pos+1);
	address = a.toInt();
	value = v.toInt();
	LEAF_NOTICE("MQTT COMMAND TO WRITE REGISTER %d <= %d", (int)address, (int)value);
	uint8_t result = bus->writeSingleRegister(address, value);
	String reply_topic = "status/write-register/"+a;
	if (result == 0) {
	  //LEAF_INFO("Write succeeeded at %d", address);
	  mqtt_publish(reply_topic, String((int)value),0,false,L_INFO,HERE);
	}
	else {
	  LEAF_ALERT("Write error = %d (%s)", (int)result, exceptionName(result));
	  mqtt_publish(reply_topic, String("ERROR ")+String(result)+" "+exceptionName(result),0,false,L_ALERT,HERE);
	}
      }
    })
    ELSEWHENPREFIX("write-unit-register/",{
      int unit = topic.toInt();
      if (unit==0) {
	unit = this->unit;
	
      };
      if (unit != this->unit) {
	bus->begin(unit, *bus_port);
      };
      String a;
      String v;
      int pos;
      uint16_t address;
      uint16_t value;
      if ((pos = payload.indexOf('=')) > 0) {
	a = payload.substring(0, pos);
	v = payload.substring(pos+1);
	address = a.toInt();
	value = v.toInt();
	uint8_t result = bus->writeSingleRegister(address, value);
	if (unit != this->unit) {
	  bus->begin(this->unit, *bus_port);
	}
	String reply_topic = "status/write-register/"+a;
	if (result == 0) {
	  mqtt_publish(reply_topic, String((int)value),0,false,L_INFO,HERE);
	}
	else {
	  mqtt_publish(reply_topic, String("ERROR ")+String(result)+" "+exceptionName(result),0,false,L_ALERT,HERE);
	}
      };
    })
    ELSEWHEN("read-register",{
      uint16_t address;
      address = payload.toInt();
      LEAF_NOTICE("MQTT COMMAND TO READ REGISTER %d", (int)address);
      uint8_t result = bus->readHoldingRegisters(address, 1);
      String reply_topic = "status/read-register/"+payload;
      if (result == 0) {
	mqtt_publish(reply_topic, String((int)bus->getResponseBuffer(0)),0,false,L_INFO,HERE);
      }
      else {
	LEAF_ALERT("Read error = %d", (int)result);
	mqtt_publish(reply_topic, String("ERROR "+String(result)),0,false,L_ALERT,HERE);
      }
    })
    ELSEWHEN("read-input",{
      uint16_t address;
      address = payload.toInt();
      LEAF_NOTICE("MQTT COMMAND TO READ INPUT REGISTER %d", (int)address);
      uint8_t result = bus->readInputRegisters(address, 1);
      String reply_topic = "status/read-input/"+payload;
      if (result == 0) {
	mqtt_publish(reply_topic, String((int)bus->getResponseBuffer(0)),0,false,L_INFO,HERE);
      }
      else {
	LEAF_ALERT("Read error = %d", (int)result);
	mqtt_publish(reply_topic, String("ERROR "+String(result)),0,false,L_ALERT,HERE);
      }
    })
    ELSEWHENPREFIX("read-unit-register/",{
      int unit = topic.toInt();
      uint16_t address;
      if (unit != this->unit) {
	bus->begin(unit, *bus_port);
      }
      address = payload.toInt();
      LEAF_NOTICE("MQTT COMMAND TO READ REGISTER %d", (int)address);
      uint8_t result = bus->readHoldingRegisters(address, 1);
      if (unit != this->unit) {
	bus->begin(this->unit, *bus_port);
      }
      String reply_topic = String("status/read-unit-register/")+unit+"/"+payload;
      if (result == 0) {
	mqtt_publish(reply_topic, String((int)bus->getResponseBuffer(0)),0,false,L_INFO,HERE);
      }
      else {
	LEAF_ALERT("Read error = %d", (int)result);
	mqtt_publish(reply_topic, String("ERROR "+String(result)),0,false,L_ALERT,HERE);
      }
    })
    ELSEWHEN("read-register-hex",{
      uint16_t address;
      address = strtol(payload.c_str(), NULL, 16);
      LEAF_NOTICE("MQTT COMMAND TO READ REGISTER %d", (int)address);
      uint8_t result = bus->readHoldingRegisters(address, 1);
      String reply_topic = "status/read-register/"+payload;
      if (result == 0) {
      char buf[10];
      snprintf(buf, sizeof(buf), "0x%x", (int)bus->getResponseBuffer(0));
      mqtt_publish(reply_topic, String(buf), 0, false,L_INFO,HERE);
      }
      else {
	LEAF_ALERT("Read error = %d", (int)result);
	mqtt_publish(reply_topic, String("ERROR "+String(result)),0,false,L_ALERT,HERE);
      }
    })
    ELSEWHEN("list",range_pub(payload))
    ELSEWHEN("poll",{
      bool is_all = (payload == "*");
      bool is_regex = (payload.startsWith("/") && payload.endsWith("/"));
      if  (is_all || is_regex) {
	regex_t reg;
	if (is_regex) {
	  String re_src = payload.substring(1,payload.length()-1);
	  LEAF_NOTICE("Compiling regex [%s]", re_src.c_str());
	  if (regcomp(&reg, re_src.c_str(), REG_EXTENDED|REG_ICASE|REG_NOSUB) != 0) {
	    LEAF_ALERT("Bad regex [%s]", re_src.c_str());
	    LEAF_BOOL_RETURN(true);
	  }
	}
	for (int i=0; i<readRanges->size(); i++) {
	  ModbusReadRange *r = readRanges->getData(i);
	  if (is_regex) {
	    if (regexec(&reg, r->getNameStr(), 0, NULL, 0) == 0) {
	      LEAF_INFO("Regex %s matched [%s]", payload.c_str(), r->getNameStr());
	    }
	    else {
	      continue;
	    }
	  }
	  pollRange(r, RANGE_ADDRESS_DEFAULT, true);
	}
      }
      else {
    ELSEWHEN("read-input-hex",{
      uint16_t address;
      address = strtol(payload.c_str(), NULL, 16);
      LEAF_NOTICE("MQTT COMMAND TO READ INPUT REGISTER %d", (int)address);
      uint8_t result = bus->readInputRegisters(address, 1);
      String reply_topic = "status/read-register/"+payload;
      if (result == 0) {
      char buf[10];
      snprintf(buf, sizeof(buf), "0x%x", (int)bus->getResponseBuffer(0));
      mqtt_publish(reply_topic, String(buf), 0, false,L_INFO,HERE);
      }
      else {
	LEAF_ALERT("Read error = %d", (int)result);
	mqtt_publish(reply_topic, String("ERROR "+String(result)),0,false,L_ALERT,HERE);
      }
    })
    ELSEWHEN("setDbg", {
	if(payload.toInt()) {
	  LEAF_NOTICE("Enabling modbus library debug trace");
	  bus->setDbg(&DBG);
	}
	else {
	  LEAF_NOTICE("Disabling modbus library debug trace");
	  bus->setDbg(NULL);
	}
    })
    else {
      handled = Leaf::commandHandler(type, name, topic, payload);
	}
    LEAF_HANDLER_END;
  }
  

  virtual bool mqtt_receive(String type, String name, String topic, String payload, bool direct=false) {
    LEAF_ENTER(L_INFO);
    bool handled = false;

    LEAF_INFO("RECV %s %s %s %s", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());

    WHEN("set/poll-interval",{
      String range;
      String interval;
      int pos;
      if ((pos = payload.indexOf('=')) > 0) {
	range = payload.substring(0, pos);
	interval = payload.substring(pos+1);
	int value = interval.toInt();
	//LEAF_INFO("Setting poll interval %s", payload.c_str());
	this->setRangePoll(range, value);
      }
    })
    else {
      handled = Leaf::mqtt_receive(type, name, topic, payload, direct);
    }

    RETURN(handled);
  }

  bool pollRange(ModbusReadRange *range, int unit=-1, bool force_publish=false)
  {
    LEAF_ENTER_INTCSTR(L_INFO, unit, range->name.c_str());

    uint8_t result = 0xe2;
    int retry = 0;
    if (unit == -1) unit=range->unit;
    if (unit == -1) unit=this->unit;

    LEAF_INFO("Polling range '%s', range unit %d derived unit %d (FC %d register %d:%d", range->name.c_str(), range->unit, unit, range->fc, range->address, range->quantity);

    if (unit == -1) {
      LEAF_ALERT("No valid unit number provided for %s", range->name.c_str());
      LEAF_BOOL_RETURN(false);
    }
    
    if (unit != last_unit) {
      // address a different slave device than the last time
      LEAF_INFO("Set bus unit id to %d", unit);
      bus->begin(unit, *bus_port);
      last_unit = unit;
    }

    uint32_t begin = millis();
    wdtReset(HERE);
    do {
      if (range->fc == FC_READ_COIL) {
	LEAF_INFO("Read %s coils unit %d @ %d:%d", range->name.c_str(), unit, range->address, range->quantity);
	result = bus->readCoils(range->address, range->quantity);
      }
      else if (range->fc == FC_READ_INP) {
	LEAF_INFO("Read %s inputs unit %d @ %d:%d", range->name.c_str(), unit, range->address, range->quantity);
	result = bus->readDiscreteInputs(range->address, range->quantity);
      }
      else if (range->fc == FC_READ_HOLD_REG) {
	LEAF_INFO("Read %s holding registers unit %d @ %d:%d", range->name.c_str(), unit, range->address, range->quantity);
	result = bus->readHoldingRegisters(range->address, range->quantity);
      }
      else if (range->fc == FC_READ_INP_REG) {
	LEAF_INFO("Read %s input registers unit %d @ %d:%d", range->name.c_str(), unit, range->address, range->quantity);
	result = bus->readInputRegisters(range->address, range->quantity);
      }
      else {
	LEAF_ALERT("Unsupported function code");
	LEAF_BOOL_RETURN(false);
      }
      
      LEAF_INFO("Transaction result is %d (%s)", (int) result, exceptionName(result));
      
      if (result == bus->ku8MBSuccess) {

	for (int item = 0; item < range->quantity; item++) {
	  if ((range->fc == FC_READ_COIL) || (range->fc == FC_READ_INP)) {
	    int word = item/16;
	    int shift = item%16;
	    int bits = bus->getResponseBuffer(word);
	    range->values[item] = (bits>>shift)&0x01;
	    LEAF_INFO("Binary Item %s/%d is word %d,bit%d (%04x => %d)", range->name.c_str(), item, word, shift, bits, (int)range->values[item]);
	  }
	  else {
	    uint16_t value = bus->getResponseBuffer(item);
	    range->values[item] = value;
	    LEAF_INFO("Register Item %s/%d => %d)", range->name.c_str(), item, (int)value);
	  }
	}
	
	publishRange(range, force_publish);
	last_read = millis();
	delay(500); // TODO: improve throttling

      }
      else {
	LEAF_WARN("Modbus read error (attempt %d) for %s: 0x%02x (%s)", retry, range->name.c_str(), (int)result, exceptionName(result));
	++retry;
	delay(retry_delay_ms);
      }
      wdtReset(HERE);
    } while ((result != 0) && (retry < retry_count));
    if (result != 0) {
      LEAF_ALERT("Modbus read error after %d retries in %s for %s (f%d:u%d:a%d:q%d): 0x%02x (%s)", retry, leaf_name.c_str(), range->name.c_str(), range->fc, unit, range->address, range->quantity, (int)result, exceptionName(result));	
      LEAF_BOOL_RETURN(false);
    }
    range->last_poll = millis();

    LEAF_BOOL_RETURN(true);
  }

  uint8_t writeRegister(uint16_t address, uint16_t value, int unit=0)
  {
    LEAF_ENTER(L_DEBUG);

    uint8_t rc = 0;
    int retry = 0;
    if (unit==0) unit=this->unit;
    //bus->clearTransmitBuffer();
    //bus->setTransmitBuffer(0, value);
    //rc = bus->writeMultipleRegisters(address, 1);
    if (fake_writes) {
      LEAF_NOTICE("FAKE MODBUS WRITE: %hu <= %hu", address, value);
    }
    else {
      do {
	bus->begin(unit, *bus_port);
	LEAF_INFO("MODBUS WRITE: %hu <= %hu", address, value);
	rc = bus->writeSingleRegister(address, value); // crashy
	if (rc != 0) {
	  LEAF_INFO("Modbus write error (attempt %d) in %s for %hu=%hu error 0x%02x", retry, leaf_name.c_str(), address, value, (int)rc);
	  delay(retry_delay_ms);
	  ++retry;

	}
      } while ((rc != 0) && (retry < retry_count));

      if (rc != 0) {
	LEAF_ALERT("Modbus write error after %d retries in %s for %hu=%hu error 0x%02x", retry, leaf_name.c_str(), address, value, (int)rc);
      }
      //LEAF_DEBUG("MODBUS WRITE RESULT: %d", (int)rc);
    }

    RETURN(rc);
  }

  ModbusReadRange *getRange(String name, String caller="unspecified caller")
  {
    ModbusReadRange *range = readRanges->get(name);
    if (range == NULL) {
      LEAF_ALERT("Cannot find %s read range for %s", name, this->leaf_name);
    }
    return range;
  }

  uint16_t *getRangeValues(String name, String caller="unspecified caller")
  {
    ModbusReadRange *range = getRange(name, caller);
    if (range == NULL) {
      return NULL;
    }
    return range->values;
  }

  void setRangePoll(String name, int poll_ms, String caller="unspecified caller")
  {
    ModbusReadRange *range = getRange(name, caller);
    if (range == NULL) {
      return;
    }
    range->poll_interval = poll_ms;

  }


};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
