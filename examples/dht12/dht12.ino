//
// Example 4: A DHT12 temperature/humidity sensor, with battery monitor
//
#include "leaf_dht12.h"				
#include "leaf_analog.h"
#include "../common/app_tempdisplay.h"

Leaf *leaves[] = {
  new Dht12Leaf("outside", LEAF_PIN(D1)|LEAF_PIN(D2)),
  new AnalogInputLeaf("battery", LEAF_PIN(A0), 0, 1023, 0, 4.5, true),
  NULL
};
// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
