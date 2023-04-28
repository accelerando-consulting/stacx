//
//@**************************** class WireBusLeaf ******************************
//
// This class encapsulates an I2C bus
//
#pragma once

#include "Wire.h"

class WireBusLeaf : public Leaf
{
protected:
  int pin_sda;
  int pin_scl;

public:

  WireBusLeaf(String name, int sda=SDA, int scl=SCL)
    : Leaf("wire", name, LEAF_PIN(sda)|LEAF_PIN(scl))
    , Debuggable(name)
  {
    LEAF_ENTER(L_INFO);
    pin_sda = sda;
    pin_scl = scl;
    do_heartbeat=false;
    LEAF_LEAVE;
  }

  void setup(void) {
    Leaf::setup();
    LEAF_ENTER(L_INFO);
    LEAF_NOTICE("I2C interface using SDA=%d SCL=%d", pin_sda, pin_scl);
    Wire.begin(pin_sda, pin_scl);

    registerLeafCommand(HERE, "scan", "Scan the I2C bus");
    registerLeafCommand(HERE, "read/", "Read <payload> bytes from addr <topic>");
    registerLeafCommand(HERE, "write/", "Write bytes from payload to addr <topic>");
    registerLeafCommand(HERE, "read_reg/", "read <payload> bytes from <topic=addr/reg>");
    registerLeafCommand(HERE, "write_reg/", "write <payload> to device at <topic=addr/reg> interpreting payload as hex bytes");
    
    scan(false);
    LEAF_LEAVE;
  }

  void scan(bool publish=true)
  {
    LEAF_ENTER(L_INFO);

    byte error, address;
    int nDevices;

    LEAF_NOTICE("Scanning I2C...");

    nDevices = 0;
    for(address = 1; address < 127; address++ )
    {
      // The i2c_scanner uses the return value of
      // the Write.endTransmisstion to see if
      // a device did acknowledge to the address.
      Wire.beginTransmission(address);
      error = Wire.endTransmission();

      if (error == 0)
      {
	LEAF_NOTICE("    I2C device detected at address 0x%02x (%d)", address, address);
	if (publish) {
	  mqtt_publish(String("status/i2c_scan/")+String(nDevices,10), "0x"+String(address, 16));
	}
	nDevices++;
      }
      else if (error==4)
      {
	LEAF_NOTICE("Unknown error at I2C address 0x%02x", address);
      }
    }
    if (nDevices == 0) {
      LEAF_NOTICE("No I2C devices found");
    }
    else {
      LEAF_NOTICE("I2C scan complete, found %d devices", nDevices);
    }
    LEAF_LEAVE;
  }

