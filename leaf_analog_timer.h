//
//@**************************** class AnalogRMSLeaf ******************************
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
volatile uint32_t minRead;
volatile uint32_t maxRead;

void IRAM_ATTR onTimer() 
{
  int value = -1;
  
  portENTER_CRITICAL_ISR(&timerMux);
  intCount++;
  if (!adcBusy(adcPin)) {
    value = adcEnd(adcPin);
    sampleCount++;
    if (value < minRead) minRead = value;
    if (value > maxRead) maxRead = value;
  }
  portEXIT_CRITICAL_ISR(&timerMux);

  if (value != -1) {
    adcStart(adcPin);
  }
}


class AnalogRMSLeaf : public AnalogInputLeaf
{
protected:
  static const int raw_sample_max = 256;
  static const int sample_max = 60;

  bool log_raw = false;
  bool log_history = false;

  int count = 0;
  int dp = 3;

  int raw_min = -1;
  int raw_max = 0;
  
  float max = nan("nomax");
  float min = nan("nomin");
  float sum = 0;
  float sum_squares = 0;
  unsigned long interval_start = 0;

  struct timeval buf_start;
  uint16_t raw_buf[raw_sample_max];

  int history_count = 0;
  struct timeval history_start;
  float history_buf[sample_max];

  SDCardLeaf *sdcard = NULL;
  char log_name[64];
  char raw_name[64];
  
  
public:
  AnalogRMSLeaf(String name, pinmask_t pins, int in_min=0, int in_max=1023, float out_min=0, float out_max=100, bool asBackplane = false) : AnalogInputLeaf(name, pins, in_min, in_max, out_min, out_max, asBackplane) 
  {
    LEAF_ENTER(L_INFO);
    report_interval_sec = 5;
    sample_interval_ms = 1;
    LEAF_LEAVE;
  }

  void setup() 
  {
    AnalogInputLeaf::setup();
    this->install_taps("sdcard");
    this->sdcard = (SDCardLeaf *)this->get_tap("sdcard");
    snprintf(this->log_name, sizeof(this->log_name), "/%s.csv", this->leaf_name);
    snprintf(this->raw_name, sizeof(this->raw_name), "/%s_raw.csv", this->leaf_name);

    if (this->sdcard) {
      if (!this->sdcard->fs->exists(this->raw_name)) {
	char buf[80];
	snprintf(buf, sizeof(buf), "# %s\n", this->raw_name);
	this->sdcard->writeFile(this->raw_name, buf);
      }
    }

    FOR_PINS({adcPin=pin;});
    adcAttachPin(adcPin);
    adcStart(adcPin);

    minRead = maxRead = 2048; // input biased to vcc/2
    timer = timerBegin(0, 80, true);
    timerAttachInterrupt(timer, &onTimer, true);
    timerAlarmWrite(timer, 1000, true);
    timerAlarmEnable(timer);

  }

  void reset() 
  {
    count = value = raw_max = 0;
    raw_min = -1;
    max = nan("nomax");
    min = nan("nomin");

    sum = sum_squares = 0;
    interval_start = 0;
  }
  
  void status_pub() 
  {
    LEAF_ENTER(L_DEBUG);
    char buf[160];
    unsigned long interval_usec = micros()-interval_start;
    
    if (count == 0) {
      LEAF_ALERT("No samples to send");
    }
    else {
      float mean = sum/count;
      float rms = sqrt(sum_squares/count);
      snprintf(buf, sizeof(buf), "{\"count\": %d, \"interval_usec\": %lu, \"raw_min\": %d, \"raw_max\":%d, \"min\": %f, \"max\":%f, \"mean\": %f, \"rms\": %f}",
	       count, interval_usec, raw_min, raw_max, min, max, mean, rms);
      mqtt_publish("status/value", String(mean, dp));
      mqtt_publish("status/rms", String(rms, dp));
      mqtt_publish("status/samples", buf);

      if (log_raw && (count%raw_sample_max > 0)) {
	send_raw();
      }

      if (history_count == 0) {
	gettimeofday(&history_start, NULL);
      }
      history_buf[history_count++] = max;
      LEAF_NOTICE("history_count=%d", history_count);
      if (log_history && (history_count == sample_max)) {
	send_history();
      }

      reset(); // set count and other accumulators to zero
    }
    LEAVE;
  }

