//@***************************** class LightLeaf ******************************

#ifndef ESP8266
#include "ESP32Tone.h"
#include <Ticker.h>
#endif


class ToneLeaf;

class ToneLeaf *toneStopContext;

class ToneLeaf : public Leaf
{
public:
  bool state;
  int tonePin = -1;
  int freq;
  int duration;
  Ticker spkrOffTimer;
  bool do_test = false;

  ToneLeaf(String name, pinmask_t pins, int freq = 440, int duration=100, bool do_test=false) : Leaf("tone", name, pins){
    state = false;
    this->freq = freq;
    this->duration = duration;
    this->do_test = do_test;
    
  }

  virtual void setup(void) {
    Leaf::setup();
    FOR_PINS({tonePin=pin;});
    enable_pins_for_output();
    LEAF_NOTICE("%s claims pin %d as Speaker", base_topic.c_str(), tonePin);
  }

    
  virtual void mqtt_do_subscribe() {
    Leaf::mqtt_do_subscribe();
    mqtt_subscribe("cmd/tone");
  }

#ifndef ESP8266
  void stopTone() 
  {
    noTone(tonePin);
    DEBUG("Silenced tone on pin %d", tonePin);
  }
#endif  

  void playTone(int pitch, int len) 
  {
    int pin = tonePin;
    if (len <= 1) len = duration;
    if (pitch <= 1) pitch=freq;

    LEAF_INFO("Playing %dHz tone for %dms on pin %d", pitch, len, pin);
#ifdef ESP8266
      analogWriteFreq(pitch);
      analogWrite(pin, 128);
#else
      tone(pin, pitch);
      toneStopContext = this;
#endif
      spkrOffTimer.once_ms(len, [](){
#ifdef ESP8266
			     analogWrite(pin, 0);
			     DEBUG("Silenced Tone on pin %d", pin);
#else
			     if (toneStopContext) {
			       toneStopContext->stopTone();
			       toneStopContext = NULL;
			     }
#endif
      });
  }

  virtual void start(void) 
  {
    Leaf::start();
    if (do_test) {
      LEAF_NOTICE("Test speaker");
      playTone(0,200);
    }
  }

  virtual bool mqtt_receive(String type, String name, String topic, String payload) {
    LEAF_ENTER(L_DEBUG);
    bool handled = Leaf::mqtt_receive(type, name, topic, payload);

    if ((type=="app")||(type=="shell")) {
      LEAF_INFO("RECV %s/%s => [%s <= %s]", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());
    }

    WHEN("cmd/tone",{
	int freq = 0;
	int duration = 0;
	int comma = payload.indexOf(",");
	if (comma) {
	  freq = payload.toInt();
	  duration = payload.substring(comma+1).toInt();
	}
	playTone(freq, duration);
	LEAF_INFO("Tone playing in background");		
    })
    ELSEWHEN("set/freq",{
      LEAF_INFO("Updating freq via set operation");
      freq = payload.toInt();
      status_pub();
    })
    ELSEWHEN("set/duration",{
      LEAF_INFO("Updating duration via set operation");
      duration = payload.toInt();
      status_pub();
    })
    else {
      if ((type=="app")||(type=="shell")) {
	LEAF_INFO("Did not handle %s", topic.c_str());
      }
    }

    LEAF_LEAVE;
    return handled;
  };



};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
