//
//@**************************** class Max6675Leaf ******************************
// 
// This class encapsulates a K-type thermocoule temperature sensor using the
// MAX6675 chip.
//

// adafruit library
// #include "max6675.h"

// YuriiSalimov library
#include <Thermocouple.h>
#include <MAX6675_Thermocouple.h>



#include "abstract_temp.h"

class Max6675Leaf : public AbstractTempLeaf
{
private:
  // adafruit lib
  // MAX6675 *thermocouple = NULL;

  // salimov lib
  MAX6675_Thermocouple *thermocouple = NULL;

  int pinCLK;
  int pinCS;
  int pinDO;
  
public:
  Max6675Leaf(String name, int cs=5, int clk=18, int miso=19) : AbstractTempLeaf(name, NO_PINS) {
    LEAF_ENTER(L_INFO);
    pinCLK=clk;
    pinCS=cs;
    pinDO=miso;
    LEAF_LEAVE;
  }

  void setup(void) {
    LEAF_ENTER(L_INFO);
    Leaf::setup();

    // adafruit
    //thermocouple = new MAX6675(pinCLK, pinCS, pinDO);

    // Salimov
    thermocouple = new MAX6675_Thermocouple(pinCLK, pinCS, pinDO);
    
    LEAF_LEAVE;
  }

  bool poll(float *h, float *t, const char **status) 
  {
    LEAF_INFO("Sampling MAX6675");
    // time to take a new sample
    *t = thermocouple->readCelsius();
    *status = "ok";
    LEAF_INFO("t=%f (%s)", *t, *status);
    return true;
  }

};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
