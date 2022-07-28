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
  int unit;
  Stream *port;
  Modbus *bus = NULL;
  String target;
  StorageLeaf *registers = NULL;

public:
  ModbusSlaveLeaf(String name,
		  String target,
		  //ModbusReadRange **readRanges,
		  int unit=1,
		  Stream *port=NULL,
		  int uart=1
    ): Leaf("modbusSlave", name, NO_PINS) {
    LEAF_ENTER(L_INFO);
    //this->readRanges = new SimpleMap<String,ModbusReadRange*>(_compareStringKeys);
    //for (int i=0; readRanges[i]; i++) {
    //  this->readRanges->put(readRanges[i]->name, readRanges[i]);
    //}
    this->unit = unit;
    this->target=target;
    if (port == NULL) {
      this->port = new HardwareSerial(uart);
    }
    else {
      this->port = port;
    }
    this->bus = new Modbus(*port, unit);

    LEAF_LEAVE;
  }

  void onCallback(uint8_t code, uint16_t addr, uint16_t len) 
  {
    NOTICE("modbus callback 0x%02x:0x%x,%u", (unsigned int)code, (unsigned int)addr, (unsigned int)len);
  }

  int getRegister(int code, int addr) 
  {
    String key = String(code)+":"+String(addr,HEX);
    int result = registers->getInt(key);
    //LEAF_NOTICE("getRegister %s <= 0x%04x", key.c_str(), result);
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
    LEAF_INFO("putRegister %s => 0x%04x", key.c_str(), value);
    registers->putInt(key, value);
  }

  int readBits(int code, int addr, int len) 
  {
    if (!hasRegister(code, addr)) return STATUS_ILLEGAL_DATA_ADDRESS;
    for (int i=0; i<len; i++) {
      int bit = getRegister(code, addr+i);
      LEAF_NOTICE("readBits %02x:%04x <= %d", code, addr+i, bit);
      bus->writeCoilToBuffer(i, bit);
    }
    return STATUS_OK;
  }

  int readWords(int code, int addr, int len) 
  {
    if (!hasRegister(code, addr)) return STATUS_ILLEGAL_DATA_ADDRESS;
    for (int i=0; i<len; i++) {
      int word = getRegister(code, addr+i);
      LEAF_NOTICE("readWords %02x:%04x <= 0x%04x", code, addr+i, word);
      bus->writeRegisterToBuffer(i, word);
    }
    return STATUS_OK;
  }

  int writeBits(int code, int addr, int len) 
  {
    for (int i=0; i<len; i++) {
      int bit = bus->readCoilFromBuffer(i);
      LEAF_NOTICE("writeBits %02x:%04x <= %d", code, addr+i, bit);
      putRegister(code,addr+i,bit);
    }
    return STATUS_OK;
  }

  int writeWords(int code, int addr, int len) 
  {
    for (int i=0; i<len; i++) {
      int word = bus->readRegisterFromBuffer(i);
      LEAF_NOTICE("writeWords %02x:%04x <= %d", code, addr+i, word);
      putRegister(code,addr+i,word);
    }
    return STATUS_OK;
  }
  
  void setup(void) {
    Leaf::setup();
    LEAF_ENTER(L_INFO);
    this->install_taps(target);
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
    bus->begin(9600);
    registers = (StorageLeaf *)get_tap("registers");
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
