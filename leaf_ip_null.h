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
  IpNullLeaf(String name, String target)
    : AbstractIpLeaf(name,target)
    , TraitDebuggable(name, L_WARN)
  {
	}

  virtual void setup() {
	  AbstractIpLeaf::setup();
	  LEAF_NOTICE("NULL IP - local comms only");
	  ip_connected=true;
  }
		
};

// Local Variables:
// mode: C++
// c-basic-offset: 2
// End:
