#pragma once
#pragma STACX_LIB ModbusMaster
//
//@************************* class ModbusMasterLeaf **************************
//
// This class encapsulates a generic modbus transciever
//

#include <HardwareSerial.h>
#include <ModbusMaster.h>

#define RANGE_MAX 64

#define FC_READ_COIL      0x01
#define FC_READ_INP      0x02
#define FC_READ_HOLD_REG  0x03
#define FC_READ_INP_REG   0x04

#define MODBUS_NO_POLL 0

class ModbusReadRange
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
    //ENTER(L_DEBUG);
    if (this->poll_interval == MODBUS_NO_POLL) return false;

    bool result = false;

    uint32_t now = millis();
    uint32_t next_poll = this->last_poll + this->poll_interval;
    if (next_poll <= now) {
      this->last_poll = now;
      DEBUG("needsPoll %s YES", name);
      result = true;
    }
    else {
      int until_next = next_poll - now;
      //NOTICE("Not time to poll yet (wait %d)", (int)until_next);
    }
    //LEAVE;
    return result;
  }
};


int modbus_master_pin_de = -1;
int modbus_master_de_assert = -1;
int modbus_master_pin_re = -1;
int modbus_master_re_assert = -1;


class ModbusMasterLeaf : public Leaf
{
  SimpleMap <String,ModbusReadRange*> *readRanges;
  int unit;
  int uart;
  int baud;
  int rxpin;
  int txpin;
  int repin;
  int depin;
  bool re_invert;
  bool de_invert;
  Stream *port;
  uint32_t config;
  ModbusMaster *bus = NULL;
  bool fake_writes = false;
  uint32_t last_read = 0;
  uint32_t read_throttle = 50;
  int last_unit = 0;

public:
  ModbusMasterLeaf(String name, pinmask_t pins=NO_PINS,
		   ModbusReadRange **readRanges=NULL,
		   int unit=0,
		   int uart=-1, int baud=9600,
		   uint32_t config=SERIAL_8N1,
		   int rxpin=-1,int txpin=-1,
		   int repin=-1,int depin=-1,
		   bool re_invert=true,bool de_invert=false,
		   Stream *stream=NULL)
    : Leaf("modbusMaster", name, pins)
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
    this->rxpin = rxpin;
    this->txpin = txpin;
    this->repin = repin;
    this->depin = depin;
    this->re_invert=re_invert;
    this->de_invert=de_invert;
    this->config = config;
    if (stream) {
      this->port = stream;
    }
    else {
      this->port = (Stream *)new HardwareSerial(uart);
    }
    this->bus = new ModbusMaster();

