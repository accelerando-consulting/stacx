#pragma once

#include "abstract_display.h"
#include "lvgl.h"

//@**************************** class LVGLLeaf ******************************

#if LV_USE_LOG
void stacx_lvgl_log(const char *buf)
{
  //WARN("LVGL: %s", buf);
  unsigned long now = millis();
  DBGPRINTF("#%4d.%03d   LVGL %s", (int)now/1000, (int)now%1000, buf);
}
#endif

// subclasses must define this
void stacx_lvgl_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p);
void stacx_lvgl_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data);

#ifndef LVGL_DEFAULT_ROTATION
#define LVGL_DEFAULT_ROTATION 0
#endif

class LVGLLeaf : public AbstractDisplayLeaf
{
protected:
  lv_disp_draw_buf_t draw_buf;
  lv_color_t *disp_draw_buf=NULL;
  lv_disp_drv_t disp_drv;
  lv_disp_t *disp = NULL;
  lv_indev_drv_t touch_indev_drv;
  lv_indev_t *touch_indev=NULL;
  bool use_touch=true;
  int lvgl_buffer_rows=40;

public:
  LVGLLeaf(String name, uint8_t rotation=LVGL_DEFAULT_ROTATION, bool use_touch=true)
    : AbstractDisplayLeaf(name, rotation)
    , Debuggable(name)
  {
    this->use_touch = use_touch;
  }

  virtual lv_disp_drv_t *get_disp_drv() { return &disp_drv; }
  virtual lv_indev_drv_t *get_indev_drv() { return &touch_indev_drv; }
  virtual lv_indev_t *get_indev() { return touch_indev; }

  virtual void setup(void);
  virtual void start(void);
  virtual void update(void);

  virtual void mqtt_do_subscribe();
  virtual void status_pub();
};

void LVGLLeaf::setup(void) {
  AbstractDisplayLeaf::setup();

  LEAF_ENTER(L_NOTICE);
  lv_init();
#if LV_USE_LOG == 1
  lv_log_register_print_cb(stacx_lvgl_log);
#endif

#ifdef LVGL_BUFFER_ROWS
  lvgl_buffer_rows = LVGL_BUFFER_ROWS;
#elif defined LVGL_BUFFER_FACTOR
  lvgl_buffer_rows = height * LVGL_BUFFER_FACTOR;
#endif

  LEAF_NOTICE("  allocate draw buffer: %d rows of %d (%d pixels)",
	      lvgl_buffer_rows, width, width*lvgl_buffer_rows);
#ifdef ESP32
  if (psramFound()) {
    LEAF_NOTICE("  (using SPIRAM for draw buffer)");
    disp_draw_buf = (lv_color_t *)heap_caps_malloc(sizeof(lv_color_t) * width * lvgl_buffer_rows, MALLOC_CAP_SPIRAM);
  }
  else {
    disp_draw_buf = (lv_color_t *)heap_caps_malloc(sizeof(lv_color_t) * width * lvgl_buffer_rows, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  }
#else
  disp_draw_buf = (lv_color_t *)malloc(sizeof(lv_color_t) * screenWidth * lvgl_buffer_rows);
#endif
  if (disp_draw_buf == NULL) {
    LEAF_ALERT("Draw buffer allocation failed");
  }

  lv_disp_draw_buf_init( &draw_buf, disp_draw_buf, NULL, width * lvgl_buffer_rows );

  LEAF_NOTICE("  init display driver");

  lv_disp_drv_init( &disp_drv );
  disp_drv.hor_res = width;
  disp_drv.ver_res = height;

  LEAF_NOTICE("  register display driver [ %d x %d ]", disp_drv.hor_res, disp_drv.ver_res);
  disp_drv.flush_cb = stacx_lvgl_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  disp = lv_disp_drv_register( &disp_drv );
  if (!disp) {
    LEAF_ALERT("Display driver register failed");
  }

  /*Initialize the input device driver*/
  if (use_touch) {
    LEAF_NOTICE("  register touch input driver");
    lv_indev_drv_init( &touch_indev_drv );
    touch_indev_drv.type = LV_INDEV_TYPE_POINTER;
    touch_indev_drv.read_cb = stacx_lvgl_touchpad_read;
    touch_indev = lv_indev_drv_register( &touch_indev_drv );
  }
  else {
    LEAF_NOTICE("  not registering touch driver");
  }

  LEAF_LEAVE;
}

void LVGLLeaf::start(void)
{
  AbstractDisplayLeaf::start();
  LEAF_ENTER(L_NOTICE);

  lv_obj_t *label = lv_label_create( lv_scr_act() );
  lv_label_set_text( label, device_id );
  lv_obj_align( label, LV_ALIGN_CENTER, 0, 0 );

  LEAF_LEAVE;
}

void LVGLLeaf::mqtt_do_subscribe() {
  LEAF_ENTER(L_DEBUG);
  AbstractDisplayLeaf::mqtt_do_subscribe();
  LEAF_LEAVE;
}

void LVGLLeaf::status_pub()
{
  LEAF_ENTER(L_DEBUG);
  AbstractDisplayLeaf::status_pub();
}

void LVGLLeaf::update()
{
  lv_timer_handler();
}






// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
