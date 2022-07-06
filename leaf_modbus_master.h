#pragma once
//
//@************************* class ModbusMasterLeaf **************************
//
// This class encapsulates a generic modbus transciever
//

#include <HardwareSerial.h>
#include <ModbusMaster.h>

#define RANGE_MAX 64

class ModbusReadRange
{
public:
  String name;
  int fc;
  int address;
  int quantity;
  uint32_t poll_interval;
  uint16_t *values = NULL;
  uint32_t last_poll = 0;
  uint32_t dedupe_interval = 60*1000;
  uint32_t last_publish_time = 0;
  String last_publish_value = "";


  ModbusReadRange(String name, int address, int quantity=1, int fc=3, uint32_t poll_interval = 5000, int dedupe_interval=60*1000)
  {
    this->name = name;
    this->fc = fc;
    this->address = address;
    this->quantity = quantity;
    this->poll_interval = poll_interval;
    this->dedupe_interval = 60*1000;
    this->values = (uint16_t *)calloc(quantity, sizeof(uint16_t));
  }

  bool needsPoll(void)
  {
    bool result = false;
    //ENTER(L_DEBUG);

    uint32_t now = millis();
    uint32_t next_poll = this->last_poll + this->poll_interval;
    if (next_poll <= now) {
      this->last_poll = now;
      //NOTICE("needsPoll YES");
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


class ModbusMasterLeaf : public Leaf
{
  SimpleMap <String,ModbusReadRange*> *readRanges;
  int unit;
  int uart;
  int baud;
  int rxpin;
  int txpin;
  HardwareSerial *port;
  uint32_t config;
  ModbusMaster *bus = NULL;
  bool fake_writes = false;
  uint32_t last_read = 0;
  uint32_t read_throttle = 50;

public:
  ModbusMasterLeaf(String name, pinmask_t pins,
		   ModbusReadRange **readRanges,
		   int unit=1,
		   int uart=2, int baud=9600,
		   uint32_t config=SERIAL_8N1,
		   int rxpin=16,int txpin=17
    ) : Leaf("modbusMaster", name, pins) {
    LEAF_ENTER(L_INFO);
    this->readRanges = new SimpleMap<String,ModbusReadRange*>(_compareStringKeys);
    for (int i=0; readRanges[i]; i++) {
      this->readRanges->put(readRanges[i]->name, readRanges[i]);
    }
    this->unit = unit;
    this->uart = uart;
    this->baud = baud;
    this->rxpin = rxpin;
    this->txpin = txpin;
    this->config = config;
    this->port = new HardwareSerial(uart);
    this->bus = new ModbusMaster();

    LEAF_LEAVE;
  }

  void setup(void) {
    Leaf::setup();
    LEAF_ENTER(L_NOTICE);
    port->begin(baud, config, rxpin, txpin);
    bus->begin(unit, *port);

    LEAF_LEAVE;

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
	  //LEAF_NOTICE("Doing poll of range %d", range_idx);
	  this->pollRange(range);
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
    mqtt_subscribe("cmd/write-register");
    mqtt_subscribe("cmd/read-register");
    mqtt_subscribe("cmd/read-register-hex");
    mqtt_subscribe("set/poll-interval");
    LEAF_LEAVE;
  }

  bool mqtt_receive(String type, String name, String topic, String payload) {
    LEAF_ENTER(L_INFO);
    bool handled = Leaf::mqtt_receive(type, name, topic, payload);

    LEAF_INFO("%s %s %s %s", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());

    if (topic == "cmd/write-register") {
      LEAF_INFO("Writing to register %s", payload.c_str());
      String a,v;
      int pos;
      uint16_t address;
      uint16_t value;
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
	  this->mqtt_publish(reply_topic, String((int)value),0,false);
	}
	else {
	  LEAF_ALERT("Write error = %d", (int)result);
	  this->mqtt_publish(reply_topic, String("ERROR "+String(result)),0,false);
	}
      }
      handled = true;
    }
    if (topic == "cmd/read-register") {
      LEAF_INFO("Reading from register %s", payload.c_str());
      uint16_t address;
      address = payload.toInt();
      LEAF_NOTICE("MQTT COMMAND TO READ REGISTER %d", (int)address);
      uint8_t result = bus->readHoldingRegisters(address, 1);
      String reply_topic = "status/read-register/"+payload;
      if (result == 0) {
	this->mqtt_publish(reply_topic, String((int)bus->getResponseBuffer(0)),0,false);
      }
      else {
	LEAF_ALERT("Read error = %d", (int)result);
	this->mqtt_publish(reply_topic, String("ERROR "+String(result)),0,false);
      }
      handled = true;
    }
    if (topic == "cmd/read-register-hex") {
      LEAF_INFO("Reading from register %s", payload.c_str());
      uint16_t address;
      address = payload.toInt();
      LEAF_NOTICE("MQTT COMMAND TO READ REGISTER %d", (int)address);
      uint8_t result = bus->readHoldingRegisters(address, 1);
      String reply_topic = "status/read-register/"+payload;
      if (result == 0) {
      char buf[10];
      snprintf(buf, sizeof(buf), "0x%x", (int)bus->getResponseBuffer(0));
      this->mqtt_publish(reply_topic, String(buf), 0, false);
      }
      else {
	LEAF_ALERT("Read error = %d", (int)result);
	this->mqtt_publish(reply_topic, String("ERROR "+String(result)),0,false);
      }
      handled = true;
    }
    else if (topic == "set/poll-interval") {
      LEAF_INFO("Setting poll interval %s", payload.c_str());
      String range,interval;
      int pos;
      if ((pos = payload.indexOf('=')) > 0) {
	range = payload.substring(0, pos);
	interval = payload.substring(pos+1);
	int value = interval.toInt();
	this->setRangePoll(range, value);
      }
      handled = true;
    }

    RETURN(handled);

  }

  void pollRange(ModbusReadRange *range)
  {
    LEAF_ENTER(L_DEBUG);

    uint8_t result = 0xe2;
    int retry = 1;

    do {
      result = bus->readHoldingRegisters(range->address, range->quantity);
      if (result == bus->ku8MBSuccess) {
	const int capacity = JSON_ARRAY_SIZE(RANGE_MAX);
	StaticJsonDocument<capacity> doc;
	for (int word = 0; word < range->quantity; word++) {
	  uint16_t value = bus->getResponseBuffer(word);
	  range->values[word] = value;
	  doc.add(value);
	}
	String jsonString;
	serializeJson(doc,jsonString);
	LEAF_INFO("%s:%s (fc%d@%d:%d) <= %s", this->leaf_name.c_str(), range->name.c_str(), range->fc, range->address, range->quantity, jsonString.c_str());

	// If a value is unchanged, do not publish, except do an unconditional
	// send every {dedupe_interval} milliseconds (in case the MQTT server
	// loses a message).
	//
	uint32_t unconditional_publish_time = range->last_publish_time + range->dedupe_interval;
	uint32_t now = millis();
	if ( (unconditional_publish_time <= now) || (jsonString != range->last_publish_value)) {
	  range->last_publish_time = now;
	  range->last_publish_value = jsonString;
	  this->mqtt_publish(range->name, jsonString, 0, false);
	}

	last_read = millis();
      }
      else {
	LEAF_INFO("Modbus read error (attempt %d) in %s for %s: 0x%02x", retry, leaf_name.c_str(), range->name.c_str(), (int)result);
	delay(50);
	++retry;
      }
    } while ((result != 0) && (retry <= 3));
    if (result != 0) {
      LEAF_ALERT("Modbus read error after %d retries in %s for %s: 0x%02x", retry, leaf_name.c_str(), range->name.c_str(), (int)result);
    }

    LEAF_LEAVE;
  }

  uint8_t writeRegister(uint16_t address, uint16_t value)
  {
    LEAF_ENTER(L_DEBUG);
    uint8_t rc = 0;
    int retry = 1;
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
