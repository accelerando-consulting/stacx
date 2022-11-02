//@***************************** class LightLeaf ******************************

#ifndef ESP8266
#include "ESP32PWM.h"
#include <Ticker.h>
#endif

class PWMLeaf : public Leaf
{
public:
  bool state=false;
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
    pinMode(pwmPin, OUTPUT);
    digitalWrite(pwmPin, LOW);
    state=false;
  }

  void startPWM()
  {
    if (pwmPin < 0) return;
    LEAF_NOTICE("startPWM (pwmPin %d freq=%d duty=%.3f duration=%d)", (int)pwmPin, frequency, duty, duration);
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
	LEAF_NOTICE("set timer for auto stop in %dms", duration);
      }
    }
  }

  virtual void setup(void) {
    Leaf::setup();
    FOR_PINS({pwmPin=pin;});
    enable_pins_for_output();
    digitalWrite(pwmPin, 0);
    chan = pwmFactory(pwmPin);
    if (chan == NULL) {
      chan = new ESP32PWM();
    }
    LEAF_NOTICE("%s claims pin %d as PWM", describe().c_str(), (int)pwmPin);
  }

  virtual void mqtt_do_subscribe() {
    Leaf::mqtt_do_subscribe();
    register_mqtt_cmd("on", "Enable the PWM output", HERE);
    register_mqtt_cmd("off", "Disable the PWM output", HERE);
    register_mqtt_cmd("toggle", "Toggle the PWM output", HERE);
    register_mqtt_cmd("test", "Output a square wave using GPIO and a tight loop", HERE);
    register_mqtt_value("duration", "Time (in ms) to run the PWM output when started (0=indefinite)", ACL_SET_ONLY, HERE);
    register_mqtt_value("duration", "Time (in ms) to run the PWM output when started (0=indefinite)", ACL_SET_ONLY, HERE);
    register_mqtt_value("freq", "PWM frequency (in Hz)", ACL_SET_ONLY, HERE);
    register_mqtt_value("duty", "PWM duty cycle (in [0.0,1.0])", ACL_SET_ONLY, HERE);
    register_mqtt_value("state", "PWM output state (1=on/0=off)", ACL_SET_ONLY, HERE);
  }

  void pwm_test(int secs)
  {
    LEAF_ENTER_INT(L_NOTICE, secs);
    bool restart=false;

    if (state) {
      stopPWM();
      restart=true;
    }

    pinMode(pwmPin, OUTPUT);

    unsigned long start=millis();
    unsigned long stop = start + ( 1000 * secs);
    int cycle = 500000/frequency;
    unsigned long pulses = 0;

    do {
      digitalWrite(pwmPin, HIGH);
      delayMicroseconds(cycle);
      digitalWrite(pwmPin, LOW);
      delayMicroseconds(cycle);
      ++pulses;
    } while (millis() < stop);
    LEAF_NOTICE("Tested for %lu sec, did %dlu pulses (%dus/pulse)", secs, pulses, 2*cycle);

    if (restart) {
      startPWM();
    }

    LEAF_LEAVE;
  }


  virtual void status_pub()
  {
    LEAF_ENTER_INT(L_INFO, state?1:0);

    DynamicJsonDocument doc(256);
    JsonObject obj = doc.to<JsonObject>();
    obj["state"]=state;
    obj["pwmPin"]=pwmPin;
    obj["frequency"]=frequency;
    obj["duration"]=duration;
    obj["duty"]=duty;
    char msg[256];
    serializeJson(doc, msg, sizeof(msg)-2);
    mqtt_publish(String("status/")+leaf_name, msg);
    LEAF_LEAVE;
  }

  virtual bool mqtt_receive(String type, String name, String topic, String payload) {
    LEAF_ENTER(L_DEBUG);
    bool handled = Leaf::mqtt_receive(type, name, topic, payload);

    if ((type == "app") || (type=="shell")) {
      LEAF_INFO("RECV %s/%s => [%s <= %s]", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());
    }

    WHEN("cmd/on",{
      startPWM();
      status_pub();
    })
    ELSEWHEN("cmd/off",{
      stopPWM();
      status_pub();
    })
    ELSEWHEN("cmd/toggle",{
      if (state) {
	stopPWM();
      }
      else {
	startPWM();
      }
      status_pub();
    })
    ELSEWHEN("cmd/test",{
      pwm_test(payload.toInt());
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
	if (pwmPin >= 0) {
	  pinMode(pwmPin, OUTPUT);
	  digitalWrite(pwmPin, 1);
	}
	idling = true;
      }
      else if (duty <= 0) {
	// special case for 0%
	stopPWM();
	if (pwmPin >= 0) {
	      pinMode(pwmPin, OUTPUT);
	      digitalWrite(pwmPin, 0);
	}
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
