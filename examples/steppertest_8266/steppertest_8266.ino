#pragma STACX_BOARD esp8266:esp8266:wifi_kit_8

#undef HELLO_PIXEL
#undef HELLO_PIN
#include "defaults.h"
#include "config.h"
#include "stacx.h"

//
// Example stack: A modbus satellite (aka sl*v*) device
//
//

#include "leaf_fs.h"
#include "leaf_fs_preferences.h"
#include "leaf_shell.h"
#include "leaf_oled.h"
#include "leaf_wire.h"
#include "leaf_ip_null.h"
#include "leaf_pubsub_null.h"

#include "abstract_app.h"

class StepperAppLeaf : public AbstractAppLeaf 
{
protected:
  int spm_fwd = 120;
  int spm_rev = 120;
  int step_us = 10;
  bool dir_fwd = true;
  bool run = false;
  OledLeaf *screen=NULL;

public:
  StepperAppLeaf(String name, String target)
    : AbstractAppLeaf(name, target)
    , Debuggable(name) {
      

    }

  virtual void setup(void) {
    AbstractAppLeaf::setup();
    screen = (OledLeaf *)get_tap("screen");
    app_display_msec = 5000;
  }
  

#if 0
  virtual void display() {
    LEAF_ENTER(L_NOTICE);
    if (!screen) {
      LEAF_ALERT("no screen");
      return;
    }
    message(screen, "cmd/clear", "", HERE);
    
    cmdf(screen, "draw", "{\"font\":10,\"align\":\"left\",\"row\": 1,\"column\": 1,\"text\":\"%s %s %d\"}",[
	 (run?"RUN  ":"STOP"), dir_fwd?"FWD":"REV", dir_fwd?spm_fwd:spm_rev);
    LEAF_LEAVE;
  }
#endif

};



Leaf *leaves[] = {
  new FSLeaf("fs", FS_DEFAULT, FS_ALLOW_FORMAT),
  new FSPreferencesLeaf("prefs"),
  new ShellLeaf("shell", "Stacx CLI"),
  new WireBusLeaf("wire"),
  new OledLeaf("screen"),

  new IpNullLeaf("nullip", "fs"),
  new PubsubNullLeaf("nullmqtt", "nullip,fs"),
  new StepperAppLeaf("app","fs,screen"),
  
  NULL
};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
