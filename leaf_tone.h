//@***************************** class LightLeaf ******************************

#ifndef ESP8266
#include "ESP32Tone.h"
#include <Ticker.h>
#endif

class ToneLeaf : public Leaf
{
public:
  bool state;
  int tonePin = -1;
  int freq;
  int duration;
  Ticker spkrOffTimer;

  ToneLeaf(String name, pinmask_t pins, int freq = 440, int duration=1000) : Leaf("tone", name, pins){
    state = false;
    this->freq = freq;
    this->duration = duration;
  }

  virtual void setup(void) {
    Leaf::setup();
    FOR_PINS({tonePin=pin;});
    enable_pins_for_output();
  }

  virtual void mqtt_do_subscribe() {
    LEAF_ENTER(L_NOTICE);
    Leaf::mqtt_do_subscribe();
    mqtt_subscribe("cmd/tone");
    LEAF_LEAVE;
  }

#ifndef ESP8266
  void stopTone() 
  {
    noTone(tonePin);
    INFO("Silenced tone on pin %d", tonePin);
  }
#endif  
    

  virtual bool mqtt_receive(String type, String name, String topic, String payload) {
    LEAF_ENTER(L_DEBUG);
    bool handled = Leaf::mqtt_receive(type, name, topic, payload);

    WHEN("cmd/tone",{
      int pin = tonePin;
      int len = payload.toInt();
      if (len == 0) len = duration;
      LEAF_INFO("Playing %dms tone on pin %d", len, pin);
#ifdef ESP8266
      analogWriteFreq(freq);
      analogWrite(pin, 128);
#else
      tone(pin, freq);
#endif
      spkrOffTimer.once_ms(len, [](){
#ifdef ESP8266
			     analogWrite(pin, 0);
			     INFO("Silenced tone on pin %d", pin);
#else
			     ToneLeaf *that = (ToneLeaf *)Leaf::get_leaf_by_type(leaves, String("tone"));
			     if (that) {
			       that->stopTone();
			     }
			     else {
			       ALERT("Can't find tone leaf to silence");
			     }
#endif
			   });
			     
      //delay(5000);
      //noTone(tonePin);
      //tone(tonePin, freq, duration);
      INFO("Tone playing in background");		
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

    LEAF_LEAVE;
    return handled;
  };



};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
