#pragma once
//#pragma STACX_LIB Seg7
#pragma STACX_LIB MaxLedControl

#define USE_MAX_LED 1
#define USE_SEG7 0
//
/*@************************** class LedSegLeaf *****************************/
//
// This class encapsulates a numeric LED array, eg using max7219.
// Requires the Seg7 arduino library
//
//
#include "leaf.h" // unnecessary but helps static code checker

#if USE_SEG7
#include <Seg7.h>
#elif USE_MAX_LED
#include <MaxLedControl.h>
#else
#error no USE_xxlibxx config
#endif

class LedSegLeaf : public Leaf
{
  //
  // Configuration
  //
  int din = 13; // default pins for esp8266 (not the spi pins because reasons)
  int cs=14;
  int clk=12;
  int digit_count=4;
  int brightness=2;


  //
  // Derived configuration
  //

  //
  // Ephemeral state
  //
  #if USE_SEG7
  Seg7 *disp=NULL;
  #elif USE_MAX_LED
  LedControl *disp=NULL;
  #endif

public:
  LedSegLeaf(String name,
	     String target,
	     int dig=4,
	     int din=-1,
	     int cs = -1,
	     int clk = -1,
	     int brt=2
	  )
    : Leaf("ledseg", name, NO_PINS, target)
    , Debuggable(name, L_INFO)
  {
    if (din >= 0) this->din = din;
    if (cs >= 0) this->cs = cs;
    if (clk >= 0) this->clk = clk;
    this->digit_count=dig;
    this->brightness=brt;
    claimPin(din);
    claimPin(cs);
    claimPin(clk);

  }

  void clear()
  {
#if USE_SEG7
    disp->clear();
#elif USE_MAX_LED
    disp->clearDisplay(0);
#endif
  }

  void num(int n)
  {
#if USE_SEG7
    disp->num(n);
#else
    for (int p=digit_count-1;p>=0;p--) {
      disp->setDigit(0, p, n%10, false);
      n/=10;
    }
#endif
  }

  void hex(int n)
  {
#if USE_SEG7
    disp->hex(n);
#else
    for (int p=digit_count-1;p>=0;p--) {
      disp->setDigit(0, p, n%16, false);
      n/=16;
    }
#endif
  }

  void stg(const char *s)
  {
#if USE_SEG7
    disp->stg(s);
#else
    for (int p=0; (p<digit_count) && s[p]; p++) {
      disp->setChar(0, p, s[p], false);
    }
#endif
  }

  virtual void setup(void) {
    Leaf::setup();
    LEAF_ENTER(L_NOTICE);

    if (disp==NULL) {
#if USE_SEG7
	    this->disp = new Seg7(brightness, cs, din, clk, digit_count);
#elif USE_MAX_LED
	    this->disp = new LedControl(din, clk, cs, 1, false);
#endif
	    if (this->disp == NULL) {
		    LEAF_ALERT("Max7129 display allocation failed");
		    return;
	    }
    }

    registerLeafIntValue("brightness", &brightness, "display brightness");
    registerLeafCommand(HERE, "num", "display a number");
    registerLeafCommand(HERE, "hex", "display a hex string");
    registerLeafCommand(HERE, "string", "display a l33t string");
    registerLeafCommand(HERE, "clear", "clear display");


    LEAF_NOTICE("MAX7219 with pins DIN/CS/CLK=%d/%d/%d digits=%d brightness=%d",
		din, cs, clk, digit_count, brightness);
#if USE_SEG7
    disp->clear();
#elif USE_MAX_LED
    /*
      The MAX72XX is in power-saving mode on startup,
      we have to do a wakeup call
    */
    disp->begin(brightness);
    disp->shutdown(0,false); // wake from shutdown mode
    disp->setIntensity(0, brightness);
    disp->clearDisplay(0);
#endif

    LEAF_LEAVE;
  }


  virtual bool commandHandler(String type, String name, String topic, String payload) {
    LEAF_HANDLER(L_NOTICE);

    WHENAND("clear", disp, {
		    LEAF_NOTICE("clearing screen");
		    clear();
		      })
    ELSEWHENAND("num", disp, {
		    LEAF_NOTICE("Display number [%s]", payload.c_str());
		    num(payload.toInt());
    })
    ELSEWHENAND("hex", disp, {
		    LEAF_NOTICE("Display hex [%s]", payload.c_str());
		    hex(payload.toInt());
    })
    ELSEWHENAND("string", disp, {
		    LEAF_NOTICE("Display string [%s]", payload.c_str());
		    stg(payload.c_str());
    })
    else if (!handled) {
      handled = Leaf::commandHandler(type, name, topic, payload);
    }

    LEAF_HANDLER_END;
  }

};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
