#pragma once

//
//@*********************** class IpNullLeaf *************************
//
// This class encapsulates a pseudo-IP service for devices that do
// not communicate besides locally
//

class IpNullLeaf : public AbstractIpLeaf
{
public:
  IpNullLeaf(String name="nullip", String target="")
    : AbstractIpLeaf(name,target)
    , Debuggable(name, L_WARN)
  {
	}

  virtual void setup() {
	  AbstractIpLeaf::setup();
	  LEAF_NOTICE("NULL IP - local comms only");
	  ip_connected=true;
  }

  virtual bool ipConnect(String reason) 
  {
    AbstractIpLeaf::ipConnect(reason);
    ipOnConnect();
    return true;
  }

  virtual bool ipDisconnect() 
  {
    AbstractIpLeaf::ipDisconnect();
    ipOnDisconnect();
    return true;
  }
};

// Local Variables:
// mode: C++
// c-basic-offset: 2
// End:
