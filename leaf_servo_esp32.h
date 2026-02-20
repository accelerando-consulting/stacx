//@***************************** class LightLeaf ******************************

#ifdef ESP8266
#error Not implemented yet
#endif

#ifdef ESP32
#include "ESP32Servo.h"
#endif

class ServoLeaf : public Leaf
{
public:
  bool state=false;
  int pwmPin = -1;
  int angle=0;
#ifdef ESP32
  Servo servo;
#endif

  ServoLeaf(String name, pinmask_t pins)
    : Leaf("pwm", name, pins)
    , Debuggable(name)
  {
    state = false;
  }

  void detach()
  {
    LEAF_NOTICE("detach (pwmPin %d)", (int)pwmPin);
    if (pwmPin < 0) return;
#ifdef ESP32
    if (servo.attached()) {
      servo.detach();
    }
    
#endif
    pinMode(pwmPin, OUTPUT);
    digitalWrite(pwmPin, LOW);
    state=false;
  }

  void attach()
  {
    if (pwmPin < 0) return;
    LEAF_NOTICE("attach (pwmPin %d)", pwmPin);

#ifdef ESP32
    if (!servo.attached()) {
	LEAF_INFO("Attach pin %d", (int)pwmPin);
	servo.attach(pwmPin);
    }
    LEAF_INFO("Servo write: %d", angle);
    servo.write(angle);
#endif
    state=true;
  }

  virtual void setup(void) {
    Leaf::setup();
    FOR_PINS({pwmPin=pin;});
    enable_pins_for_output();
    digitalWrite(pwmPin, LOW);
    
    registerLeafCommand(HERE,"on", "Enable the Servo output");
    registerLeafCommand(HERE,"off", "Disable the Servo output");
    registerLeafCommand(HERE,"toggle", "Toggle the Servo output");
    registerLeafCommand(HERE,"test", "Output a square wave using GPIO and a tight loop");

    registerLeafIntValue("angle", &angle, "servo angle in degrees", ACL_GET_SET, VALUE_NO_SAVE);
    registerLeafBoolValue("state", &state, "servo enable state", ACL_GET_SET, VALUE_NO_SAVE);

    LEAF_NOTICE("%s claims pin %d as PWM", describe().c_str(), (int)pwmPin);

  }

  virtual void start() 
  {
    Leaf::start();
#ifdef ESP8266
    if (!pwm8266Run) {
      pwm8266Timer.attachInterruptInterval(pwm8266IntervalUsec, Pwm8266TimerHandler);
      pwm8266Run = true;
    }
#endif
    if (state) {
      attach();
    }
  }

  virtual void stop() 
  {
#ifdef ESP8266
    if (pwm8266Run) {
      pwm8266Timer.detachInterrupt();
      pwm8266Run=false;
    }
#endif
    detach();
    Leaf::stop();
  }

  void servo_test(int secs)
  {
    LEAF_ENTER_INT(L_NOTICE, secs);
    bool restart=false;

    if (state) {
      detach();
      restart=true;
    }

    pinMode(pwmPin, OUTPUT);

    unsigned long start=millis();
    unsigned long stop = start + ( 1000 * secs);
    unsigned long pulses = 0;
    unsigned long elapsed = 0;
    int cycle = 20000;
    int pulse_len = angle;

    if (angle <= 180) {
      // 0-180 means degrees, 1000-2000 means msec
      pulse_len = 1000 + (angle * 1000 / 180);
    }

    LEAF_INFO("Half wave time of test cycle will be %dus", cycle);

    do {
      digitalWrite(pwmPin, HIGH);
      delayMicroseconds(pulse_len);
      digitalWrite(pwmPin, LOW);
      delayMicroseconds(cycle-pulse_len);
      ++pulses;
      elapsed+=cycle;
      if (elapsed > 2000000) {
	wdtReset(HERE);
	elapsed=0;
      }
    } while (millis() < stop);
    LEAF_NOTICE("Tested for %lu sec, did %dlu pulses", secs, pulses);

    if (restart) {
      pinMode(pwmPin, INPUT);
      attach();
    }

    LEAF_LEAVE;
  }


  virtual void status_pub()
  {
    LEAF_ENTER_INT(L_DEBUG, state?1:0);

    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    obj["state"]=state;
    obj["pwmPin"]=pwmPin;
    obj["angle"]=angle;
    char msg[256];
    serializeJson(doc, msg, sizeof(msg)-2);
    mqtt_publish(String("status/")+leaf_name, msg);
    LEAF_LEAVE;
  }

  virtual bool valueChangeHandler(String topic, Value *v)
  {
    LEAF_HANDLER(L_INFO);

    WHEN("angle", {
#ifdef ESP32
	LEAF_INFO("Update servo angle %d", angle);
	if (state) {
	  servo.write(angle);
	}
#endif
      })
    ELSEWHEN("state", if (state) attach(); else detach();)
    else handled = Leaf::valueChangeHandler(topic, v);
    
    if (handled) status_pub();

    LEAF_HANDLER_END;
  }

  virtual bool commandHandler(String type, String name, String topic, String payload) {
    LEAF_HANDLER(L_DEBUG);

    WHEN("on", attach())
    ELSEWHEN("off",detach())
    ELSEWHEN("toggle",{
      if (state) {
	attach();
      }
      else {
	detach();
      }
    })
    ELSEWHEN("test", servo_test(payload.toInt()))
    else handled = Leaf::commandHandler(type, name, topic, payload);

    LEAF_HANDLER_END;
  }
};


// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
