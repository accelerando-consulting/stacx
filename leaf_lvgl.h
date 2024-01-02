
#include <SPI.h>
#include <TFT_eSPI.h>
#include "lvgl.h"
#include "leaf_tft.h"

//@**************************** class LVGLLeaf ******************************

void stacx_lvgl_log(const char *buf) 
{
  NOTICE("LVGL: %s", buf);
}


/* Display flushing */
void stacx_lvgl_disp_flush( lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p )
{
  uint32_t w = ( area->x2 - area->x1 + 1 );
  uint32_t h = ( area->y2 - area->y1 + 1 );
  //NOTICE("LVGL flush x=%d y=%d w=%d h=%d", (int)area->x1, (int)area->y1, (int)w, (int)h);
    
  tftObj.startWrite();
  tftObj.setAddrWindow( area->x1, area->y1, w, h );
  tftObj.pushColors( ( uint16_t * )&color_p->full, w * h, true );
  tftObj.endWrite();
  
  lv_disp_flush_ready( disp );
}

/*Read the touchpad*/
void stacx_lvgl_touchpad_read( lv_indev_drv_t * indev_driver, lv_indev_data_t * data )
{
    data->state = LV_INDEV_STATE_REL;
}


class LVGLLeaf : public TFTLeaf
{
protected:
  lv_disp_draw_buf_t draw_buf;
  lv_color_t buf[ TFT_WIDTH * TFT_HEIGHT / 4 ];
  lv_disp_drv_t disp_drv;
  lv_indev_drv_t touch_indev_drv;

public:
  LVGLLeaf(String name, uint8_t rotation=0)
    : TFTLeaf(name, rotation)
    , Debuggable(name)
  {
  }

  virtual void setup(void);
  virtual void start(void);
  virtual void loop();
  
  virtual void mqtt_do_subscribe();
  virtual void status_pub();
};

void LVGLLeaf::setup(void) {
  TFTLeaf::setup();

  LEAF_ENTER(L_NOTICE);
  lv_init();

  lv_disp_draw_buf_init( &draw_buf, buf, NULL, width * 10 );
  lv_disp_drv_init( &disp_drv );

  if (rotation % 2) {
    disp_drv.ver_res = width;
    disp_drv.hor_res = height;
  }
  else {
    disp_drv.hor_res = width;
    disp_drv.ver_res = height;
  }
  
  disp_drv.flush_cb = stacx_lvgl_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register( &disp_drv );

  /*Initialize the (dummy) input device driver*/
  lv_indev_drv_init( &touch_indev_drv );
  touch_indev_drv.type = LV_INDEV_TYPE_POINTER;
  touch_indev_drv.read_cb = stacx_lvgl_touchpad_read;
  lv_indev_drv_register( &touch_indev_drv );

  LEAF_LEAVE;
}

void LVGLLeaf::start(void) 
{
  TFTLeaf::start();
  LEAF_ENTER(L_NOTICE);
  
  lv_obj_t *label = lv_label_create( lv_scr_act() );
  lv_label_set_text( label, device_id );
  lv_obj_align( label, LV_ALIGN_CENTER, 0, 0 );

  LEAF_LEAVE;
}

void LVGLLeaf::mqtt_do_subscribe() {
  LEAF_ENTER(L_DEBUG);
  TFTLeaf::mqtt_do_subscribe();
  LEAF_LEAVE;
}

void LVGLLeaf::status_pub()
{
  LEAF_ENTER(L_DEBUG);
  TFTLeaf::status_pub();
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
  TFTLeaf::loop();
}




// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
