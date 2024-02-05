#include "abstract_display.h"
#include "lvgl.h"

//@**************************** class LVGLLeaf ******************************

void stacx_lvgl_log(const char *buf)
{
  NOTICE("LVGL: %s", buf);
}

// subclasses must define this
void stacx_lvgl_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p);
void stacx_lvgl_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data);

#ifndef LVGL_BUFFER_ROWS
#define LVGL_BUFFER_ROWS 40
#endif

#ifndef LVGL_BUFFER_FACTOR
#define LVGL_BUFFFER_FACTOR 1/4
#endif

class LVGLLeaf : public AbstractDisplayLeaf
{
protected:
  lv_disp_draw_buf_t draw_buf;
  lv_color_t *disp_draw_buf=NULL;
  lv_disp_drv_t disp_drv;
  lv_indev_drv_t touch_indev_drv;
  lv_indev_t *touch_indev;

public:
  LVGLLeaf(String name, uint8_t rotation=0)
    : AbstractDisplayLeaf(name, rotation)
    , Debuggable(name)
  {
  }

  virtual lv_disp_drv_t *get_disp_drv() { return &disp_drv; }
  virtual lv_indev_drv_t *get_indev_drv() { return &touch_indev_drv; }
  virtual lv_indev_t *get_indev() { return touch_indev; }

  virtual void setup(void);
  virtual void start(void);
  virtual void loop();

  virtual void mqtt_do_subscribe();
  virtual void status_pub();
};

void LVGLLeaf::setup(void) {
  AbstractDisplayLeaf::setup();

  LEAF_ENTER(L_NOTICE);
  lv_init();

  LEAF_NOTICE("Draw buffer size will be %d rows of %d (%d pixels)",
	      LVGL_BUFFER_ROWS, width, width*LVGL_BUFFER_ROWS);
#ifdef ESP32
  disp_draw_buf = (lv_color_t *)heap_caps_malloc(sizeof(lv_color_t) * width * LVGL_BUFFER_ROWS, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#else
  disp_draw_buf = (lv_color_t *)malloc(sizeof(lv_color_t) * screenWidth * LVGL_BUFFER_ROWS);
#endif

  lv_disp_draw_buf_init( &draw_buf, disp_draw_buf, NULL, width * LVGL_BUFFER_ROWS );
  lv_disp_drv_init( &disp_drv );
  disp_drv.hor_res = width;
  disp_drv.ver_res = height;

  LEAF_NOTICE("Display driver resolution %d x %d", disp_drv.hor_res, disp_drv.ver_res);


  disp_drv.flush_cb = stacx_lvgl_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register( &disp_drv );

  /*Initialize the input device driver*/
  lv_indev_drv_init( &touch_indev_drv );
  touch_indev_drv.type = LV_INDEV_TYPE_POINTER;
  touch_indev_drv.read_cb = stacx_lvgl_touchpad_read;
  touch_indev = lv_indev_drv_register( &touch_indev_drv );

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

void LVGLLeaf::loop()
{
#if 0
  static unsigned long last=0;
  unsigned long now=millis();
  if (now != last) {
    if (now > last) {
      lv_tick_inc(now-last);
    }
    last=now;
  }
#endif

  lv_timer_handler(); /* let the GUI do its work */
  AbstractDisplayLeaf::loop();
}




// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
