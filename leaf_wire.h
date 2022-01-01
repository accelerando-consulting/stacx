//
//@**************************** class WireBusLeaf ******************************
// 
// This class encapsulates an i2c bus
// 
#pragma once

#include <Wire.h>

class WireBusLeaf : public Leaf
{
protected:
  int pin_sda;
  int pin_scl;
  
public:

  WireBusLeaf(String name, int sda=SDA, int scl=SCL) : Leaf("wire", name, LEAF_PIN(sda)|LEAF_PIN(scl)) {
    LEAF_ENTER(L_INFO);
    pin_sda = sda;
    pin_scl = scl;
    do_heartbeat=false;
    LEAF_LEAVE;
  }

  void setup(void) {
    Leaf::setup();
    LEAF_ENTER(L_INFO);
    LEAF_NOTICE("Wire interface using SDA=%d SCL=%d", pin_sda, pin_scl);
    Wire.begin(pin_sda, pin_scl);
    scan();
    LEAF_LEAVE;
  }

  void scan(void) 
  {
    LEAF_ENTER(L_INFO);
    
    byte error, address;
    int nDevices;

    LEAF_NOTICE("Scanning...");

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
	LEAF_NOTICE("    I2C device detected at address 0x%02x", address);
	nDevices++;
      }
      else if (error==4) 
      {
	LEAF_NOTICE("Unknown error at address 0x%02x", address);
      }    
    }
    if (nDevices == 0) {
      LEAF_NOTICE("No I2C devices found");
    }
    else {
      LEAF_NOTICE("Scan complete, found %d devices", nDevices);
    }
    LEAF_LEAVE;
  }
  
  bool mqtt_receive(String type, String name, String topic, String payload) {
    LEAF_ENTER(L_INFO);
    bool handled = false;

    WHEN("i2c_scan", {
	scan();
	handled = true;
      })
    ELSEWHEN("i2c_read", {
/*
	int pos,addr;
	char buf[65];
	pos = topic.indexOf('/',0);
	if (pos < 0) return;
	String arg = topic.substring(pos).c_str();
	addr = strtol(arg.c_str(), NULL, 16);
	int bytes = payload.toInt();
	if (bytes > 64) bytes=64;
	wire->requestFrom((uint8_t)addr, (uint8_t)bytes);
	for (int b=0; b<bytes;b++) {
	  while (!wire->available()) {
	    unsigned long now = millis();
	    if ((now - start) > 1000) {
	      ALERT("Timeout waiting for I2C byte %d of %d\n", b+1, bytes);
	      return true;
	    }
	  }
	  buf[b] = wire->read();
	}
	DumpHex(L_NOTICE, "i2c read", buf, bytes);
	handled = true;
*/
      })
    ELSEWHEN("i2c_write", {
/*
	int pos,addr,val;
	pos = topic.indexOf('/',0);
	if (pos < 0) return true;
	String arg = topic.substring(pos).c_str();
	addr = strtol(arg.c_str(), NULL, 16);

	wire->beginTransmission(addr);
	wire->write(reg);

	for (int b=0; b<payload.length(); b+=2) {
	  value = strol(payload.substring(b,b+2).c_str(), NULL, 16);
	  wire->write(value);
	}
	wire->endTransmission();
	NOTICE("I2C wrote to device 0x%x => hex[%s]", addr, payload);
*/
      })
    ELSEWHEN("i2c_read_reg", {
/*
	int pos,addr,reg,val;
	pos = topic.indexOf('/',0);
	if (pos < 0) return true;
	String arg = topic.substring(pos).c_str();
	if (arg.startsWith("0x")) {
	  addr = strtol(arg.substring(2).c_str(), NULL, 16);
	}
	else {
	  addr = arg.toInt();
	}
	if (payload.startsWith("0x")) {
	  reg = strtol(payload.substring(2).c_str(), NULL, 16);
	}
	else {
	  reg = payload.toInt();
	}
	wire->beginTransmission(addr);
	wire->write(reg);
	wire->endTransmission();
	wire->requestFrom((int)add, (int)1);
	while (!wire->available()) {
	  unsigned long now = millis();
	  if ((now - start) > 1000) return -1;
	}
	value = wire->read();
	NOTICE("I2C read from device 0x%x reg 0x%02x <= %02x", addr, reg, value);
*/
      })
    ELSEWHEN("i2c_write_reg", {
/*
	int pos,addr,reg,val;
	pos = topic.indexOf('/',0);
	if (pos < 0) return true;
	String arg = topic.substring(pos).c_str();
	if (arg.startsWith("0x")) {
	  addr = strtol(arg.substring(2).c_str(), NULL, 16);
	}
	else {
	  addr = arg.toInt();
	}
	pos = payload.indexOf('/',0);
	if (pos < 0) return true;
	arg = payload.substring(0,pos);
	if (arg.startsWith("0x")) {
	  reg = strtol(arg.substring(2).c_str(), NULL, 16);
	}
	else {
	  reg = arg.toInt();
	}
	arg = payload.substring(pos+1)
	if (arg.startsWith("0x")) {
	  value = strtol(arg.substring(2).c_str(), NULL, 16);
	}
	else {
	  value = arg.toInt();
	}
	wire->beginTransmission(addr);
	wire->write(reg);
	wire->write(value);
	wire->endTransmission();
	while (!wire->available()) {
	  unsigned long now = millis();
	  if ((now - start) > 1000) return -1;
	}
	value = wire->read();
	NOTICE("I2C wrote to device 0x%x reg 0x%02x => %02x", addr, reg, value);
*/
      })
      ;
    return handled;
    
  }
};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End: