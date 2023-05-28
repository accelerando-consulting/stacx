//
// Example 7: a 433MHz ASK radio receiver
// (Eg for radio-controlled outlets, lights and garage doors)
//
#include "leaf_rcrx.h"

Leaf *leaves[] = {
  new RcRxLeaf("rcrx", LEAF_PIN(D1));
	
  NULL
};

