#pragma once

#include <Wire.h>

#ifdef USE_OLED_U8x8
#include <U8x8lib.h>
#else
#include <SSD1306Wire.h>
#endif

#ifndef OLED_SDA
#define OLED_SDA SDA
#endif

#ifndef OLED_SCL
#define OLED_SCL SCL
#endif

#ifndef OLED_CLASS
#ifdef USE_OLED_U8x8
#define OLED_CLASS U8X8_SSD1306_64X48_ER_HW_I2C
#else
#define OLED_CLASS SSD1306Wire
#endif
#endif

void oled_text(int column, int row, const char *text) ;

#ifdef USE_OLED_U8x8
OLED_CLASS *_oled= NULL;
#else
OLED_CLASS *_oled = NULL;

#ifndef OLED_GEOMETRY
#define OLED_GEOMETRY GEOMETRY_128_32
#endif
#endif

int oled_textheight = 10;

bool oled_ready = false;

void oled_setup(void) 
{
  NOTICE("OLED setup");

  Wire.begin(SDA, SCL);
  Wire.setClock(100000);
  Wire.beginTransmission(0x3c);
  int error = Wire.endTransmission();
  if (error != 0) {
    ALERT("No response from I2C address 0x3c, presume OLED not present");
    _oled = NULL;
    return;
  }

#ifdef USE_OLED_U8x8
  _oled = new OLED_CLASS(/* reset=*/ U8X8_PIN_NONE) ;
  _oled->begin();
  _oled->setFont(u8x8_font_5x7_r);
  _oled->drawString(0,1,"Stacx");
#else
  _oled = new OLED_CLASS(0x3c, OLED_SDA, OLED_SCL, OLED_GEOMETRY);
  if (!_oled->init()) {
    ALERT("OLED failed");
    return;
  }
  WARN("OLED at 0x3c");

  _oled->clear();
  _oled->display();
  _oled->flipScreenVertically();
  _oled->setFont(ArialMT_Plain_10);
  //_oled->setFont(Monospaced_plain_8);
  //_oled->setFont(Monospaced_plain_6);
  _oled->setTextAlignment(TEXT_ALIGN_LEFT);

  oled_text(0,0,"Stacx");
  _oled->display();
#endif

  NOTICE("OLED ready");
  oled_ready = true;
}

void oled_text(int column, int row, const char *text) 
{
  if (!oled_ready) {
    // oled not present, log what it would have shown
    INFO("OLED TEXT @[%d,%d]: %s", row, column, text);
    return;
  }
  INFO("OLED TEXT @[%d,%d]: %s", row, column, text);
  
#ifdef USE_OLED_U8x8
  _oled->drawString(column,row,text);
#else
  OLEDDISPLAY_COLOR textcolor = _oled->getColor();
  _oled->setColor(BLACK);
  int textwidth = 128-column; //_oled->getStringWidth(text);
  //INFO("blanking %d,%d %d,%d for %s", column, row, oled_textheight+1, textwidth, text);
  _oled->fillRect(column, row+1, textwidth, oled_textheight+1);
  _oled->setColor(textcolor);
  _oled->drawString(column, row, text);
  _oled->display();
#endif
  
  DEBUG("TEXT done");
  DBGFLUSH();
}

void oled_text(int column, int row, String text) 
{
  oled_text(column, row, text.c_str());
}

// Local Variables:
// mode: C++
// c-basic-offset: 2
// End:
