//
// Example 1: An entry door controller with PIR, Light, Latch and weather
//


#include "pod_motion.h"
#include "pod_lock.h"
#include "pod_light.h"
#include "pod_contact.h"

Pod *pods[] = {
  new MotionPod("entry", POD_PIN(D8)),
  new MotionPod("porch", POD_PIN(D7)),
  new LockPod("entry", POD_PIN(D6), false, true),
  new LightPod("entry", POD_PIN(D5)),
  new ContactPod("entry", POD_PIN(D2)),
  NULL
};

//
// Example 2: an LED room light
//

#include "pod_light.h"

Pod *pods[] = {
  new LightPod("bedhead", POD_PIN(0)),
  NULL
};

//
// Example 3: a standalone DHT11 temperature sensor
//

#include "pod_temp_abstract.h"
#include "pod_dht11.h"					

Pod *pods[] = {
  new Dht11Pod("livingroom", POD_PIN(2)),
  NULL
};

  
//
// Example 4: A garden water controller (relay shield)
//
#include "pod_outlet.h"

Pod *pods[] = {
  new MotionPod("outlet", POD_PIN(D1)),
  NULL
};

//
// Example 4: A DHT12 temperature/humidity sensor
//
#include "pod_temp_abstract.h"
#include "pod_dht12.h"					

Pod *pods[] = {
  new Dht12Pod("outside", POD_PIN(D1)|POD_PIN(D2)),
  NULL
};



// local Variables:
// mode: C++
// c-basic-offset: 2
// End:

