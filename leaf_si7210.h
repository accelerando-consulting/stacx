//
//@**************************** class Si7210Leaf ******************************
//
// This class encapsulates a Si7210 I2C hall-effect sensor
//
#include <Wire.h>
#include "trait_wirenode.h"
#include "trait_pollable.h"

// You should define a wireleaf in your leaves.h before including this leaf

// The Si7210 address is factory programmed to one of 0x30, 0x31, 0x32 or 0x33.

class Si7210Leaf : public Leaf, public WireNode, public Pollable
{
public:
  Si7210Leaf(String name, pinmask_t pins=0, byte address=0)
    : Leaf("si7210", name, pins)
    , WireNode(address)
    , Pollable(500, 900)
    , Debuggable(name)
 {
    LEAF_ENTER(L_INFO);
    this->delta = 256;
    this->do_heartbeat = false;

    LEAF_LEAVE;
  }

  virtual void setup();
  virtual void loop();
  virtual void status_pub();

protected:
  byte id;
  int field;
  int delta;
  float temp;

  static const int reg_id = 0xc0;
  static const int reg_dspsigm = 0xc1;
  static const int reg_dspsigl = 0xc2;
  static const int reg_dspsigsel = 0xc3;
  static const int reg_ctrl = 0xc4;
  static const int reg_autoinc = 0xc5;

  static const int bit_ctrl_sleep = 0;
  static const int bit_ctrl_stop = 1;
  static const int bit_ctrl_oneburst = 2;
  static const int bit_ctrl_usestore = 3;
  static const int bit_ctrl_meas = 7;

  static const int scale_fine = 0.00125;  // fine   scale => 0.00125 mT/bit (+-20.47mT full scale)
  static const int scale_coarse = 0.0125; // coarse scale => 0.01250 mT/bit (+-204.7mT full scale)

  int scale = scale_coarse;
  float offset;
  float gain;

  void set_oneshot(bool oneshot=true, int sig=0);
  virtual bool poll ();
  bool pollTemp() ;


};

void Si7210Leaf::setup(void) {
  Leaf::setup();
  LEAF_ENTER(L_DEBUG);

  if (address == 0) {
    // autodetect the device
    for (address = 0x30; address <= 0x33; address++) {
      LEAF_DEBUG("Probe 0x%x", (int)address);
      int v = read_register(0xc0, 500);
      if (v < 0) {
	LEAF_DEBUG("No response from I2C address %02x", (int)address);
	continue;
      }
      if (v != 0x14) {
	LEAF_NOTICE("Chip ID not recognised %02x", (int)v);
	continue;
      }
      LEAF_INFO("Detected si7210 (id=%02x) at address %02x ", (int)id, (int)address);
      id = v;
      break;
    }
    if (address > 0x33) {
      LEAF_NOTICE("Failed to detect si7210");
      address = 0;
    }
  }

  // on accelerando's si7210 board we connect the open-drain-with-pullup interrupt output to gpio0
  pinMode(0, INPUT);

  // Turn on continuous conversion (TODO: use interrupts instead)
  if (address) {
    //set_oneshot(false);
  }

  // Read the temperature calibration coefficients (datasheet p18)
  int rawOffset = read_register(0x1d);
  int rawGain = read_register(0x1e);
  offset = rawOffset/16.0;
  gain = 1 + (rawGain/2048.0);
  LEAF_NOTICE("rawOffset=%d rawGain=%d offset=%f gain=%f", rawOffset, rawGain, offset, gain);

  LEAF_LEAVE;
}

void Si7210Leaf::set_oneshot(bool oneshot, int sig)
{
  LEAF_ENTER(L_DEBUG);

  byte sigr = read_register(reg_dspsigsel);
  if ((sigr & 0x07) != sig) {
    write_register(reg_dspsigsel, (sigr & ~0x07)|sig);
  }

  byte ctrl = read_register(reg_ctrl);
  //LEAF_DEBUG("Ctrl register is %02x", (int)ctrl);
  if (oneshot) {
    clear_bit(ctrl, bit_ctrl_stop);
    set_bit(ctrl, bit_ctrl_oneburst);
  }
  else {
    clear_bit(ctrl, bit_ctrl_stop);
    clear_bit(ctrl, bit_ctrl_oneburst);
    }
  //LEAF_DEBUG("Set ctrl register to %02x", (int)ctrl);
  write_register(reg_ctrl, ctrl);

  LEAF_LEAVE;
}