  virtual bool commandHandler(String type, String name, String topic, String payload) 
  {
    LEAF_HANDLER(L_INFO);

    //FIXME: need a leaf prefix handler
    WHEN("wire_scan", {
	LEAF_INFO("process wire_scan");
	scan();
      })
    ELSEWHENPREFIX("wire_read/", {
	LEAF_INFO("process wire_read/+");
	uint8_t buf[65];
	int bytes = payload.toInt();
	int addr;
	if (topic.startsWith("0x")) {
	  addr = strtol(topic.substring(2).c_str(), NULL, 16);
	}
	else {
	  addr = topic.toInt();
	}
	if (bytes > 64) {
	  bytes=64;
	}
	Wire.requestFrom((uint8_t)addr, (uint8_t)bytes);
	for (int b=0; b<bytes;b++) {
	  unsigned long then = millis();
	  while (!Wire.available()) {
	    unsigned long now = millis();
	    if ((now - then) > 1000) {
	      ALERT("Timeout waiting for I2C byte %d of %d\n", b+1, bytes);
	      return true;
	    }
	  }
	  buf[b] = Wire.read();
	}
	DumpHex(L_NOTICE, "I2C read", buf, bytes);
      })
    ELSEWHENPREFIX("wire_write/", {
	LEAF_INFO("process wire_write/+");
	int addr;
	if (topic.startsWith("0x")) {
	  addr = strtol(topic.substring(2).c_str(), NULL, 16);
	}
	else {
	  addr = topic.toInt();
	}
	Wire.beginTransmission(addr);
	for (int b=0; b<payload.length(); b+=2) {
	  int value = strtol(payload.substring(b,b+2).c_str(), NULL, 16);
	  Wire.write(value);
	}
	Wire.endTransmission();
	LEAF_NOTICE("I2C wrote to device 0x%x => hex[%s]", addr, payload);
      })
    ELSEWHENPREFIX("wire_read_reg/", {
	LEAF_INFO("process wire_read_reg/<%s>",topic.c_str());
	int reg;
	int addr;
	int count=1;
	
	int pos = topic.indexOf('/');
	if (pos < 0) {
	  return true;
	}
	String addr_arg = topic.substring(0,pos);
	String reg_arg = topic.substring(pos+1);
	LEAF_INFO("parsed topic words addr_arg=[%s] reg_arg=[%s]", addr_arg.c_str(), reg_arg.c_str())
	
	if (addr_arg.startsWith("0x")) {
	  addr = strtol(addr_arg.substring(2).c_str(), NULL, 16);
	}
	else {
	  addr = addr_arg.toInt();
	}
	if (reg_arg.startsWith("0x")) {
	  reg = strtol(reg_arg.substring(2).c_str(), NULL, 16);
	}
	else {
	  reg = reg_arg.toInt();
	}

	if (payload.startsWith("0x")) {
	  count = strtol(payload.substring(2).c_str(), NULL, 16);
	}
	else {
	  count = payload.toInt();
	}
	


	Wire.beginTransmission(addr);
	Wire.write(reg);
	Wire.endTransmission();
	Wire.requestFrom((int)addr, (int)count);
	char buf[65];
	for (int b=0; (b<count)&&(b<(sizeof(buf)/2));b++) {
	  unsigned long then = millis();
	  while (!Wire.available()) {
	    unsigned long now = millis();
	    if ((now - then) > 1000) {
	      ALERT("Timeout waiting for I2C byte %d of %d\n", b+1, count);
	      Wire.endTransmission(true);
	      return true;
	    }
	  }
	  snprintf(buf+2*b, sizeof(buf)-2*b, "%02x", (int)Wire.read());
	}
	Wire.endTransmission(true);

	LEAF_NOTICE("I2C read of %d byte%s from device 0x%x reg 0x%02x <= %s", count, (count>1)?"s":"", addr, reg, buf);
      })
    ELSEWHENPREFIX("wire_write_reg/", {
	LEAF_INFO("process wire_write_reg/+");
	int addr;
	int reg;
	int value;
	int pos = topic.indexOf('/',0);
	if (pos < 0) {
	  return true;
	}
	String addr_arg = topic.substring(0,pos).c_str();
	String reg_arg = topic.substring(pos).c_str();

	if (addr_arg.startsWith("0x")) {
	  addr = strtol(addr_arg.substring(2).c_str(), NULL, 16);
	}
	else {
	  addr = addr_arg.toInt();
	}
	if (reg_arg.startsWith("0x")) {
	  reg = strtol(reg_arg.substring(2).c_str(), NULL, 16);
	}
	else {
	  reg = reg_arg.toInt();
	}

	Wire.beginTransmission(addr);
	Wire.write(reg);
	for (int b=0; b<payload.length(); b+=2) {
	  int value = strtol(payload.substring(b,b+2).c_str(), NULL, 16);
	  Wire.write(value);
	}
	Wire.endTransmission();
	LEAF_NOTICE("I2C wrote to device 0x%x reg 0x%02x => %02x", addr, reg, payload.c_str());
      })
      LEAF_HANDLER_END;
  }
  

};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