    LEAF_LEAVE;
  }

  void setup(void) {
    Leaf::setup();
    LEAF_ENTER(L_INFO);
    if (uart >= 0) {
      LEAF_NOTICE("Hardware serial setup baud=%d rx=%d tx=%d", (int)baud, rxpin, txpin);
      ((HardwareSerial *)port)->begin(baud, config, rxpin, txpin);
    }
    LEAF_NOTICE("Modbus begin unit=%d", unit);
    //bus->setDbg(&Serial);
    bus->begin(unit, *port);

    LEAF_NOTICE("%s claims pins rx/e=%d/%d tx/e=%d/%d", describe().c_str(), rxpin,repin,txpin,depin);
    if (repin>=0) {
      // Set up the Receive Enable pin as output
      LEAF_NOTICE("  enabling RE pin %d as output", repin);
      modbus_master_pin_re = repin;
      modbus_master_re_assert = !re_invert;
      pinMode(repin,OUTPUT);
    }
    if (depin>=0) {
      // Setup the transmit enable (DE) pin as output
      LEAF_NOTICE("  enabling DE pin %d as output", depin);
      modbus_master_pin_de = depin;
      modbus_master_de_assert = !de_invert;
      pinMode(depin,OUTPUT);
    }
	
    if ((depin>=0) || (repin>=0)) {
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
    if ((depin>=0) || (repin>=0)) {
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
    LEAF_LEAVE;
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

    LEAF_DEBUG("%s:%s (fc%d unit%d @%d:%d) <= %s", this->leaf_name.c_str(), range->name.c_str(), range->fc, unit, range->address, range->quantity, jsonString.c_str());

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
      LEAF_INFO("%s:%s (fc%d unit%d @%d:%d) <= %s", this->leaf_name.c_str(), range->name.c_str(), range->fc, unit, range->address, range->quantity, jsonString.c_str());
      mqtt_publish(range->name, jsonString, 0, false, L_NOTICE, HERE);
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
	    (range, 0, false);
	  //LEAF_NOTICE("  poll range done");
	  break; // poll only one range per loop to give better round robin
	}
      }
    }
    else {
      //LEAF_NOTICE("Suppress read for rate throttling");
    }

    //LEAF_LEAVE;
  }

  void mqtt_do_subscribe() {
    LEAF_ENTER(L_DEBUG);
    Leaf::mqtt_do_subscribe();
    if (!leaf_mute) {
      mqtt_subscribe("cmd/write-register", HERE);
      mqtt_subscribe("cmd/read-register", HERE);
      mqtt_subscribe("cmd/write-unit-register/+", HERE);
      mqtt_subscribe("cmd/read-unit-register/+", HERE);
      mqtt_subscribe("cmd/read-register-hex", HERE);
      mqtt_subscribe("set/poll-interval", HERE);
    }
    LEAF_LEAVE;
  }

  virtual void status_pub() 
  {
    for (int range_idx = 0; range_idx < readRanges->size(); range_idx++) {
      ModbusReadRange *range = this->readRanges->getData(range_idx);
      LEAF_NOTICE("Range %d: %s", range_idx, range->name.c_str());
      DumpHex(L_NOTICE, "  range values", range->values, range->quantity*sizeof(uint16_t));
      publishRange(range, true);
    }
  }
  

  virtual bool mqtt_receive(String type, String name, String topic, String payload, bool direct=false) {
    LEAF_ENTER(L_INFO);
    bool handled = false;

    LEAF_INFO("%s %s %s %s", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());

    WHEN("cmd/write-register",{
      String a;
      String v;
      int pos;
      uint16_t address;
      uint16_t value;
      LEAF_INFO("Writing to register %s", payload.c_str());
      if ((pos = payload.indexOf('=')) > 0) {
	a = payload.substring(0, pos);
	v = payload.substring(pos+1);
	address = a.toInt();
	value = v.toInt();
	LEAF_NOTICE("MQTT COMMAND TO WRITE REGISTER %d <= %d", (int)address, (int)value);
	uint8_t result = bus->writeSingleRegister(address, value);
	String reply_topic = "status/write-register/"+a;
	if (result == 0) {
	  LEAF_INFO("Write succeeeded at %d", address);
	  mqtt_publish(reply_topic, String((int)value),0,false,L_INFO,HERE);
	}
	else {
	  LEAF_ALERT("Write error = %d", (int)result);
	  mqtt_publish(reply_topic, String("ERROR "+String(result)),0,false,L_ALERT,HERE);
	}
      }
    })
    ELSEWHENPREFIX("cmd/write-unit-register/",{
      int unit = topic.toInt();
      if (unit==0) {
	unit = this->unit;
	
      };
      if (unit != this->unit) {
	bus->begin(unit, *port);
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
	  bus->begin(this->unit, *port);
	}
	String reply_topic = "status/write-register/"+a;
	if (result == 0) {
	  mqtt_publish(reply_topic, String((int)value),0,false,L_INFO,HERE);
	}
	else {
	  mqtt_publish(reply_topic, String("ERROR "+String(result)),0,false,L_ALERT,HERE);
	}
      };
    })
    ELSEWHEN("cmd/read-register",{
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
    ELSEWHENPREFIX("cmd/read-unit-register/",{
      int unit = topic.toInt();
      uint16_t address;
      if (unit != this->unit) {
	bus->begin(unit, *port);
      }
      address = payload.toInt();
      LEAF_NOTICE("MQTT COMMAND TO READ REGISTER %d", (int)address);
      uint8_t result = bus->readHoldingRegisters(address, 1);
      if (unit != this->unit) {
	bus->begin(this->unit, *port);
      }
      String reply_topic = "status/read-register/"+payload;
      if (result == 0) {
	mqtt_publish(reply_topic, String((int)bus->getResponseBuffer(0)),0,false,L_INFO,HERE);
      }
      else {
	LEAF_ALERT("Read error = %d", (int)result);
	mqtt_publish(reply_topic, String("ERROR "+String(result)),0,false,L_ALERT,HERE);
      }
    })
    ELSEWHEN("cmd/read-register-hex",{
      uint16_t address;
      address = payload.toInt();
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
    ELSEWHEN("set/poll-interval",{
      String range;
      String interval;
      int pos;
      if ((pos = payload.indexOf('=')) > 0) {
	range = payload.substring(0, pos);
	interval = payload.substring(pos+1);
	int value = interval.toInt();
	LEAF_INFO("Setting poll interval %s", payload.c_str());
	this->setRangePoll(range, value);
      }
    })
    ELSEWHEN("cmd/poll",{
	ModbusReadRange *range = getRange(payload);
	if (range) {
	  pollRange(range, 0, true);
	}
	else {
	  LEAF_WARN("Read range [%s] not found", payload.c_str());
	}
    })
    else {
      handled = Leaf::mqtt_receive(type, name, topic, payload, direct);
    }

    RETURN(handled);
  }

  void pollRange(ModbusReadRange *range, int unit=0, bool force_publish=false)
  {
    LEAF_ENTER(L_DEBUG);

    uint8_t result = 0xe2;
    int retry = 1;
    if (unit == 0) unit=range->unit;
    if (unit == 0) unit=this->unit;

    if (unit == 0) {
      LEAF_ALERT("No valid unit number provided for %s", range->name.c_str());
      LEAF_VOID_RETURN;
    }
    
    if (unit != last_unit) {
      // address a different slave device than the last time
      LEAF_DEBUG("Set bus unit id to %d", unit);
      bus->begin(unit, *port);
      last_unit = unit;
    }

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
	LEAF_VOID_RETURN;
      }
      
      LEAF_INFO("Transaction result is %d", (int) result);
      
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
      }
      else {
	LEAF_WARN("Modbus read error (attempt %d) in %s for %s: 0x%02x", retry, leaf_name.c_str(), range->name.c_str(), (int)result);
	delay(50);
	++retry;
      }
    } while ((result != 0) && (retry <= 3));
    if (result != 0) {
      LEAF_ALERT("Modbus read error after %d retries in %s for %s: 0x%02x", retry, leaf_name.c_str(), range->name.c_str(), (int)result);
    }

    LEAF_LEAVE;
  }

  uint8_t writeRegister(uint16_t address, uint16_t value, int unit=0)
  {
    LEAF_ENTER(L_DEBUG);

    uint8_t rc = 0;
    int retry = 1;
    if (unit==0) unit=this->unit;
    //bus->clearTransmitBuffer();
    //bus->setTransmitBuffer(0, value);
    //rc = bus->writeMultipleRegisters(address, 1);
    if (fake_writes) {
      LEAF_NOTICE("FAKE MODBUS WRITE: %hu <= %hu", address, value);
    }
    else {
      do {
	bus->begin(unit, *port);
	LEAF_INFO("MODBUS WRITE: %hu <= %hu", address, value);
	rc = bus->writeSingleRegister(address, value); // crashy
	if (rc != 0) {
	  LEAF_INFO("Modbus write error (attempt %d) in %s for %hu=%hu error 0x%02x", retry, leaf_name.c_str(), address, value, (int)rc);
	  delay(100);
	  ++retry;

	}
      } while ((rc != 0) && (retry <= 3));

      if (rc != 0) {
	LEAF_ALERT("Modbus write error after %d retries in %s for %hu=%hu error 0x%02x", retry, leaf_name.c_str(), address, value, (int)rc);
      }
      LEAF_DEBUG("MODBUS WRITE RESULT: %d", (int)rc);
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
