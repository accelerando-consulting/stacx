//
// Example 1: An entry door controller with PIR, Light, Latch and weather
//


#include "leaf_motion.h"
#include "leaf_lock.h"
#include "leaf_light.h"
#include "leaf_contact.h"

Leaf *leaves[] = {
  new MotionLeaf("entry", LEAF_PIN(D8)),
  new MotionLeaf("porch", LEAF_PIN(D7)),
  new LockLeaf("entry", LEAF_PIN(D6), false, true),
  new LightLeaf("entry", LEAF_PIN(D5)),
  new ContactLeaf("entry", LEAF_PIN(D2)),
  NULL
};

//
// Example 2: an LED room light
//

#include "leaf_light.h"

Leaf *leaves[] = {
  new LightLeaf("bedhead", LEAF_PIN(0)),
  NULL
};

//
// Example 3: a standalone DHT11 temperature sensor
//

#include "leaf_temp_abstract.h"
#include "leaf_dht11.h"					

Leaf *leaves[] = {
  new Dht11Leaf("livingroom", LEAF_PIN(2)),
  NULL
};

  
//
// Example 4: A garden water controller (relay shield)
//
#include "leaf_outlet.h"

Leaf *leaves[] = {
  new MotionLeaf("outlet", LEAF_PIN(D1)),
  NULL
};

//
// Example 4: A DHT12 temperature/humidity sensor, with battery monitor
//
#include "leaf_temp_abstract.h"
#include "leaf_dht12.h"				
#include "leaf_analog.h"

Leaf *leaves[] = {
  new Dht12Leaf("outside", LEAF_PIN(D1)|LEAF_PIN(D2)),
  new AnalogInputLeaf("battery", LEAF_PIN(A0), 0, 1023, 0, 4.5, true),
  NULL
};

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

//
// Example 6: a NeoPixel chaser
//
#include "leaf_chaser.h"

Leaf *leaves[] = {
  new ChaserLeaf("star", LEAF_PIN(D4), 16, 0x070C0A),
  NULL
};

//
// Example 7: a 433MHz ASK radio receiver
// (Eg for radio-controlled outlets, lights and garage doors)
//
#include "leaf_rcrx.h"

Leaf *leaves[] = {
  new RcRxLeaf("rcrx", LEAF_PIN(D1));
	
  NULL
};

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


// local Variables:
// mode: C++
// c-basic-offset: 2
// End:

