//
//@**************************** class DHTLeaf ******************************
// 
// This class encapsulates a TCP/IP interface such as the ESP32's built in
// wifi, or that provided by a cellular modem
// 
#pragma once

class AbstractIpLeaf : public Leaf
{
public:
 
  AbstractIpLeaf(String name, pinmask_t pins=NO_PINS) : Leaf("ip", name, pins) {
    LEAF_ENTER(L_INFO);

    LEAF_LEAVE;
  }

  virtual void setup(void) {
    Leaf::setup();
  }
  
  virtual void loop(void) {
    Leaf::loop();
    //LEAF_ENTER(L_INFO);
    
    //LEAF_LEAVE;
  }
};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
