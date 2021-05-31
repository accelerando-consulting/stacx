//
//@**************************** class Si7210Leaf ******************************
// 
// This class encapsulates a Si7210 i2c hall-effect sensor
// 
#include <Wire.h>

class WireBusLeaf : public Leaf
{
protected:
  int pin_sda;
  int pin_scl;
  
public:

  WireBusLeaf(String name, int sda, int scl) : Leaf("wire", name, LEAF_PIN(sda)|LEAF_PIN(scl)) {
    LEAF_ENTER(L_INFO);
    pin_sda = sda;
    pin_scl = scl;
    do_heartbeat=false;
    LEAF_LEAVE;
  }

  void setup(void) {
    Leaf::setup();
    LEAF_ENTER(L_NOTICE);
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
	LEAF_NOTICE("I2C device detected at address 0x%02x", address);
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
  

};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
