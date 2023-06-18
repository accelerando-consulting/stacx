//
//@**************************** class CPUTempLeaf ******************************
// 
// This class encapsulates a temp sensor built into a CPU
//

#include "abstract_temp.h"

class CpuTempLeaf : public AbstractTempLeaf
{
public:
  
  CpuTempLeaf(String name)
    : AbstractTempLeaf(name, NO_PINS)
    , Debuggable(name)
  {
    sample_interval_ms = 5000;
    report_interval_sec = 600;
#ifdef ARDUINO_ESP32C3_DEV    
    temperature_sensor_handle_t temp_handle = NULL;
    temperature_sensor_config_t temp_sensor;
#endif
    
  void setup(void) {
    LEAF_ENTER(L_INFO);
    Leaf::setup();

#ifdef ARDUINO_ESP32C3_DEV    
    // ESP32-c3 has an api for temperature sensor
    ESP_ERROR_CHECK(temperature_sensor_install(&temp_sensor, &temp_handle));

    temp_sensor.range_min = -10;
    temp_sensor.range_max = 80;
    
    
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
    temperature_sensor_enable();
#endif

    LEAF_LEAVE;
  }

  void stop(void) 
  {
    LEAF_ENTER(L_NOTICE);
    temperature_sensor_disable();
    LEAF_LEAVE;
  }
  

  bool poll(float *h, float *t, const char **status) 
  {
#ifdef ARDUINO_ESP32C3_DEV
    float tsens_out;
    ESP_ERROR_CHECK(temperature_sensor_get_celsius(temp_handle, t));
    LEAF_NOTICE("CPU Temperature %.2f °C\n", *t);
#endif
    return true;
  }


};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
