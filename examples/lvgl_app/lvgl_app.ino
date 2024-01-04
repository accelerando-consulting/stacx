#pragma STACX_BOARD espressif:esp32:esp32
#include "config.h"
#include "stacx.h"

#include "leaf_fs.h"
#include "leaf_fs_preferences.h"
#include "leaf_shell.h"
#include "leaf_ip_null.h"
#include "leaf_pubsub_null.h"
#include "leaf_lvgl.h"
#include "abstract_app.h"

class LVGLExampleAppLeaf : public AbstractAppLeaf
{
protected: // config
  int refresh_msec = 200;
  
protected: // LVGL screen resources
  lv_style_t style_clock;
  lv_obj_t *screen_home = NULL;
  lv_obj_t *home_clock = NULL;
  TFTLeaf *screen=NULL;

protected: // ephemeral state
  unsigned long last_refresh = 0;

public:
  LVGLExampleAppLeaf(String name, String target=NO_TAPS)
    : AbstractAppLeaf(name,target)
    , Debuggable(name)
  {
  }

  void refresh() {
    LEAF_ENTER(L_INFO);

    char buf[64];
    time_t now;
    struct tm timeinfo;
  
    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(buf, sizeof(buf), "%X", &timeinfo);
    lv_label_set_text(home_clock, buf);
    lv_timer_handler();
    LEAF_LEAVE;
  }

  virtual void loop() {
    AbstractAppLeaf::loop();
    unsigned long now=millis();

    if (now > (last_refresh+refresh_msec)) {
    refresh();
    last_refresh=now;
  }
}
      
  
  virtual void setup(void) {
    AbstractAppLeaf::setup();
    LEAF_ENTER(L_NOTICE);
    screen = (TFTLeaf *)get_tap("screen");
    LEAF_LEAVE;
  }
  
  virtual void start(void) {
    LEAF_ENTER(L_NOTICE);
    if (screen) {
      screen_home = lv_obj_create(NULL);
      lv_style_init(&style_clock);
      lv_style_set_text_font(&style_clock, &lv_font_montserrat_32);
      lv_style_set_text_color(&style_clock, lv_palette_main(LV_PALETTE_BLUE_GREY));

      home_clock = lv_label_create(screen_home);
      lv_obj_add_style(home_clock, &style_clock, 0);
      lv_label_set_text(home_clock, "");
      lv_obj_align(home_clock, LV_ALIGN_TOP_MID, 5, 20);

      LEAF_NOTICE("Load home screen");
      lv_scr_load(screen_home);

    }
    else {
      LEAF_ALERT("No screen resource");
    }
    LEAF_LEAVE;
    AbstractAppLeaf::start();
  }

};

  


Leaf *leaves[] = {
  new FSLeaf("fs", FS_DEFAULT, FS_ALLOW_FORMAT),
  new FSPreferencesLeaf("prefs"),
  new ShellLeaf("shell", "Stacx CLI"),

  (new IpNullLeaf("nullip", "fs"))->setTrace(L_WARN),
  (new PubsubNullLeaf("nullmqtt", "ip"))->setTrace(L_WARN),
  
  new LVGLLeaf("screen", 3),
  new LVGLExampleAppLeaf("app","screen,prefs"),
  NULL
};
