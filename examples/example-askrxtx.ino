//
// Example 8: ASK receiver and transmitter
//
#include "leaf_rcrx.h"
#include "leaf_rctx.h"

Leaf *leaves[] = {
	new RcRxLeaf("workshop", LEAF_PIN(D7)),
	new RcTxLeaf("workshop", LEAF_PIN(D8)),
	NULL
};

