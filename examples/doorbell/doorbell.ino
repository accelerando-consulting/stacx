//
// Example 5: A doorbell
//
#include "leaf_ground.h"
#include "leaf_light.h"
#include "leaf_button.h"
#include "leaf_analog.h"

Leaf *leaves[] = {
  new GroundLeaf("doorbell", LEAF_PIN(D2)|LEAF_PIN(D4)),
  new LightLeaf("doorbell", LEAF_PIN(D1),500,20),
  new ButtonLeaf("doorbell", LEAF_PIN(D3)),
  new AnalogInputLeaf("battery", LEAF_PIN(A0), 0, 1023, 0, 4.5, true),
  NULL
};
