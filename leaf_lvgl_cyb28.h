#pragma once
#include "leaf_lvgl_etft.h"
#include <TFT_Touch.h>

//
// The Cheap Yellow Board identifying itself as "2.8inch_ESP32-2432S028R"
// uses a separate SPI bus for its touch controller, which violates TFT_eSPI's
// assumption that the display and touch panel share the same SPI, differing only
// by Chip Select pin.
//
// This class wraps the quirks of this particular CYB
//

TFT_Touch *_cyb28_touch=NULL;

void _cyb28_touchpad_read( lv_indev_drv_t * indev_driver, lv_indev_data_t * data )
{
    uint16_t touchX, touchY;
    if (!_cyb28_touch) {
      data->state = LV_INDEV_STATE_REL;
      return;
    }

    bool touched = _cyb28_touch->Pressed();

    if( !touched )
    {
	data->state = LV_INDEV_STATE_REL;
    }
    else
    {
	touchX = _cyb28_touch->X();
	touchY = _cyb28_touch->Y();

	data->state = LV_INDEV_STATE_PR;

	/*Set the coordinates*/
	data->point.x = touchX;
	data->point.y = touchY;

	DEBUG("touch event x=%d y=%d", (int)touchX, (int)touchY);
    }
}

class CYB28Leaf : public LVGLeTFTLeaf
{
protected:
  int touch_dout = 39;
  int touch_din = 32;
  int touch_cs = 33;
  int touch_clk = 25;

  TFT_Touch *touch = NULL;
  lv_indev_drv_t cyb28_indev_drv;

public:
  CYB28Leaf(String name, uint8_t rotation=0)
    : LVGLeTFTLeaf(name, rotation)
    , Debuggable(name)
  {
  }

  virtual lv_indev_drv_t *get_indev_drv() { return &cyb28_indev_drv; }

  virtual void setup (void) {
    LVGLeTFTLeaf::setup();
    LEAF_ENTER(L_NOTICE);

    // TODO read settings

    LEAF_NOTICE("  tft touch setup");
    _cyb28_touch = touch = new TFT_Touch(touch_cs, touch_clk, touch_din, touch_dout);
    touch->setCal(526, 3443, 750, 3377, 320, 240, 1);
    touch->setRotation(rotation);

    LEAF_NOTICE("  LVGL input driver setup");
    // reject the superclass' input driver and substitute our own
    lv_indev_drv_init(&cyb28_indev_drv);
    cyb28_indev_drv.type = LV_INDEV_TYPE_POINTER;
    cyb28_indev_drv.read_cb = _cyb28_touchpad_read;
    lv_indev_drv_update( get_indev(), &cyb28_indev_drv );

    LEAF_LEAVE;
  }

  virtual void start(void)
  {
    LEAF_ENTER(L_NOTICE);
    LVGLLeaf::start();
    LEAF_LEAVE;
  }


};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
