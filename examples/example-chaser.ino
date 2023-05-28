
//
// Example 6: a NeoPixel chaser
//
#include "leaf_chaser.h"

Leaf *leaves[] = {
  new ChaserLeaf("star", LEAF_PIN(D4), 16, 0x070C0A),
  NULL
};

