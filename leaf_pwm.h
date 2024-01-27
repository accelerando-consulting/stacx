//@***************************** class LightLeaf ******************************

#ifdef ESP8266
#include "ESP8266_PWM.h"

extern "C" {
  void IRAM_ATTR Pwm8266TimerHandler();
  bool pwm8266Run=false;
  int pwm8266IntervalUsec = 20;
  ESP8266Timer pwm8266Timer;
  ESP8266_PWM pwm8266;
}

#endif

#ifdef ESP32
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
#ifdef ESP32
  ESP32PWM *chan=NULL;
  Ticker pwmOffTimer;
#endif

  PWMLeaf(String name, pinmask_t pins, int freq = 3000, int duration=0,float duty=0.5)
    : Leaf("pwm", name, pins)
    , Debuggable(name)
  {
    state = false;
    this->frequency = freq;
    this->duration = duration;
    this->duty = duty;
  }

  void stopPWM()
  {
    LEAF_NOTICE("stopPWM (pwmPin %d)", (int)pwmPin);
    if (pwmPin < 0) return;
#ifdef ESP32
    if (chan != NULL) {
      if(chan->attached())
      {
	chan->detachPin(pwmPin);
      }
    }
#endif
    
    pinMode(pwmPin, OUTPUT);
    digitalWrite(pwmPin, LOW);
    state=false;
  }

  void startPWM()
  {
    if (pwmPin < 0) return;
    LEAF_NOTICE("startPWM (pwmPin %d freq=%d duty=%.3f duration=%d)", (int)pwmPin, frequency, duty, duration);

#ifdef ESP8266
    if (!pwm8266Run) {
      pwm8266Timer.attachInterruptInterval(pwm8266IntervalUsec, Pwm8266TimerHandler);
      pwm8266Run = true;
    }
    pwm8266.setPWM(pwmPin, frequency, duty*100); // 8266 uses percent
#endif
#ifdef ESP32
    if (chan) {
      if (!chan->attached()) {
	chan->attachPin(pwmPin,frequency, 10); // This adds the PWM instance
					     // to the factory list
      }
      chan->writeScaled(duty);
      chan->writeTone(frequency);
    }
#endif
    state=true;
      
    if (duration) {
#ifdef ESP32
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
#else
      ALERT("Duration not supported for esp8266");
#endif
    }
  }

  virtual void setup(void) {
    Leaf::setup();
    FOR_PINS({pwmPin=pin;});
    enable_pins_for_output();
    digitalWrite(pwmPin, 0);
#ifdef ESP32
    chan = pwmFactory(pwmPin);
    if (chan == NULL) {
      chan = new ESP32PWM();
    }
#endif
    
    registerCommand(HERE,"on", "Enable the PWM output");
    registerCommand(HERE,"off", "Disable the PWM output");
    registerCommand(HERE,"toggle", "Toggle the PWM output");
    registerCommand(HERE,"test", "Output a square wave using GPIO and a tight loop");

    registerLeafValue(HERE, "duration", VALUE_KIND_INT, &duration, "Time (in ms) to run the PWM output when started (0=indefinite)", ACL_GET_SET, VALUE_NO_SAVE);
    registerLeafValue(HERE, "freq", VALUE_KIND_INT, &frequency, "PWM frequency (in Hz)", ACL_GET_SET, VALUE_NO_SAVE);
    registerLeafValue(HERE, "duty", VALUE_KIND_FLOAT, &duty, "PWM duty cycle (in [0.0,1.0])", ACL_GET_SET, VALUE_NO_SAVE);
    registerLeafValue(HERE, "state",VALUE_KIND_BOOL, &state, "PWM output state (1=on/0=off)", ACL_GET_SET, VALUE_NO_SAVE);

    LEAF_NOTICE("%s claims pin %d as PWM", describe().c_str(), (int)pwmPin);
    LEAF_NOTICE("%s PWM settings %dHz %d%% duty", describe().c_str(), frequency, (int)(duty*100));

  }

  virtual void start() 
  {
    Leaf::start();
    if (!pwm8266Run) {
      pwm8266Timer.attachInterruptInterval(pwm8266IntervalUsec, Pwm8266TimerHandler);
      pwm8266Run = true;
    }
  }

  virtual void stop() 
  {
    if (pwm8266Run) {
      pwm8266Timer.detachInterrupt();
      pwm8266Run=false;
    }
    Leaf::stop();
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
    unsigned long elapsed = 0;
    LEAF_INFO("Half wave time of test cycle will be %dus", cycle);

    do {
      digitalWrite(pwmPin, HIGH);
      delayMicroseconds(cycle);
      digitalWrite(pwmPin, LOW);
      delayMicroseconds(cycle);
      ++pulses;
      elapsed+=2*cycle;
      if (elapsed > 2000000) {
	wdtReset(HERE);
	elapsed=0;
      }
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

  virtual bool valueChangeHandler(String topic, Value *v) {
    LEAF_HANDLER(L_NOTICE);

    WHENAND("freq",state, {
#ifdef ESP32
	if (chan) {
	  chan->writeTone(frequency);
	}
#endif
#ifdef ESP8266
	pwm8266.setPWM(pwmPin, frequency, duty*100);
#endif	
      })
    ELSEWHEN("duration",startPWM())
    ELSEWHEN("duty",{
      if (duty > 1.1) {
	LEAF_ALERT("Invalid duty value, must be in range [0,1]");
	LEAF_RETURN(true);
      }
      else if (duty > 1) {
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
	else if (state) {
	  // We are running PWM already, just change the duty cycle
	  LEAF_NOTICE("Update duty cycle to %.3f", duty);
#ifdef ESP32
	  if (chan) {
	    chan->writeScaled(duty);
	  }
#endif
#ifdef ESP8266
	  pwm8266.setPWM(pwmPin, frequency, duty*100);
#endif
	}
      }
    })
    ELSEWHEN("state", if (state) startPWM(); else stopPWM();)
    else handled = Leaf::valueChangeHandler(topic, v);
    
    if (handled) status_pub();

    LEAF_HANDLER_END;
  }

  virtual bool commandHandler(String type, String name, String topic, String payload) {
    LEAF_HANDLER(L_NOTICE);

    WHEN("on", startPWM())
    ELSEWHEN("off",stopPWM())
    ELSEWHEN("toggle",{
      if (state) {
	stopPWM();
      }
      else {
	startPWM();
      }
    })
    ELSEWHEN("test", pwm_test(payload.toInt()))
    else handled = Leaf::commandHandler(type, name, topic, payload);

    if (handled) status_pub();
    LEAF_HANDLER_END;
  }
};

#ifdef ESP8266
extern "C" {
    
  void IRAM_ATTR Pwm8266TimerHandler()
  {
    pwm8266.run();
  }

}
#endif

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
