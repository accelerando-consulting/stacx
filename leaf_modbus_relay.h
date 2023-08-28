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

#ifndef MODBUS_RELAY_TIMEOUT_MSEC
#define MODBUS_RELAY_TIMEOUT_MSEC 5000
#endif

#ifndef MODBUS_RELAY_TEST_PAYLOAD
#define MODBUS_RELAY_TEST_PAYLOAD "0104310D0001AEF5"
#endif

#ifndef MODBUS_RELAY_TEST_AT_START
#define MODBUS_RELAY_TEST_AT_START false
#endif

class ModbusRelayLeaf : public ModbusMasterLeaf
{
  Stream *relay_port;
  bool test_at_start = MODBUS_RELAY_TEST_AT_START;
  bool tested_at_start = false;
  enum _busdir bus_direction = READING;
  String test_payload = MODBUS_RELAY_TEST_PAYLOAD;
  int transaction_timeout_ms = MODBUS_RELAY_TIMEOUT_MSEC;
  unsigned long transaction_start_ms = 0;

  unsigned long up_word_count = 0;
  unsigned long up_byte_count = 0;
  unsigned long down_word_count = 0;
  unsigned long down_byte_count = 0;

public:
  ModbusRelayLeaf(String name,
		  String target,
		  Stream *relay_port,
		  ModbusReadRange **readRanges=NULL,
		  int bus_uart=1,
		  int baud = 115200,
		  int options = SERIAL_8N1,
		  int bus_rx_pin=-1,
		  int bus_tx_pin=-1,
		  int bus_nre_pin=-1,
		  int bus_de_pin=-1)
    : ModbusMasterLeaf(name, target, readRanges, 0,
		       bus_uart, baud, options, bus_rx_pin, bus_tx_pin,
		       bus_nre_pin, bus_de_pin, /*re-invert*/true, /*de-invert*/false,
		       /*bus stream*/NULL)
    , Debuggable(name)
  {
    LEAF_ENTER(L_INFO);
    this->relay_port = relay_port;
    LEAF_LEAVE;
  }

  void setup(void) {
    ModbusMasterLeaf::setup();
    LEAF_ENTER(L_NOTICE);

    registerCommand(HERE,"modbus_relay_send", "send bytes (hex) to modbus");
    registerCommand(HERE,"modbus_relay_test", "send a test payload to modbus");
    registerLeafStrValue("test_payload", &test_payload, "test bytes (hex) to modbus");
    registerLeafBoolValue("test_at_start", &test_at_start, "perform a test transaction at startup");
    registerLeafIntValue("timeout_ms", &transaction_timeout_ms, "Modbus transaction timeout (ms)");

    LEAF_LEAVE;
  }

  void start(void)
  {
    ModbusMasterLeaf::start();
    LEAF_ENTER(L_NOTICE);
    if (test_at_start) tested_at_start=false; // loop will do test
    LEAF_LEAVE;
  }

  void status_pub()
  {
    ModbusMasterLeaf::status_pub();
    LEAF_ENTER(L_NOTICE);
    String prefix = "status/" + getName() + "/";
    mqtt_publish(prefix+"up_word_count", String(up_word_count));
    mqtt_publish(prefix+"down_word_count", String(down_word_count));
    mqtt_publish(prefix+"up_byte_count", String(up_byte_count));
    mqtt_publish(prefix+"down_byte_count", String(down_byte_count));
    LEAF_LEAVE;
  }

  void set_direction(enum _busdir dir, codepoint_t where=undisclosed_location)
  {
    if (dir==READING) {
      if (bus_direction == READING) {
	// nothing to do
	return;
      }
      if (bus_re_pin>=0) {
	digitalWrite(bus_re_pin, !bus_re_invert);
      }
      if (bus_de_pin>=0) {
	digitalWrite(bus_de_pin, bus_de_invert);
      }
      LEAF_NOTICE_AT(CODEPOINT(where), "MODBUS relay direction READING");
      bus_direction = dir;
    }
    else if (dir==WRITING) {
      if (bus_direction == WRITING) {
	// nothing to do
	LEAF_WARN_AT(CODEPOINT(where), "MODBUS relay direction was already WRITING");
	return;
      }
      if (bus_re_pin>=0) {
	digitalWrite(bus_re_pin, bus_re_invert);
      }
      if (bus_de_pin>=0) {
	digitalWrite(bus_de_pin, !bus_de_invert);
      }
      LEAF_NOTICE_AT(CODEPOINT(where), "MODBUS relay direction WRITING");
      bus_direction = dir;
    }
    else {
      LEAF_ALERT_AT(CODEPOINT(where), "set_direction canthappen invalid direction %d", (int)dir);
    }
  }

