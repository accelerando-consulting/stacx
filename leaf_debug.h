#include "leaf_button.h"

class DebugLeaf : public ButtonLeaf
{
public:
  DebugLeaf(String name, pinmask_t pins) : ButtonLeaf(name, pins)
  {
  }
  
  void status_pub() 
  {
    if (button.read()==LOW) {
      debug++;
      if (debug > L_DEBUG) {
	debug = 0;
      }
      LEAF_NOTICE("Incrementing debug level => %d", debug);
    }
    ButtonLeaf::status_pub();
  }
};

	
// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
