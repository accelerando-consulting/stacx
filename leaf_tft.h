
#include <TFT_eSPI.h>
#include "abstract_display.h"

//@***************************** class TFTLeaf ******************************

TFT_eSPI tftObj = TFT_eSPI(TFT_WIDTH,TFT_HEIGHT);

class TFTLeaf : public AbstractDisplayLeaf
{
  TFT_eSPI *tft = NULL;

public:
  TFTLeaf(String name, uint8_t rotation=0)
    : AbstractDisplayLeaf(name, rotation)
    , Debuggable(name)
  {
    width = TFT_WIDTH;
    height = TFT_HEIGHT;
    color = TFT_WHITE;
  }

  virtual void setup(void) {
    AbstractDisplayLeaf::setup();
    debug_flush=true;
    LEAF_ENTER(L_NOTICE);
    //this->tft = new TFT_eSPI();
    this->tft = &tftObj;
    LEAF_NOTICE("tft=%p", tft);
    
    LEAF_NOTICE("tft init");
    tft->init();
    //LEAF_NOTICE("tft invert");
    //tft->invertDisplay(1);
    LEAF_NOTICE("tft setrotation %d", rotation);
    tft->setRotation(rotation);
    LEAF_NOTICE("tft clear");
    tft->fillScreen(TFT_BLACK);
    LEAF_NOTICE("tft home");
    tft->setCursor(5, 5);
    LEAF_NOTICE("tft setcolor");
    tft->setTextColor(color);
    LEAF_NOTICE("tft setwrap");
    tft->setTextWrap(true);
    LEAF_NOTICE("tft print");
#ifdef BUILD_NUMBER
    char buf[32];
    snprintf(buf, sizeof(buf), "%s b%d", device_id, BUILD_NUMBER);
    tft->print(buf);
#else
    tft->print(String(device_id));
#endif
    
    LEAF_NOTICE("%s is %dx%d", base_topic.c_str(), width, height);
    debug_flush = false;
    
    LEAF_LEAVE;
  }

protected:
  
  virtual void setAlignment(String payload)
  {
    LEAF_ENTER(L_DEBUG);

    if (payload.equals("left")) {
      tft->setTextDatum(TL_DATUM);
    }
    else if (payload.equals("right")) {
      tft->setTextDatum(TR_DATUM);
    }
    else if (payload.equals("both")) {
      tft->setTextDatum(MC_DATUM);
    }
    else {
      // default is center
      tft->setTextDatum(TC_DATUM);
    }
  }

  virtual void setFont(int font)
  {
    LEAF_ENTER(L_DEBUG);

    switch (font) {
    case 48:
      tft->setTextFont(6);
      textheight=26;
      break;
    case 24:
    case 26:
      tft->setTextFont(4);
      textheight=26;
      break;
    case 18: 
    case 16: 
      tft->setTextFont(2);
      textheight=16;
      break;
    case 10:
    case 9:
    case 8:
    default:
      tft->setTextFont(1);
      textheight=8;
      break;
    }
  }

  virtual void drawLine(int x1, int y1, int x2, int y2, uint32_t color) 
  {
    tft->drawLine(x1, y1, x2, y2, color) ;
  }

  virtual void drawString(const char *txt, int column, int row) 
  {
    tft->drawString(txt, column, row);
  }
};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
