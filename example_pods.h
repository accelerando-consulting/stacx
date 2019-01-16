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
// Example 4: A DHT12 temperature/humidity sensor, with battery monitor
//
#include "pod_temp_abstract.h"
#include "pod_dht12.h"				
#include "pod_analog.h"

Pod *pods[] = {
  new Dht12Pod("outside", POD_PIN(D1)|POD_PIN(D2)),
  new AnalogInputPod("battery", POD_PIN(A0), 0, 1023, 0, 4.5, true),
  NULL
};

//
// Example 5: A doorbell
//
#include "pod_ground.h"
#include "pod_light.h"
#include "pod_button.h"
#include "pod_analog.h"

Pod *pods[] = {
  new GroundPod("doorbell", POD_PIN(D2)|POD_PIN(D4)),
  new LightPod("doorbell", POD_PIN(D1),500,20),
  new ButtonPod("doorbell", POD_PIN(D3)),
  new AnalogInputPod("battery", POD_PIN(A0), 0, 1023, 0, 4.5, true),
  NULL
};



// local Variables:
// mode: C++
// c-basic-offset: 2
// End:

