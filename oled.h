#include <Wire.h>
#include <SSD1306Wire.h>

void oled_text(int column, int row, const char *text) ;

SSD1306Wire *_oled = NULL;
int oled_textheight = 10;

void oled_setup(void) 
{
  NOTICE("OLED setup");
  _oled = new SSD1306Wire(0x3c, SDA, SCL, GEOMETRY_128_32);
  if (!_oled->init()) {
    ALERT("OLED failed");
    return;
  }
  Wire.setClock(100000);
  _oled->clear();
  _oled->display();
  _oled->flipScreenVertically();
  //_oled->setFont(ArialMT_Plain_10);
  _oled->setFont(Monospaced_plain_8);
  //_oled->setFont(Monospaced_plain_6);
  _oled->setTextAlignment(TEXT_ALIGN_LEFT);

  oled_text(0,0,"Stacx");
}

void oled_text(int column, int row, const char *text) 
{
  DEBUG("TEXT @[%d,%d]: %s", row, column, text);
  OLEDDISPLAY_COLOR textcolor = _oled->getColor();
  _oled->setColor(BLACK);
  int textwidth = 128-column; //_oled->getStringWidth(text);
  //INFO("blanking %d,%d %d,%d for %s", column, row, oled_textheight+1, textwidth, text);
  _oled->fillRect(column, row, textwidth, oled_textheight);
  _oled->setColor(textcolor);
  _oled->drawString(column, row, text);
  _oled->display();
  DEBUG("TEXT done");
  Serial.flush();
}

void oled_text(int column, int row, String text) 
{
  oled_text(column, row, text.c_str());
}

// Local Variables:
// mode: C++
// c-basic-offset: 2
// End:
