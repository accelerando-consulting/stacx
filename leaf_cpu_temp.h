//
//@**************************** class CPUTempLeaf ******************************
// 
// This class encapsulates a temp sensor built into a CPU
//

#include "abstract_temp.h"
#ifdef ARDUINO_ESP32C3_DEV
#include <driver/temp_sensor.h>
#endif

class CpuTempLeaf : public AbstractTempLeaf
{
protected:
#ifdef ARDUINO_ESP32C3_DEV
  temp_sensor_config_t temp_sensor_config;
#endif

public:
  
  CpuTempLeaf(String name)
    : AbstractTempLeaf(name, NO_PINS)
    , Debuggable(name)
  {
    sample_interval_ms = 5000;
    report_interval_sec = 600;
#ifdef ARDUINO_ESP32C3_DEV
    temp_sensor_config.dac_offset = TSENS_DAC_L1;
    temp_sensor_config.clk_div = 6;
#endif
  }
    
  void setup(void) {
    LEAF_ENTER(L_INFO);
    Leaf::setup();

#ifdef ARDUINO_ESP32C3_DEV    
    temp_sensor_set_config(temp_sensor_config);
#else
    // TODO your board here?
    LEAF_ALERT("Unsupported board %s", ARDUINO_BOARD);
    stop();
#endif
    
    registerCommand(HERE,"poll", "poll the CPU temp sensor");
    LEAF_LEAVE;
  }

  
  void start(void) 
  {
    Leaf::start();
    LEAF_ENTER(L_INFO);

#ifdef ARDUINO_ESP32C3_DEV
    esp_err_t err = temp_sensor_start();
    if (err != ESP_OK) {
      LEAF_ALERT("Temperature sensor start failed: %d", (int)err);
    }
#endif

    LEAF_LEAVE;
  }

  void stop(void) 
  {
    LEAF_ENTER(L_NOTICE);
    esp_err_t err = temp_sensor_stop();
    if (err != ESP_OK) {
      LEAF_ALERT("Temperature sensor stop failed: %d", (int)err);
    }
    LEAF_LEAVE;
  }
  

  bool poll(float *h, float *t, const char **status) 
  {
#ifdef ARDUINO_ESP32C3_DEV
    esp_err_t err = temp_sensor_read_celsius(t);
    if (err != ESP_OK) {
      LEAF_ALERT("Temperature sensor read failed: %d", (int)err);
      return false;
    }
    LEAF_NOTICE("CPU Temperature %.2f °C\n", *t);
#endif
    return true;
  }


};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
