//
//@**************************** class AnalogACLeaf ******************************
// 
// This class encapsulates an analog input sensor publishes measured
// voltage values to MQTT
// 
#include "leaf_analog.h"

hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

int adcPin;
volatile uint32_t intCount;
volatile uint32_t sampleCount;
volatile int minRead;
volatile int maxRead;

void IRAM_ATTR onTimer() 
{
  int value = -1;
  
  portENTER_CRITICAL_ISR(&timerMux);
  intCount++;
  if (!adcBusy(adcPin)) {
    value = adcEnd(adcPin);
    sampleCount++;
    if ((minRead < 0) || (value < minRead)) minRead = value;
    if ((maxRead < 0) || (value > maxRead)) maxRead = value;
  }
  portEXIT_CRITICAL_ISR(&timerMux);

  if (value != -1) {
    adcStart(adcPin);
  }
}


class AnalogACLeaf : public AnalogInputLeaf
{
protected:
  int count = 0;
  int dp = 3;

  int raw_min = -1;
  int raw_max = 0;
   
  float max = nan("nomax");
  float min = nan("nomin");
  unsigned long interval_start = 0;
  
public:
  AnalogACLeaf(String name, pinmask_t pins, int in_min=0, int in_max=4095, float out_min=0, float out_max=100, bool asBackplane = false) : AnalogInputLeaf(name, pins, in_min, in_max, out_min, out_max, asBackplane) 
  {
    LEAF_ENTER(L_INFO);
    report_interval_sec = 5;
    sample_interval_ms = 1;
    LEAF_LEAVE;
  }

  void setup() 
  {
    AnalogInputLeaf::setup();

    FOR_PINS({adcPin=pin;});
    adcAttachPin(adcPin);
    adcStart(adcPin);

    reset();
    timer = timerBegin(0, 80, true);
    timerAttachInterrupt(timer, &onTimer, true);
    timerAlarmWrite(timer, 1000, true);
    timerAlarmEnable(timer);

  }

  void reset() 
  {
    minRead = maxRead = -1;
    count = raw_max = 0;
    raw_min = -1;
    max = nan("nomax");
    min = nan("nomin");

    interval_start = 0;
  }
  
  void status_pub() 
  {
    LEAF_ENTER(L_DEBUG);
    char buf[160];
    unsigned long interval_usec = micros()-interval_start;
    
    if (count == 0) {
      ALERT("No samples to send");
    }
    else {
      snprintf(buf, sizeof(buf), "{\"count\": %d, \"interval_usec\": %lu, \"raw_min\": %d, \"raw_max\":%d, \"min\": %f, \"max\":%f }",
	       count, interval_usec, raw_min, raw_max, min, max);
      //mqtt_publish("status/value", String(mean, dp));
      //mqtt_publish("status/rms", String(rms, dp));
      mqtt_publish("status/samples", buf);

      reset(); // set count and other accumulators to zero
    }
    LEAVE;
  }

  bool sample(void) 
  {
    LEAF_ENTER(L_DEBUG);
    int newSamples;
    
    portENTER_CRITICAL_ISR(&timerMux);
    newSamples = sampleCount - count;
    if (newSamples > 0) {
      raw_min = minRead;
      raw_max = maxRead;
    }
    portEXIT_CRITICAL_ISR(&timerMux);
    
    if (newSamples > 0) {
      count += newSamples;
      value = raw_min;
      min = get_value();

      value = raw_max;
      max = get_value();
    }
    return false;
  }
};


// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
