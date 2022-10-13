//@***************************** class LightLeaf ******************************

#ifndef ESP8266
#include "ESP32PWM.h"
#include <Ticker.h>
#endif

class PWMLeaf : public Leaf
{
public:
  bool state;
  bool idling = false;
  int pwmPin = -1;
  int frequency;
  int duration;
  float duty;
  ESP32PWM *chan;
  Ticker pwmOffTimer;

  PWMLeaf(String name, pinmask_t pins, int freq = 3500, int duration=0,float duty=0.5) : Leaf("pwm", name, pins){
    state = false;
    this->frequency = freq;
    this->duration = duration;
    this->duty = duty;
  }

  void stopPWM() 
  {
    LEAF_NOTICE("stopPWM (pwmPin %d)", (int)pwmPin);
    if (pwmPin < 0) return;
    if (chan != NULL) {
      if(chan->attached())
      {
	chan->detachPin(pwmPin);
      }
    }
    state=false;
  }

  void startPWM() 
  {
    LEAF_NOTICE("startPWM (pwmPin %d freq=%d duty=%.3f duration=%d)", (int)pwmPin, frequency, duty, duration);
    if (pwmPin < 0) return;
    if (chan) {
      if (!chan->attached()) {
	chan->attachPin(pwmPin,frequency, 10); // This adds the PWM instance
					     // to the factory list
      }
      chan->writeScaled(duty);
      chan->writeTone(frequency);
      state=true;
      if (duration) {
	pwmOffTimer.once_ms(duration, [](){
	  PWMLeaf *that = (PWMLeaf *)Leaf::get_leaf_by_type(leaves, String("pwm"));
	  if (that) {
	    that->stopPWM();
	  }
	  else {
	    ALERT("Can't find PWM leaf to stop");
	  }
	});
      }
    }
  }
  
  virtual void setup(void) {
    Leaf::setup();
    FOR_PINS({pwmPin=pin;});
    enable_pins_for_output();
    chan = pwmFactory(pwmPin);
    if (chan == NULL) {
      chan = new ESP32PWM();
    }
    LEAF_NOTICE("%s claims pin %d as PWM", base_topic.c_str(), (int)pwmPin);
  }

  virtual void mqtt_do_subscribe() {
    Leaf::mqtt_do_subscribe();
    mqtt_subscribe("cmd/start-pwm", HERE);
    mqtt_subscribe("cmd/stop-pwm", HERE);
  }

  virtual void status_pub() 
  {
    DynamicJsonDocument doc(256);
    JsonObject obj = doc.to<JsonObject>();
    obj["pwmPin"]=pwmPin;
    obj["frequency"]=frequency;
    obj["duration"]=duration;
    obj["duty"]=duty;
    char msg[256];
    serializeJson(doc, msg, sizeof(msg)-2);
    mqtt_publish(String("status/")+leaf_name, msg);
  }
  
  virtual bool mqtt_receive(String type, String name, String topic, String payload) {
    LEAF_ENTER(L_DEBUG);
    bool handled = Leaf::mqtt_receive(type, name, topic, payload);

    if (type == "app") {
      LEAF_NOTICE("RECV %s/%s => [%s <= %s]", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());
    }

    WHEN("cmd/on",{
	startPWM();
      })
    ELSEWHEN("cmd/off",{
	stopPWM();
      })
    ELSEWHEN("cmd/toggle",{
	if (state) {
	  stopPWM();
	}
	else {
	  startPWM();
	}
      })
    ELSEWHEN("set/freq",{
      LEAF_INFO("Updating freq via set operation");
      frequency = payload.toInt();
      status_pub();
      if (state && chan) chan->writeTone(frequency);
    })
    ELSEWHEN("set/duration",{
      LEAF_INFO("Updating duration via set operation");
      duration = payload.toInt();
      startPWM();
      status_pub();
    })
    ELSEWHEN("set/duty",{
      LEAF_INFO("Updating duty cycle via set operation");
      duty = payload.toFloat();
      if (duty > 1) {
	// special case for 100%
	stopPWM();
	if (pwmPin >= 0) digitalWrite(pwmPin, 1);
	idling = true;
      }
      else if (duty <= 0) {
	// special case for 0%
	stopPWM();
	if (pwmPin >= 0) digitalWrite(pwmPin, 0);
	idling = true;
      }
      else {
	if (idling) {
	  // We were not doing PWM because "faking" 0% or 100%, start up again
	  idling = false;
	  startPWM();
	}
	else if (state && chan) {
	  // We are running PWM already, just change the duty cycle
	  LEAF_NOTICE("Update duty cycle to %.3f", duty);
	  chan->writeScaled(duty);
	}
      }
      status_pub();
      })
    ELSEWHEN("set/state",{
      if (payload.toInt()) {
	startPWM();
      }
      else {
	stopPWM();
      }
      status_pub();
    })
    else {
      if (type == "app") {
	LEAF_NOTICE("Message not handled");
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
