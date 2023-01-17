//
//@**************************** class RTCLeaf ******************************
//
// This class encapsulates a DS1307 real time clock
//
#pragma once
#include <Wire.h>
#include <RTClib.h>

#include "trait_wirenode.h"
#include "trait_pollable.h"

class RTCLeaf : public Leaf, public WireNode, public Pollable
{
protected:
  RTC_DS1307 rtc;
  bool rtc_ok = false;
  String timestamp;
	
public:

  RTCLeaf(String name, pinmask_t pins=0, int address=0, TwoWire *bus=&Wire)
    : Leaf("rtc", name, pins)
    , WireNode(address, bus)
    , Pollable(10000, 60)
    , Debuggable(name)
  {
    LEAF_ENTER(L_DEBUG);
    this->do_heartbeat = false;
    LEAF_LEAVE;
  }

  virtual bool poll() 
  {
    timestamp = rtc.now().timestamp();
    //LEAF_NOTICE("clock: %s", timestamp.c_str());
    return true;
  }
  
  virtual void setup(void) {
    LEAF_ENTER(L_INFO);
    Leaf::setup();

    if (! rtc.begin(wire)) {
      LEAF_ALERT("Couldn't find RTC");
      return;
    }

    if (! rtc.isrunning()) {
      ALERT("RTC is NOT running, let's set the time!");
      // When time needs to be set on a new device, or after a power loss, the
      // following line sets the RTC to the date & time this sketch was compiled
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
      // This line sets the RTC with an explicit date & time, for example to set
      // January 21, 2014 at 3am you would call:
      // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
    }
    NOTICE("RTC time is %s\n", rtc.now().timestamp().c_str());
    rtc_ok=true;

    LEAF_LEAVE;
  }

  virtual void status_pub()
  {
    publish("status/clock", rtc.now().timestamp());
  }

  virtual void loop() 
  {
    Leaf::loop();
    pollable_loop();
  }

  virtual bool mqtt_receive(String type, String name, String topic, String payload) {
    LEAF_ENTER(L_INFO);
    bool handled = Leaf::mqtt_receive(type, name, topic, payload);

    if (type == "app") {
      LEAF_NOTICE("RECV %s/%s => [%s <= %s]", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());
    }
    
    do {
    WHEN("get/clock",{status_pub();})
    ELSEWHEN("set/clock",{
	int pos = payload.indexOf(",");
	if (pos < 0) break;
	int year = payload.substring(0,pos).toInt();
	payload.remove(0,pos+1);
	LEAF_NOTICE("Parsed year=%d remain=[%s]", year, payload.c_str());

	pos = payload.indexOf(",");
	if (pos < 0) break;
	int month = payload.substring(0,pos).toInt();
	payload.remove(0,pos+1);
	LEAF_NOTICE("Parsed month=%d remain=[%s]", month, payload.c_str());

	pos = payload.indexOf(",");
	if (pos < 0) break;
	int day = payload.substring(0,pos).toInt();
	payload.remove(0,pos+1);
	LEAF_NOTICE("Parsed day=%d remain=[%s]", day, payload.c_str());
	
	pos = payload.indexOf(",");
	if (pos < 0) break;
	int hour = payload.substring(0,pos).toInt();
	payload.remove(0,pos+1);
	LEAF_NOTICE("Parsed hour=%d remain=[%s]", hour, payload.c_str());
	
	pos = payload.indexOf(",");
	if (pos < 0) break;
	int minute = payload.substring(0,pos).toInt();
	payload.remove(0,pos+1);
	LEAF_NOTICE("Parsed minute=%d remain=[%s]", minute, payload.c_str());

	int second = payload.toInt();
	rtc.adjust(DateTime(year, month, day, hour, minute,second));
	NOTICE("RTC time set to %s\n", rtc.now().timestamp().c_str());
});
    } while (0);
    LEAF_RETURN(handled);
  }

};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
