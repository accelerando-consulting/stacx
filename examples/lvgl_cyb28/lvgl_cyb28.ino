#pragma STACX_BOARD espressif:esp32:esp32
#include "config.h"
#include "stacx.h"

#include "leaf_fs.h"
#include "leaf_fs_preferences.h"
#include "leaf_shell.h"
#include "leaf_ip_null.h"
#include "leaf_pubsub_null.h"
#include "leaf_lvgl_cyb28.h"
#include "abstract_app.h"

class LVGLExampleAppLeaf : public AbstractAppLeaf
{
protected: // config
  int refresh_msec = 200;
  
protected: // LVGL screen resources
  lv_style_t style_home,style_clock,style_border,style_meter,style_legend,style_rate;
  lv_obj_t *screen_home = NULL;
  lv_obj_t *home_clock = NULL;
  lv_obj_t *home_border = NULL;
  lv_obj_t *home_meter = NULL;
  lv_obj_t *home_legend = NULL;
  lv_obj_t *home_rate = NULL;
  LVGLLeaf *screen=NULL;

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
    screen = (LVGLLeaf *)get_tap("screen");
    LEAF_LEAVE;
  }
  
  virtual void start(void) {
    LEAF_ENTER(L_NOTICE);
    if (screen) {
      lv_coord_t w = screen->getWidth();
      lv_coord_t h = screen->getHeight();
      lv_coord_t i = 0;
      
      screen_home = lv_obj_create(NULL);

      static lv_point_t border_points[]={	{i,i}, {w-i-1,i}, {w-i-1,h-i-1}, {i,h-i-1},{i,i} };
      lv_style_init(&style_home);
      lv_style_set_bg_color(&style_home, lv_palette_main(LV_PALETTE_YELLOW));
      lv_obj_add_style(screen_home, &style_home, 0);
      
      lv_style_init(&style_border);
      lv_style_set_line_width(&style_border, 1);
      lv_style_set_line_color(&style_border, lv_palette_main(LV_PALETTE_BLUE));

      home_border = lv_line_create(screen_home);
      lv_line_set_points(home_border, border_points, 5);
      lv_obj_add_style(home_border, &style_border, 0);
      lv_obj_center(home_border);
      
      lv_style_init(&style_clock);
      lv_style_set_text_font(&style_clock, &lv_font_montserrat_32);
      lv_style_set_text_color(&style_clock, lv_palette_main(LV_PALETTE_BLUE_GREY));

      home_clock = lv_label_create(screen_home);
      lv_obj_add_style(home_clock, &style_clock, 0);
      lv_label_set_text(home_clock, "");
      lv_obj_align(home_clock, LV_ALIGN_BOTTOM_MID, 0, -10);

      home_meter = lv_meter_create(screen_home);
      lv_obj_set_size(home_meter,w,w);
      lv_obj_align(home_meter, LV_ALIGN_TOP_MID, 0, h/10);

      lv_obj_t *home_meter_border = lv_arc_create(screen_home);
      lv_obj_align(home_meter_border, LV_ALIGN_TOP_MID, 0, h*0.05);
      lv_obj_set_size(home_meter_border, w*0.95, w*0.95);
      lv_arc_set_rotation(home_meter_border, 0);
      lv_arc_set_bg_angles(home_meter_border, 0, 360);
      lv_arc_set_value(home_meter_border, 0);
      lv_obj_remove_style(home_meter_border, NULL, LV_PART_KNOB);
      lv_obj_clear_flag(home_meter_border, LV_OBJ_FLAG_CLICKABLE);

      lv_style_t fat_black_style;
      lv_style_init(&fat_black_style);
      lv_style_set_line_width(&fat_black_style, 20);
      lv_style_set_line_color(&fat_black_style, lv_color_black());
      lv_style_set_bg_grad_dir(&fat_black_style, LV_GRAD_DIR_NONE);
      lv_obj_add_style(home_meter_border, &fat_black_style, LV_PART_MAIN);
      lv_obj_add_style(home_meter_border, &fat_black_style, LV_PART_INDICATOR);

      lv_meter_scale_t *scale = lv_meter_add_scale(home_meter);
      lv_meter_set_scale_ticks(home_meter, scale, 31, 2, 10, lv_color_black());
      lv_meter_set_scale_major_ticks(home_meter, scale, 6, 2, 20, lv_color_black(),10);
      lv_meter_set_scale_range(home_meter, scale, 0, 100, 120, 210);

      home_legend = lv_label_create(screen_home);
      lv_label_set_text(home_legend, "uSv/hr");
      lv_obj_align(home_legend, LV_ALIGN_CENTER, 0, -40);

      lv_style_init(&style_legend);
      lv_style_set_text_font(&style_legend, &lv_font_montserrat_24);
      lv_style_set_text_color(&style_legend, lv_color_black());
      lv_obj_add_style(home_legend, &style_legend, 0);

      home_rate = lv_label_create(screen_home);
      lv_label_set_text(home_rate, "0000");
      lv_obj_align(home_rate, LV_ALIGN_CENTER, 0, 20);

      lv_style_init(&style_rate);
      lv_style_set_text_font(&style_rate, &lv_font_unscii_16);
      lv_style_set_text_color(&style_rate, lv_color_black());
      lv_obj_add_style(home_rate, &style_rate, 0);






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
  
  new CYB28Leaf("screen", 0),
  new LVGLExampleAppLeaf("app","screen,prefs"),
  NULL
};
