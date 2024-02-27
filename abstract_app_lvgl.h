#pragma once

#include "lvgl.h" // redundant, but aids static analysis

#ifdef CLASS_APP
// You must define CLASS_APP as the name of your app class, to use LVGL_EVENT_METHOD
class CLASS_APP;
#define LVGL_EVENT_HANDLER(handler) ([](lv_event_t *e){((CLASS_APP *)lv_event_get_user_data(e))->handler(e,lv_event_get_target(e),lv_event_get_code(e));})
#endif

class LVGLAppLeaf: public AbstractAppLeaf, public TraitLVGL
{
protected:

public:
  LVGLAppLeaf(String name,
	      String target=NO_TAPS,
	      void (*keypad_read_cb)(struct _lv_indev_drv_t * indev_drv, lv_indev_data_t * data)=NULL,
	      void (*encoder_read_cb)(struct _lv_indev_drv_t * indev_drv, lv_indev_data_t * data)=NULL
    )
    : AbstractAppLeaf(name,target)
    , TraitLVGL(name, keypad_read_cb, encoder_read_cb)
    , Debuggable(name)
  {
  }

  virtual void setup(void) 
  {
    AbstractAppLeaf::setup();
    LEAF_ENTER(L_NOTICE);

    trait_lvgl_setup((LVGLLeaf *)get_tap("screen"));
    
    LEAF_LEAVE;
  }

  virtual void start(void) 
  {
    AbstractAppLeaf::start();
    LEAF_ENTER(L_NOTICE);
    trait_lvgl_start();
    LEAF_LEAVE;
  }
  
};


// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
