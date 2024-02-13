#pragma once

#ifndef TFT_WIDTH
#define TFT_WIDTH 800
#endif

#ifndef TFT_HEIGHT
#define TFT_HEIGHT 480
#endif

#include "leaf_lvgl.h"


//
// The Cheap Yellow Board identifying itself as "4.3inch_ESP32-8048S043"
//

#define TOUCH_GT911
#define TOUCH_GT911_SCL 20
#define TOUCH_GT911_SDA 19
#define TOUCH_GT911_INT 255
#define TOUCH_GT911_RST 38
#define TOUCH_GT911_ROTATION ROTATION_INVERTED
#define TOUCH_MAP_X1 0
#define TOUCH_MAP_X2 272
#define TOUCH_MAP_Y1 0
#define TOUCH_MAP_Y2 480

#include <Arduino_GFX_Library.h>

#define ESP32_8048S043
#define GFX_DEV_DEVICE ESP32_8048S043
#define GFX_BL 2

// touch code presumes a global named gfx.  The stupid it burns.
Arduino_RGB_Display *gfx = NULL;

// TODO: make these static class members
int cyb43_rotation = 1;
int cyb43_touch_last_x = 0, cyb43_touch_last_y = 0;

#include <Wire.h>
#include <TAMC_GT911.h>
TAMC_GT911 cyb43_ts = TAMC_GT911(TOUCH_GT911_SDA, TOUCH_GT911_SCL, TOUCH_GT911_INT, TOUCH_GT911_RST, max(TOUCH_MAP_X1, TOUCH_MAP_X2), max(TOUCH_MAP_Y1, TOUCH_MAP_Y2));

void cyb43_touch_init()
{
  WARN("cyb43_touch_init");
  //Wire.begin(TOUCH_GT911_SDA, TOUCH_GT911_SCL);
  cyb43_ts.begin();
  cyb43_ts.setRotation(TOUCH_GT911_ROTATION);
}

bool cyb43_touch_has_signal()
{
  return true;
}

bool cyb43_touch_touched()
{
  cyb43_ts.read();
  if (cyb43_ts.isTouched)
  {
    int tx,ty;
    if (cyb43_rotation % 2) {
      tx = cyb43_ts.points[0].y;
      ty = cyb43_ts.points[0].x;
    }
    else {
      tx = cyb43_ts.points[0].x;
      ty = cyb43_ts.points[0].y;
    }
    
    cyb43_touch_last_x = map(tx, TOUCH_MAP_X1, TOUCH_MAP_X2, 0, gfx->width() - 1);
    cyb43_touch_last_y = map(ty, TOUCH_MAP_Y1, TOUCH_MAP_Y2, 0, gfx->height() - 1);
    /*Serial.printf("  Touch coord range [ %d:%d, %d:%d ], screen range [ 0:%d , 0:%d ]\n",
		  TOUCH_MAP_X1, TOUCH_MAP_X2, 
		  TOUCH_MAP_Y1, TOUCH_MAP_Y2,
		  gfx->width()-1,
		  gfx->height()-1);*/
    Serial.printf("  Touch [ %d, %d ] => screen [ %d , %d ]\n",
		  tx, ty, cyb43_touch_last_x, cyb43_touch_last_y);
    
    return true;
  }
  else
  {
    return false;
  }
}

bool cyb43_touch_released()
{
  return true;
}


void stacx_lvgl_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

#if (LV_COLOR_16_SWAP != 0)
  gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#else
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#endif

  lv_disp_flush_ready(disp);
}

void stacx_lvgl_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
{
  if (cyb43_touch_has_signal())
  {
    if (cyb43_touch_touched())
    {
      data->state = LV_INDEV_STATE_PR;

      /*Set the coordinates*/
      data->point.x = cyb43_touch_last_x;
      data->point.y = cyb43_touch_last_y;
    }
    else if (cyb43_touch_released())
    {
      data->state = LV_INDEV_STATE_REL;
    }
  }
  else
  {
    data->state = LV_INDEV_STATE_REL;
  }
}