  void send_raw() 
  {
    LEAF_ENTER(L_DEBUG);
    
    // "0123455679.654321,4096,1234.123,...
    char buf[10+1+6+1 + (this->raw_sample_max * (4+1+4+1+3+1))];
    int offset = 0;
    
    snprintf(buf+offset, sizeof(buf)-offset, "%u.%06u,",
	     buf_start.tv_sec, buf_start.tv_usec);
    offset = strlen(buf);
    int buffer_count = this->count % this->raw_sample_max;
    if (buffer_count == 0) buffer_count = this->raw_sample_max;
    for (int i = 0 ; i < buffer_count ; i++) {
      char sample[20];
      snprintf(sample, sizeof(sample), "%u,", raw_buf[i]);
      strlcpy(buf+offset, sample, sizeof(buf)-offset);
      offset += strlen(sample);
    }
    buf[offset-1]='\n';
    LEAF_NOTICE("send_raw %d %d", buffer_count, offset);
    //publish("samples", buf);
    if (this->sdcard && this->log_raw) {
      sdcard->appendFile(this->raw_name, buf);
    }
    
    LEAF_LEAVE;
  }

  void send_history() 
  {
    LEAF_ENTER(L_DEBUG);
    
    // "0123455679.654321,1234.123,...
    char buf[10+1+6+1 + (this->sample_max * (4+1+3+1))];
    int offset = 0;
    
    snprintf(buf+offset, sizeof(buf)-offset, "%u.%06u,",
	     history_start.tv_sec, history_start.tv_usec);
    offset = strlen(buf);
    int buffer_count = this->history_count % this->sample_max;
    if (buffer_count == 0) buffer_count = this->sample_max;
    for (int i = 0 ; i < buffer_count ; i++) {
      char sample[20];
      snprintf(sample, sizeof(sample), "%.3f,", history_buf[i]);
      strlcpy(buf+offset, sample, sizeof(buf)-offset);
      offset += strlen(sample);
    }
    buf[offset-1]='\n';
    LEAF_NOTICE("send_history %d %d", buffer_count, offset);
    //publish("samples", buf);
    if (this->sdcard && this->log_history) {
      sdcard->appendFile(this->log_name, buf);
    }
    this->history_count = 0;
    
    LEAF_LEAVE;
  }
  
  bool sample(void) 
  {
    LEAF_ENTER(L_DEBUG);
    int inputPin;
    FOR_PINS({inputPin=pin;});
    // time to take a new sample
    if (interval_start == 0) {
      interval_start = micros();
    }

    value = analogRead(inputPin);
    if ((raw_min < 0) || (value < raw_min)) raw_min = value;
    if (value > raw_max) raw_max = value;
    
    float float_value = convert(value);
    if ((count%raw_sample_max) == 0) gettimeofday(&buf_start, NULL);
    raw_buf[count%raw_sample_max] = (uint16_t)value;
    count = count+1;
    LEAF_INFO("Sampling Analog input on pin %d: count=%d raw=%d mapped=%f", inputPin, count, value, float_value);

    if (isnan(max) || (float_value > max)) max = float_value;
    if (isnan(min) || (float_value < min)) min = float_value;
    sum += float_value;
    sum_squares += (float_value*float_value);

    if (log_raw && (count%raw_sample_max == 0)) {
      send_raw();
    }
    
      

    // We do not use the 'changed' mechanism in this leaf, reporting is by
    // interval only
    RETURN(false);
  }
};


// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