  virtual String test_send(String payload)
  {
    LEAF_ENTER_STR(L_NOTICE, payload);
    char buf[3];
    publish("event/test_send", String("start"));
    LEAF_NOTICE("Modbus Test Send [%s]", payload.c_str());
    set_direction(WRITING, HERE);
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
    set_direction(READING, HERE);
    delay(100);

    // wait for up to 5sec for a first response, then stop listening 1sec after the most recent character
    unsigned long now = millis();
    unsigned long start = now;
    unsigned long timeout = now+transaction_timeout_ms;
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
	LEAF_INFO("Read character %02X", (int)c);
	word_timeout = millis()+1000;
	pos += snprintf(response+pos,sizeof(response)-pos, "%02X", (int)c);
	++count;
	if (count >= max_response) break;
      }

      // TODO: once we have a length byte we can stop waiting once we have the whole response
      now = millis();
    }
    if (count == 0) {
      LEAF_WARN("Modbus test timeout: no recv in %d checks over %dms", waits, (int)(now-start));
      snprintf(response, sizeof(response), "TIMEOUT");
      publish("event/test_send", String("timeout"));
    }
    else {
      LEAF_NOTICE("Modbus Test Recv [%s] took %dms", response, (int)(now - start));
      publish("event/test_send", String("complete"));
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
      handled = ModbusMasterLeaf::commandHandler(type, name, topic, payload);
    }

    LEAF_HANDLER_END;
  }

  void loop(void) {
    char buf[128];

    ModbusMasterLeaf::loop();
    //LEAF_ENTER(L_NOTICE);

    if (test_at_start && !tested_at_start) {
      tested_at_start=true;
      LEAF_WARN("Perform startup modbus test");
      String response = test_send(test_payload);
      if (response.length()) {
	LEAF_WARN("Completed (successful) startup modbus test");
      }
    }

    if (transaction_start_ms > 0) {
      unsigned long elapsed = millis() - transaction_start_ms;
      if (elapsed > transaction_timeout_ms) {
	transaction_start_ms = 0;
	LEAF_ALERT("Relayed command timeout (%lu)", elapsed);
	publish("event/timeout", String(elapsed));

      }
    }

    // Look for input from the bus
    bool outofband = false;
    unsigned char c;

    while (true) {
      int count=0;
      if (relay_port->available()) {
	set_direction(WRITING, HERE);
	transaction_start_ms = millis();
	while (relay_port->available()) {
	  c = relay_port->read();
	  if ((count==0) && (c=='#')) {
	    LEAF_NOTICE("Out of band modbus message detected");
	    outofband = true;
	  }
	  if (count < sizeof(buf)) buf[count]=c;
	  ++count;
	  if (outofband) continue;

	  LEAF_INFO("Write char %02X", (int)c);
	  int wrote = bus_port->write(c);
	  if (wrote != 1) {
	    LEAF_ALERT("Downstream relay write error %d", wrote);
	  }
	  else {
	    ++down_byte_count;
	  }
	  bus_port->flush();
	}
	set_direction(READING, HERE);
	// have written all data available from upstream

	if (outofband) {
	  // input was not for relay, it was an out of band backdoor message diverted to MQTT
	  while (count >= sizeof(buf)) --count; // truncate to make room for terminator
	  buf[count]='\0'; // terminate the buffer

	  char *topic = buf+1; // skip the '#' which signified out of band
	  char *payload = strchr(topic, ' ');
	  char empty_payload[2] = "1";
	  if (!payload) {
	    payload=empty_payload;
	  }
	  else {
	    *payload++ = '\0'; // split the buffer
	  }
	  LEAF_WARN("Diverting Modbus to MQTT as [%s] <= [%s]", topic, payload);
	  pubsubLeaf->_mqtt_route(topic, payload);
	  outofband=false;
	  break;
	}

	// Finished writing to the physical modbus, prepare to receive response
	LEAF_NOTICE("Relayed %d bytes to physical bus", count);
	LEAF_NOTIDUMP("Modbus Relay Down", buf, count);
	++down_word_count;
	publish("event/relay_down", String(count));
      }
      else {
	// nothing to write, try reading
	set_direction(READING, HERE);
	int avail = bus_port->available();
	if (avail > 0) {
	  LEAF_NOTICE("There are %d bytes of response data in buffer", avail);
	  // some response data has appeared
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
	  LEAF_NOTIDUMP("Modbus Relay Up", buf, count);
	  ++up_word_count;
	  if (transaction_start_ms > 0) {
	    unsigned long elapsed = millis() - transaction_start_ms;
	    LEAF_NOTICE("Relayed %d bytes from physical bus, transaction lasted %dms", count, (int)elapsed);
	    publish("event/relay_up", String(count)+","+String(elapsed));
	    transaction_start_ms=0;
	  }
	}
	else {
	  // nothing to relay in either direction, leave the loop
	  break;
	}
      }
#ifndef ESP8266
      //yield();
#endif
    }

  }

};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
