//
//@**************************** class SDCardLeaf *****************************
//
// Access to SD cards
//
#include <SD.h>
#include <SPI.h>

class SDCardLeaf : public FSLeaf
{
public:
  //
  // Declare your leaf-specific instance data here
  //
  uint8_t csPin;
  uint8_t sckPin;
  uint8_t mosiPin;
  uint8_t misoPin;
  uint8_t cardType;

  //
  // Leaf constructor method(s)
  // Call the superclass constructor to handle common arguments (type, name, pins)
  //
  SDCardLeaf(String name, fs::SDFS *fs=&SD, int csPin=SS, int sckPin=-1, int mosiPin=-1, int misoPin=-1 )
    : FSLeaf(name, (fs::FS *)fs)
    , Debuggable(name)
  {
    this->csPin = csPin;
    this->sckPin = sckPin;
    this->mosiPin = mosiPin;
    this->misoPin = misoPin;
  }

  virtual bool mount()
  {
    if (sckPin != -1) {
      SPI.begin(sckPin, misoPin, mosiPin, csPin);
    }

    if(!SD.begin(csPin)) {
      return false;
    }
    fs = &SD;
    mounted = true;

    cardType = SD.cardType();

    if(cardType == CARD_NONE){
      LEAF_ALERT("No SD card attached");
      return false;
    }

    String type;
    if(cardType == CARD_MMC){
      type = String("MMC");
    } else if(cardType == CARD_SD){
      type = String("SDSC");
    } else if(cardType == CARD_SDHC){
      type = String("SDHC");
    } else {
      type = String("UNKNOWN");
    }
    LEAF_NOTICE("SD card type: %s", type.c_str());

    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    LEAF_NOTICE("SD Card Size: %lluMB", cardSize);

    LEAF_NOTICE("Total space: %lluMB", SD.totalBytes() / (1024 * 1024));
    LEAF_NOTICE("Used space: %lluMB", SD.usedBytes() / (1024 * 1024));
    return true;
  }

};


// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
