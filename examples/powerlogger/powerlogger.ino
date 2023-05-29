//
// Example 10: a power usage data logger
//
#include "leaf_sdcard.h"
#include "leaf_analog_rms.h"
#include "leaf_debug.h"

Leaf *leaves[] = {
  new SDCardLeaf("sdcard"),
  new AnalogRMSLeaf("dcbus", LEAF_PIN(35), 0, 4095, 0, 495),
  new AnalogRMSLeaf("acamps", LEAF_PIN(33), 0, 4095, 0, 16.5),
  new AnalogRMSLeaf("acvolts", LEAF_PIN(34), 0, 4095, 0, 100),
  new DebugLeaf("debug", LEAF_PIN(0)),
  NULL
};

