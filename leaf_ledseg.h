#pragma once
#pragma STACX_LIB Seg7
//
/*@************************** class LedSegLeaf *****************************/
//
// This class encapsulates a numeric LED array, eg using max7219.
// Requires the Seg7 arduino library
//
// 
#include "leaf.h" // unnecessary but helps static code checker
#include <Seg7.h>

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
  Seg7 *disp=NULL;
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

  void setup(void) {
    Leaf::setup();
    LEAF_ENTER(L_NOTICE);

    if (disp==NULL) {
	    this->disp = new Seg7(brightness, cs, din, clk, digit_count);
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
    disp->clear();
    disp->stg("7219");//NOCOMMIT
    
    LEAF_LEAVE;
  }

  virtual bool commandHandler(String type, String name, String topic, String payload) {
    LEAF_HANDLER(L_NOTICE);

    WHENAND("clear", disp, {
		    LEAF_NOTICE("clearing screen");
		    disp->clear();
		      })
    ELSEWHENAND("num", disp, {
		    LEAF_NOTICE("Display number [%s]", payload.c_str());
		    disp->num(payload.toInt());
    })
    ELSEWHENAND("hex", disp, {
		    LEAF_NOTICE("Display hex [%s]", payload.c_str());
		    disp->hex(payload.toInt());
    })
    ELSEWHENAND("string", disp, {
		    LEAF_NOTICE("Display string [%s]", payload.c_str());
		    disp->stg(payload.c_str());
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