class CYB43Leaf : public LVGLLeaf
{
protected:
  int touch_dout = 39;
  int touch_din = 32;
  int touch_cs = 33;
  int touch_clk = 25;

  Arduino_ESP32RGBPanel *rgbpanel = NULL;
  Arduino_RGB_Display *gfx = NULL;

  lv_indev_drv_t cyb43_indev_drv;

public:
  CYB43Leaf(String name, uint8_t rotation=255)
    : LVGLLeaf(name, rotation)
    , Debuggable(name)
  {
    if (rotation != 255) {
      ::cyb43_rotation = rotation;
    }
  }

  virtual lv_indev_drv_t *get_indev_drv() { return &cyb43_indev_drv; }

  virtual void setup (void) {
    LEAF_ENTER(L_NOTICE);

    // TODO read settings

    LEAF_NOTICE("  GFX allocations");

    rgbpanel = new Arduino_ESP32RGBPanel(
      40 /* DE */, 41 /* VSYNC */, 39 /* HSYNC */, 42 /* PCLK */,
      45 /* R0 */, 48 /* R1 */, 47 /* R2 */, 21 /* R3 */, 14 /* R4 */,
      5 /* G0 */, 6 /* G1 */, 7 /* G2 */, 15 /* G3 */, 16 /* G4 */, 4 /* G5 */,
      8 /* B0 */, 3 /* B1 */, 46 /* B2 */, 9 /* B3 */, 1 /* B4 */,
      0 /* hsync_polarity */, 8 /* hsync_front_porch */, 4 /* hsync_pulse_width */, 8 /* hsync_back_porch */,
      0 /* vsync_polarity */, 8 /* vsync_front_porch */, 4 /* vsync_pulse_width */, 8 /* vsync_back_porch */,
      1 /* pclk_active_neg */, 16000000 /* prefer_speed */);

    ::gfx = gfx = new Arduino_RGB_Display(
      800 /* width */, 480 /* height */, rgbpanel, 0 /* rotation */, true /* auto_flush */);

#ifdef GFX_EXTRA_PRE_INIT
    LEAF_NOTICE("  GFX extra pre init");
    GFX_EXTRA_PRE_INIT();
#endif

    LEAF_NOTICE("  GFX panel setup");
    if (!gfx->begin())
    {
      LEAF_ALERT("gfx->begin() failed!");
      stop();
      return;
    }

#ifdef GFX_BL
    LEAF_NOTICE("  GFX backlight enable");
    pinMode(GFX_BL, OUTPUT);
    digitalWrite(GFX_BL, HIGH);
#endif

    gfx->setRotation(rotation);
    if (rotation % 2) {
      width = gfx->width();
      height = gfx->height();
    }
    else {
      width = gfx->height();
      height = gfx->width();
    }

    LEAF_NOTICE("  Screen test %d x %d", width, height);
    gfx->fillScreen(RED);
    delay(250);
    gfx->fillScreen(GREEN);
    delay(250);
    gfx->fillScreen(BLUE);
    delay(250);
    gfx->fillScreen(WHITE);

    gfx->setCursor(20, 20);
    gfx->setTextSize(3);
    gfx->setTextColor(BLACK);
    gfx->println(device_id);
    delay(750);

    LEAF_NOTICE("  LVGL init");
    LVGLLeaf::setup();

    debug_flush=1;
    debug_wait=100;
    
    LEAF_NOTICE("  LVGL input driver setup");
    cyb43_touch_init();

    LEAF_LEAVE;
  }

  virtual void start(void)
  {
    LVGLLeaf::start();
    LEAF_ENTER(L_NOTICE);


#ifdef CANVAS
    LEAF_NOTICE("Flush canvas");
    gfx->flush();
#endif

    LEAF_LEAVE;
  }


};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