bool Si7210Leaf::poll()
{
  LEAF_ENTER(L_DEBUG);

  bool result = false;

  set_oneshot(true);

  unsigned long start = millis();

  bool newResult = false;
  int newField = 0;
  while (!newResult) {
    byte m = read_register(reg_dspsigm);
    byte l = read_register(reg_dspsigl);
    newResult = (m & 0x80);

    unsigned long now = millis();
    if (!newResult && (now  > (start+sample_interval_ms)) ) {
      LEAF_ALERT("poll timeout");
      break;
    }
    LEAF_DEBUG("poll m=%02x l=%02x (%s)", m,l,newResult?"NEW RESULT":"no new result");
    if (!newResult) {
      continue;
    }

    newField = (((m&0x7f)<<8)|(l&0xFF))-16384;
    // float mT = newField*scale
    LEAF_DEBUG("poll result %d", newField);
  }

  if (newResult) {
    if (abs(field - newField) > delta) {
      field = newField;
      result = true;
    }
  }
  result |= pollTemp();

  LEAF_LEAVE;
  return result;
}

#define TEMP_HISTORY 3

bool Si7210Leaf::pollTemp()
{
  LEAF_ENTER(L_DEBUG);
  static float history[TEMP_HISTORY];
  int history_count = 0;

  bool result = false;

  set_oneshot(true, 0x01); // select temp

  unsigned long start = millis();

  bool newResult = false;
  int value = 0;
  float newTemp;
  float rawTemp;
  float vdd = 3.3;

  while (!newResult) {
    byte m = read_register(reg_dspsigm);
    byte l = read_register(reg_dspsigl);
    newResult = (m & 0x80);

    unsigned long now = millis();
    if (!newResult && (now  > (start+sample_interval_ms)) ) {
      LEAF_ALERT("poll timeout");
      break;
    }
    LEAF_DEBUG("pollTemp m=%02x l=%02x (%s)", m,l,newResult?"NEW RESULT":"no new result");
    if (!newResult) {
      continue;
    }

    value = ((m&0x7f)*32)+ (l>>3);  // wTF but that's what the datasheet says
    LEAF_DEBUG("pollTemp result %d", value);
  }

  if (newResult) {
    rawTemp = -3.83e-6 * value * value + 0.16094 * value - 279.8;  // whut the actual, datasheet page 18
    newTemp = gain * rawTemp + offset - (vdd * 0.222);
    // todo: average the last few samples
    LEAF_DEBUG("rawTemp=%f newTemp=%f temp=%f", rawTemp, newTemp, temp);

    if (history_count < TEMP_HISTORY) {
      // history array not full, just append
      history[history_count++] = newTemp;
    }
    else {
      // history array is full, shift the values down one and append the new value
      for (int i=1; i < TEMP_HISTORY;i++) {
	history[i-1] = history[i];
      }
      history[TEMP_HISTORY] = newTemp;
    }

    float sum = 0;
    for (int i=0; i<history_count; i++) {
      sum += history[i];
    }
    newTemp = sum / history_count;
    LEAF_DEBUG("Averaged temp is %f", newTemp);

    result = (abs(temp-newTemp) > 0.5);
    if (result) {
      temp = newTemp;
    }
  }
  return result;
}




void Si7210Leaf::status_pub()
{
  LEAF_ENTER(L_INFO);
  publish("status/field", String(field));
  publish("status/temp", String(temp));
  LEAF_LEAVE;
}

void Si7210Leaf::loop(void) {
  LEAF_ENTER(L_DEBUG);

  if (!address) {
    //LEAF_DEBUG("si7210 not found");
    return;
  }
  Leaf::loop();


  pollable_loop();
  LEAF_LEAVE;
}


// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
