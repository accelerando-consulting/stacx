#pragma once

#include "lvgl.h" // redundant, but aids static analysis

#ifdef CLASS_APP
// You must define CLASS_APP as the name of your app class, to use LVGL_EVENT_METHOD
class CLASS_APP;
#define LVGL_EVENT_HANDLER(handler) ([](lv_event_t *e){((CLASS_APP *)lv_event_get_user_data(e))->handler(e,lv_event_get_target(e),lv_event_get_code(e));})
#endif
#define LVGL_EVENT_HANDLER_CLASS(handler,classname) ([](lv_event_t *e){((classname *)lv_event_get_user_data(e))->handler(e,lv_event_get_target(e),lv_event_get_code(e));})

#define LVGL_DECLARE_SCREEN(s) \
  lv_obj_t *screen_##s = NULL;	     \
  lv_group_t *group_##s = NULL
  

#define LVGL_CREATE_SCREEN(s) \
      screen_##s=lv_obj_create(NULL);		\
      group_##s=lv_group_create();		\
      lv_group_set_default(group_##s)
#define LVGL_SCREEN_EVENT(s,o,c) ((active_screen==(screen_##s)) && (obj == (s##_##o)) && (code==(LV_EVENT_##c))) 
#define LVGL_WHEN(s,o,c,block) if LVGL_SCREEN_EVENT(s,o,c) { block; }
#define LVGL_SCREEN_CHANGE(s) lv_scr_load(screen_##s); lvglAppEnableInputGroup(group_##s)


class TraitLVGL: virtual public Debuggable
{
protected:
  LVGLLeaf *screen=NULL;

  void (*keypad_read_cb)(struct _lv_indev_drv_t * indev_drv, lv_indev_data_t * data)=NULL;
  void (*encoder_read_cb)(struct _lv_indev_drv_t * indev_drv, lv_indev_data_t * data)=NULL;
  void (*default_event_handler)(lv_event_t *e)=NULL;

  int screen_width = -1;
  int screen_height = -1;

  lv_indev_drv_t keypad_indev_drv;
  lv_indev_t *keypad=NULL;

  lv_indev_drv_t encoder_indev_drv;
  lv_indev_t *encoder=NULL;

  lv_group_t *group_default = NULL;

public:
  TraitLVGL(String name,
	    void (*keypad_read_cb)(struct _lv_indev_drv_t * indev_drv, lv_indev_data_t * data)=NULL,
	    void (*encoder_read_cb)(struct _lv_indev_drv_t * indev_drv, lv_indev_data_t * data)=NULL
	  )
    : Debuggable(name)
  {
    this->keypad_read_cb = keypad_read_cb;
    this->encoder_read_cb = encoder_read_cb;
  }

  inline bool isInputEvent(lv_event_code_t code) 
  {
    return (code < LV_EVENT_COVER_CHECK);
  }
  
  const char *lvglEventName(lv_event_code_t code) 
  {
    static const char *event_names[_LV_EVENT_LAST] = {
      "LV_EVENT_ALL",                   // 0
      "LV_EVENT_PRESSED",
      "LV_EVENT_PRESSING",
      "LV_EVENT_PRESS_LOST",
      "LV_EVENT_SHORT_CLICKED",
      
      "LV_EVENT_LONG_PRESSED",          // 5
      "LV_EVENT_LONG_PRESSED_REPEAT",
      "LV_EVENT_CLICKED",
      "LV_EVENT_RELEASED",
      "LV_EVENT_SCROLL_BEGIN",
      
      "LV_EVENT_SCROLL_END",           // 10
      "LV_EVENT_SCROLL",
      "LV_EVENT_GESTURE",
      "LV_EVENT_KEY",
      "LV_EVENT_FOCUSED",
      
      "LV_EVENT_DEFOCUSED",            // 15
      "LV_EVENT_LEAVE",
      "LV_EVENT_HIT_TEST",
      
      /** Drawing events*/
      "LV_EVENT_COVER_CHECK",
      "LV_EVENT_REFR_EXT_DRAW_SIZE",
      "LV_EVENT_DRAW_MAIN_BEGIN",
      "LV_EVENT_DRAW_MAIN",
      "LV_EVENT_DRAW_MAIN_END",
      "LV_EVENT_DRAW_POST_BEGIN",
      "LV_EVENT_DRAW_POST",
      "LV_EVENT_DRAW_POST_END",
      "LV_EVENT_DRAW_PART_BEGIN",
      "LV_EVENT_DRAW_PART_END",
      
      /** Special events*/
      "LV_EVENT_VALUE_CHANGED",
      "LV_EVENT_INSERT",
      "LV_EVENT_REFRESH",
      "LV_EVENT_READY",
      "LV_EVENT_CANCEL",
      
      /** Other events*/
      "LV_EVENT_DELETE",
      "LV_EVENT_CHILD_CHANGED",
      "LV_EVENT_CHILD_CREATED",
      "LV_EVENT_CHILD_DELETED",
      "LV_EVENT_SCREEN_UNLOAD_START",
      "LV_EVENT_SCREEN_LOAD_START",
      "LV_EVENT_SCREEN_LOADED",
      "LV_EVENT_SCREEN_UNLOADED",
      "LV_EVENT_SIZE_CHANGED",
      "LV_EVENT_STYLE_CHANGED",
      "LV_EVENT_LAYOUT_CHANGED",
      "LV_EVENT_GET_SELF_SIZE",
    };
    return event_names[(int)code];
  }

  const char *lvglGestureName(uint8_t mask) 
  {
    // codes are not contiguous, because they are in fact a bitmask
    if (mask == LV_DIR_ALL) {
      return "LV_DIR_ALL";
    }
    else if (mask == LV_DIR_HOR) {
      return "LV_DIR_HOR";
    }
    else if (mask == LV_DIR_VER) {
      return "LV_DIR_VER";
    }
    else if (mask == LV_DIR_BOTTOM) {
      return "LV_DIR_BOTTOM";
    }
    else if (mask == LV_DIR_TOP) {
      return "LV_DIR_TOP";
    }
    else if (mask == LV_DIR_RIGHT) {
      return "LV_DIR_RIGHT";
    }
    else if (mask == LV_DIR_LEFT) {
      return "LV_DIR_LEFT";
    }
    else if (mask == LV_DIR_NONE) {
      return "LV_DIR_NONE";
    }
    else {
      return "LV_DIR_INVALD";
    }
  }

  virtual void lvglAppHandleEvent(lv_event_t *e, lv_obj_t *obj, lv_event_code_t code) 
  {
    // this should never get called
    LEAF_ENTER(L_ALERT);
    LEAF_LEAVE;
  }

  virtual void trait_lvgl_setup(LVGLLeaf *screen=NULL) 
  {
    LEAF_ENTER_PRETTY(L_NOTICE);

    if (screen) {
      this->screen = screen;
    }
    
    default_event_handler = [](lv_event_t *e){((TraitLVGL *)lv_event_get_user_data(e))->lvglAppHandleEvent(e,lv_event_get_target(e),lv_event_get_code(e));};

    if (keypad_read_cb) {
      LEAF_NOTICE("Set up keypad input device");
      lv_indev_drv_init(&keypad_indev_drv);
      keypad_indev_drv.type = LV_INDEV_TYPE_KEYPAD;
      keypad_indev_drv.read_cb = keypad_read_cb;
      keypad = lv_indev_drv_register(&keypad_indev_drv);
    }
    if (encoder_read_cb) {
      LEAF_NOTICE("Set up encoder input device");
      lv_indev_drv_init(&encoder_indev_drv);
      encoder_indev_drv.type = LV_INDEV_TYPE_ENCODER;
      encoder_indev_drv.read_cb = encoder_read_cb;
      encoder = lv_indev_drv_register(&encoder_indev_drv);
    }
    LEAF_LEAVE;
  }

  void lvglAppEnableInputGroup(lv_group_t *group) 
  {
    if (encoder) {
      lv_indev_set_group(encoder, group);
    }
    if (keypad) {
      lv_indev_set_group(keypad, group);
    }
  }

  virtual void trait_lvgl_start(void) 
  {
    LEAF_ENTER(L_NOTICE);
    
    if (screen) {
      screen_width = screen->getWidth();
      screen_height = screen->getHeight();
    
      group_default = lv_group_create();
      lv_group_set_default(group_default);
      lvglAppEnableInputGroup(group_default);
    }
    else {
      LEAF_ALERT("No screen link");
    }
     
    LEAF_LEAVE;
  }


  void lvglAppStyleInit(lv_style_t *style,
			const lv_font_t *font=NULL,
			lv_color_t *color=NULL,
			lv_color_t *bgcolor=NULL
    ) 
  {
    lv_style_init(style);
    if (font != NULL) {
      lv_style_set_text_font(style, font);
    }
    if (color) {
      lv_style_set_text_color(style, *color);
    }
    if (bgcolor) {
      lv_style_set_bg_color(style, *bgcolor);
    }
    
  }

  lv_obj_t *lvglAppLabelCreate(
    lv_obj_t *parent,
    lv_style_t *style = NULL,
    lv_align_t align = LV_ALIGN_CENTER,
    lv_coord_t x_ofs = 0,
    lv_coord_t y_ofs = 0,
    const char *text = NULL) 
  {
    lv_obj_t *o = lv_label_create(parent);
    if (style) {
      lv_obj_add_style(o, style, 0);
    }
    lv_label_set_text(o, (text!=NULL)?text:"");
    if (align != LV_ALIGN_DEFAULT) {
      lv_obj_align(o, align, x_ofs, y_ofs);
    }
    return o;
  }

  lv_obj_t *lvglAppLabelCreateRelative(
    lv_obj_t *parent,
    lv_obj_t *relative_to,
    lv_style_t *style = NULL,
    lv_align_t align = LV_ALIGN_OUT_RIGHT_MID,
    lv_coord_t x_ofs = 0,
    lv_coord_t y_ofs = 0,
    const char *text = NULL) 
  {
    lv_obj_t *o = lvglAppLabelCreate(parent, style, LV_ALIGN_DEFAULT, 0, 0, text);
    if (align != LV_ALIGN_DEFAULT) {
      lv_obj_align_to(o, relative_to, align, x_ofs, y_ofs);
    }
    return o;
  }

  lv_obj_t *lvglAppButtonCreate(
    lv_obj_t *parent,
    const char *text,
    lv_align_t align = LV_ALIGN_CENTER,
    lv_coord_t x_ofs = 0,
    lv_coord_t y_ofs = 0,
    void (*event_cb)(lv_event_t *e)=NULL,
    lv_event_code_t code = LV_EVENT_PRESSED
    )
  {
    lv_obj_t *btn = lv_btn_create(parent);
    if (align != LV_ALIGN_DEFAULT) {
      lv_obj_align(btn, align, x_ofs, y_ofs);
    }

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_center(lbl);
    if (event_cb != NULL) {
      lv_obj_add_event_cb(btn, event_cb, code, this);
    }
    else {
      lv_obj_add_event_cb(btn, default_event_handler, code, this);
    }
      
    return btn;
  }

  lv_obj_t *lvglAppButtonCreateRelative(
    lv_obj_t *parent,
    lv_obj_t *relative_to,
    const char *text,
    lv_align_t align = LV_ALIGN_OUT_RIGHT_MID,
    lv_coord_t x_ofs = 0,
    lv_coord_t y_ofs = 0,
    void (*event_cb)(lv_event_t *e)=NULL,
    lv_event_code_t code = LV_EVENT_PRESSED
    )
  {
    lv_obj_t *btn = lvglAppButtonCreate(parent, text, LV_ALIGN_DEFAULT, 0, 0, event_cb, code);
    if (align != LV_ALIGN_DEFAULT) {
      lv_obj_align_to(btn, relative_to, align, x_ofs, y_ofs);
    }
    return btn;
  }

  lv_obj_t *lvglAppSpinboxCreate(
    lv_obj_t *parent,
    const char *title,
    lv_obj_t *relative_to=NULL,
    lv_align_t align = LV_ALIGN_DEFAULT,
    lv_coord_t x_ofs = 0,
    lv_coord_t y_ofs = 0,
    lv_obj_t **minus_btn=NULL,
    lv_obj_t **plus_btn=NULL,
    lv_obj_t **label_r=NULL,
    int value=-1,
    int label_width=-1)
  {
    lv_obj_t *spinbox, *label, *o;
    o = label = lv_label_create(parent);
    if (label_r) {
      *label_r = label;
    }
    lv_label_set_text(o, title);
    if (!relative_to && align != LV_ALIGN_DEFAULT) {
      lv_obj_align(o, align, x_ofs, y_ofs);
    }
    if (relative_to) {
      lv_obj_align_to(o, relative_to, align, x_ofs, y_ofs);
    }
    if (label_width == -1) {
      label_width = lv_obj_get_width(label);
    }

    o = spinbox = lv_spinbox_create(parent);
    lv_spinbox_set_range(spinbox, 0, 999999);
    lv_spinbox_set_digit_format(spinbox, 6, 0);
    lv_spinbox_step_prev(spinbox);
    lv_obj_set_width(spinbox, screen_width/3);
    if (value != -1) {
      lv_spinbox_set_value(spinbox, value);
    }
    lv_coord_t h = lv_obj_get_height(spinbox) * 0.75;
    lv_obj_align_to(spinbox, label, LV_ALIGN_TOP_LEFT, label_width+10+h, -10 );

    o = lv_btn_create(parent);
    if (plus_btn) {
      *plus_btn = o;
    }
    lv_obj_set_size(o, h, h);
    lv_obj_align_to(o, spinbox, LV_ALIGN_OUT_RIGHT_MID, 5, 0);
    lv_obj_add_event_cb(o, default_event_handler, LV_EVENT_PRESSED,  this);
    lv_obj_t *l = lv_label_create(o);
    lv_label_set_text(l, LV_SYMBOL_PLUS);
    lv_obj_center(l);

    o = lv_btn_create(parent);
    if (minus_btn) {
      *minus_btn = o;
    }
    lv_obj_set_size(o, h, h);
    lv_obj_align_to(o, spinbox, LV_ALIGN_OUT_LEFT_MID, -5, 0);
    lv_obj_add_event_cb(o, default_event_handler, LV_EVENT_PRESSED, this);
    l = lv_label_create(o);
    lv_label_set_text(l, LV_SYMBOL_MINUS);
    lv_obj_center(l);

    return spinbox;
  }


  lv_obj_t *lvglAppDropdownCreate(
    lv_obj_t *parent,
    const char *title,
    lv_obj_t *relative_to=NULL,
    lv_align_t align = LV_ALIGN_DEFAULT,
    lv_coord_t x_ofs = 0,
    lv_coord_t y_ofs = 0,
    lv_obj_t **label_r=NULL,
    const char *options = NULL,
    int selected = -1,
    int width = -1,
    int label_width=-1)
  {
    lv_obj_t *dropdown, *label, *o;
    o = label = lv_label_create(parent);
    if (label_r) {
      *label_r = label;
    }
    lv_label_set_text(o, title);
    if (!relative_to && align != LV_ALIGN_DEFAULT) {
      lv_obj_align(o, align, x_ofs, y_ofs);
    }
    if (relative_to) {
      lv_obj_align_to(o, relative_to, align, x_ofs, y_ofs);
    }
    if (label_width == -1) {
      label_width = lv_obj_get_width(label);
    }

    o = dropdown = lv_dropdown_create(parent);
    if (width == -1) {
      width = lv_pct(75);
    }
    lv_obj_set_width(dropdown, width);
    lv_obj_align_to(dropdown, label, LV_ALIGN_TOP_LEFT, label_width+5, -5);

    if (options != NULL) {
      lv_dropdown_set_options(dropdown, options);
    }
    if (selected != -1) {
      lv_dropdown_set_selected(dropdown, selected);
    }
    
    return dropdown;
  }

  lv_obj_t *lvglAppTextLineCreate(
    lv_obj_t *parent,
    const char *title,
    lv_obj_t *relative_to=NULL,
    lv_align_t align = LV_ALIGN_DEFAULT,
    lv_coord_t x_ofs = 0,
    lv_coord_t y_ofs = 0,
    lv_obj_t **label_r=NULL,
    const char *value = NULL,
    int width = -1,
    int label_width=-1)
  {
    lv_obj_t *textbox, *label, *o;
    o = label = lv_label_create(parent);
    if (label_r) {
      *label_r = label;
    }
    lv_label_set_text(o, title);
    if (!relative_to && align != LV_ALIGN_DEFAULT) {
      lv_obj_align(o, align, x_ofs, y_ofs);
    }
    if (relative_to) {
      lv_obj_align_to(o, relative_to, align, x_ofs, y_ofs);
    }
    if (label_width == -1) {
      label_width = lv_obj_get_width(label);
    }

    o = textbox = lv_textarea_create(parent);
    lv_textarea_set_one_line(textbox, true);
    lv_textarea_set_password_mode(textbox, false);
    if (width == -1) {
      width = lv_pct(75);
    }
    lv_obj_set_width(textbox, width);
    lv_obj_align_to(textbox, label, LV_ALIGN_TOP_LEFT, label_width+5, -5);

    if (value != NULL) {
      lv_textarea_set_text(textbox, value);
    }

    
    return textbox;
  }

  lv_obj_t *lvglAppTextValueCreate(
    lv_obj_t *parent,
    const char *title,
    lv_obj_t *relative_to=NULL,
    lv_style_t *style = NULL,
    lv_align_t align = LV_ALIGN_DEFAULT,
    lv_coord_t x_ofs = 0,
    lv_coord_t y_ofs = 0,
    lv_obj_t **label_r=NULL,
    const char *value = NULL,
    int width = -1,
    int label_width=-1)
  {
    lv_obj_t *textvalue, *label, *o;
    o = label = lv_label_create(parent);
    if (label_r) {
      *label_r = label;
    }
    lv_label_set_text(o, title);
    if (!relative_to && align != LV_ALIGN_DEFAULT) {
      lv_obj_align(o, align, x_ofs, y_ofs);
    }
    if (relative_to) {
      lv_obj_align_to(o, relative_to, align, x_ofs, y_ofs);
    }
    if (label_width == -1) {
      label_width = lv_obj_get_width(label);
    }

    o = textvalue = lv_label_create(parent);
    lv_obj_align_to(textvalue, label, LV_ALIGN_TOP_LEFT, label_width+5, -5);
    if (style != NULL) {
      lv_obj_add_style(textvalue, style, 0);
    }
      
    if (value != NULL) {
      lv_label_set_text(textvalue, value);
    }

    return textvalue;
  }

  lv_obj_t *lvglAppSwitchCreate(
    lv_obj_t *parent,
    const char *title,
    lv_obj_t *relative_to=NULL,
    lv_align_t align = LV_ALIGN_DEFAULT,
    lv_coord_t x_ofs = 0,
    lv_coord_t y_ofs = 0,
    lv_obj_t **label_r=NULL,
    int value = -1,
    int label_width=-1,
    void (*event_cb)(lv_event_t *e)=NULL,
    lv_event_code_t code = LV_EVENT_PRESSED
)
  {
    lv_obj_t *sw, *label, *o;
    o = label = lv_label_create(parent);
    if (label_r) {
      *label_r = label;
    }
    lv_label_set_text(o, title);
    if (!relative_to && align != LV_ALIGN_DEFAULT) {
      lv_obj_align(o, align, x_ofs, y_ofs);
    }
    if (relative_to) {
      lv_obj_align_to(o, relative_to, align, x_ofs, y_ofs);
    }
    if (label_width == -1) {
      label_width = lv_obj_get_width(label);
    }

    o = sw = lv_switch_create(parent);
    if (value == 1) {
      lv_obj_add_state(sw, LV_STATE_CHECKED);
    }
    lv_obj_align_to(sw, label, LV_ALIGN_TOP_LEFT, label_width+5, -5);

    if (event_cb != NULL) {
      lv_obj_add_event_cb(sw, event_cb, code, this);
    }
    else {
      lv_obj_add_event_cb(sw, default_event_handler, code, this);
    }

    return sw;
  }

  
};


// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
