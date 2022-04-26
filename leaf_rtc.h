//
//@**************************** class RTCLeaf ******************************
//
// This class encapsulates a DS1307 real time clock
//
#pragma once

#include <Wire.h>
#include <RTClib.h>

class RTCLeaf : public Leaf, public WireNode, public Pollable
{
protected:
  RTC_DS1307 rtc;
  bool rtc_ok = false;
  String timestmap;
	
public:

  RTCLeaf(String name, pinmask_t pins=0, int address=0, TwoWire *bus=NULL) : Leaf("rtc", name, pins) {
    LEAF_ENTER(L_DEBUG);
    this->do_heartbeat = false;
    if (bus == NULL) {
      bus = &Wire;
    }
    setWireBus(bus);
    this->sample_interval_ms = 10000;
    this->report_interval_sec = 60;

    LEAF_LEAVE;
  }

  bool poll() 
  {
    timestamp = rtc.now().timestamp();
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
    //LEAF_NOTICE("count=%lu", count);
    //mqtt_publish("status/count", String(count));
    publish("status/clock", rtc.now().timestamp().c_str());
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
    
    WHEN("get/clock",{status_pub();});
    LEAF_RETURN(handled);
  }

};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
