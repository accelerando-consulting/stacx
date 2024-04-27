#pragma once

#include "leaf_lvgl.h"
#include <SPI.h>
#include <TFT_eSPI.h>

//@**************************** class LVGLLeaf ******************************

TFT_eSPI tftObj = TFT_eSPI(TFT_WIDTH,TFT_HEIGHT);

#ifndef LVGL_BUFFER_FACTOR
#define LVGL_BUFFFER_FACTOR 1/4
#endif

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


class LVGLeTFTLeaf : public LVGLLeaf
{
protected:
  TFT_eSPI *tft = NULL;

public:
  LVGLeTFTLeaf(String name, uint8_t rotation=0)
    : LVGLLeaf(name, rotation)
    , Debuggable(name)
  {
    LEAF_ENTER(L_NOTICE);
    if (rotation % 2) {
      width = TFT_HEIGHT;
      height = TFT_WIDTH;
    }
    else {
      width = TFT_WIDTH;
      height = TFT_HEIGHT;
    }
    LEAF_NOTICE("Screen size declared as width=%d height=%d", width, height);
    color = TFT_WHITE;
    tft = &tftObj;
    LEAF_LEAVE;
  }

  virtual lv_disp_drv_t *get_disp_drv() { return &disp_drv; }
  virtual lv_indev_drv_t *get_indev_drv() { return &touch_indev_drv; }
  virtual lv_indev_t *get_indev() { return touch_indev; }

  virtual void setup(void) {
    LEAF_ENTER(L_NOTICE);

    // Set up the LVGL library in superclass
    LVGLLeaf::setup();
  
    // Set up the underlying display hardware
    LEAF_NOTICE("tft init");
    tft->init();
#if !TFT_INVERSION_OFF    
    LEAF_NOTICE("tft invert");
    tft->invertDisplay(1);
#endif
    LEAF_NOTICE("tft setrotation %d", rotation);
    tft->setRotation(rotation);

    LEAF_NOTICE("tft color test");
    tft->fillScreen(TFT_RED);
    delay(400);
    tft->fillScreen(TFT_GREEN);
    delay(400);
    tft->fillScreen(TFT_BLUE);
    delay(400);
    tft->fillScreen(TFT_WHITE);
    delay(800);

    LEAF_NOTICE("tft home");
    tft->setCursor(5, 5);
    LEAF_NOTICE("tft setcolor");
    tft->setTextColor(TFT_BLACK);
    LEAF_NOTICE("tft setwrap");
    tft->setTextWrap(true);
    LEAF_NOTICE("tft print");
#ifdef BUILD_NUMBER
    char buf[32];
    snprintf(buf, sizeof(buf), "%s b%d", device_id, BUILD_NUMBER);
    LEAF_NOTICE("TFT hello banner [%s]", buf);
    tft->print(buf);
#else
    LEAF_NOTICE("TFT hello banner [%s]", device_id);
    tft->print(String(device_id));
#endif
    delay(500);

    LEAF_LEAVE;
  }

};




// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
