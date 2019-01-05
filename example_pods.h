//
// Example 1: An entry door controller with PIR, Light, Latch and weather
//

#include "pod_motion.h"
#include "pod_lock.h"
#include "pod_light.h"
#include "pod_contact.h"
#include "pod_temp_abstract.h"
#include "pod_dht12.h"					

Pod *pods[] = {
  new MotionPod("inside", POD_PIN(D7)),
  new MotionPod("outside", POD_PIN(D8)),
  new LockPod("door", POD_PIN(D6)),
  new LightPod("entry", POD_PIN(D5)),
  new ContactPod("door", POD_PIN(D4)),
  new Dht12Pod("entry", POD_PIN(D1)|POD_PIN(D2))
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
  new Dht11Pod("bedroom", POD_PIN(2)),
  NULL
};

  
// local Variables:
// mode: C++
// c-basic-offset: 2
// End:

