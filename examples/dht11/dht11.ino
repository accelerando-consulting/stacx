
//
// Example 3: a standalone DHT11 temperature sensor
//

#include "leaf_dht11.h"
#include "../examples/app_tempdisplay.h"

Leaf *leaves[] = {
  new Dht11Leaf("livingroom", LEAF_PIN(2)),
  NULL
};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:


