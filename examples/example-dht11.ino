
//
// Example 3: a standalone DHT11 temperature sensor
//

#include "leaf_temp_abstract.h"
#include "leaf_dht11.h"					

Leaf *leaves[] = {
  new Dht11Leaf("livingroom", LEAF_PIN(2)),
  NULL
};
