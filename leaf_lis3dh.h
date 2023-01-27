//
//@**************************** class LIS3dhLeaf ******************************
//
// This class encapsulates the LIS3DH 6-axis IMS sensor
//

#include <Wire.h>
#include "abstract_ims.h"

#include "Adafruit_LIS3DH.h"

class LIS3DHLeaf : public AbstractIMSLeaf
{
protected:
  bool found;
  Adafruit_LIS3DH lis;
  float delta_ang = 5;
  byte adc_mask = 0;
  uint16_t adc[3];
  uint16_t adc_delta[3];
  int adc_sample_interval_sec = 30;
  unsigned long last_adc_sample = 0;

public:
  LIS3DHLeaf(String name, byte adc_mask=0) : AbstractIMSLeaf(name, 0) {
    LEAF_ENTER(L_INFO);
    found = false;
    this->sample_interval_ms = 500;
    this->adc_sample_interval_sec = 30;
    this->report_interval_sec = 900;
    this->adc_mask = adc_mask;
    for (int i=0; i<3; i++) {
      adc[i] = 0;
      adc_delta[i] = 50;
    }
    LEAF_LEAVE;
  }

  virtual void setup(void) {
    AbstractIMSLeaf::setup();

    LEAF_ENTER(L_INFO);
    Wire.begin();
    if (! lis.begin(0x18)){
      LEAF_ALERT("LIS3DH sensor not found");
    }
    else {
      found = true;
      LEAF_NOTICE("Found LIS3DH");
      lis.setRange(LIS3DH_RANGE_16_G);


    }

    LEAF_LEAVE;
  }

  void adc_poll()
  {
    const int oversample = 3;
    unsigned long s;
    int n;

    for (int i=0;i<3;i++) {
      if (adc_mask & (1<<i)) {
	// Analog-to-digital Channel i+1 is wanted (channel numbers are
	// 1-based)
	//
	// Take 3 samples and average them, cos the ADC is noisy AF
	for (n=1,s=0 ; n<=oversample; n++) {
	  uint16_t v = lis.readADC(i+1);
	  s += v;
	  //LEAF_NOTICE("Channel %d oversample %d s=%lu", i, n, s);
	  delay(5);
	}
	int value = s / oversample;
	//LEAF_NOTICE("Average of %d samples is %d", oversample, value);

	// Only consider the value to have "changed" if it's different by
	// more than adc_delta[i]
	//
	if (abs(value-adc[i])>adc_delta[i]) {
	  LEAF_NOTICE("ADC channel %d new value %d", i, value);
	  adc[i] = value;
	  publish("status/adc/"+String(i+1), String((int)adc[i]));
	}
      }
    }
  }

  virtual bool poll()
  {
    if (!found) return false;

    LEAF_ENTER(L_DEBUG);
    bool result = false;
    float accX,accY,accZ;
    float angX,angY;

    lis.read();
    accX = lis.x;
    accY = lis.y;
    accZ = lis.z;


    unsigned long now = millis();
    if (now > (last_adc_sample + (adc_sample_interval_sec * 1000))) {
      adc_poll();
      last_adc_sample = now;
    }

    // Computing accel angles
    angX = wrap((atan2(accY, sqrt(accZ * accZ + accX * accX))) * RAD_TO_DEG);
    angY = wrap((-atan2(accX, sqrt(accZ * accZ + accY * accY))) * RAD_TO_DEG);
    //LEAF_DEBUG("angAcc[]=[%4.f,%4.f]",angX, angY);

    //
    // Calculate if inclination has changed "significantly"
    //
    if (isnan(tilt_x) || abs(tilt_x-angX) >= delta_ang) {
      result=true;
      tilt_x = angX;
    }
    if (isnan(tilt_y) || abs(tilt_y-angY) >= delta_ang) {
      tilt_y = angY;
      result=true;
    }

    LEAF_LEAVE;
    return result;

  }

  void status_pub()
  {
    if (!found) return;

    AbstractIMSLeaf::status_pub();
    char msg[64];

    snprintf(msg, sizeof(msg), "[%.2f, %.2f]", tilt_x, tilt_y);
    mqtt_publish("status/orientation", msg);
  }


};


// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
