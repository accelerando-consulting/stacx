//
//@************************* class PidExampleLeaf **************************
// 
// This class encapsulates the Example pid loop
// 
#include "leaf_pid_abstract.h"


class ExampleLeaf : public PidLeaf
{
protected:
  
public:
  ExampleLeaf(String name, String target, double p, double i, double d, int mode = P_ON_M, int direction = DIRECT, int sample_time = 500 )
    : PidLeaf(name, p, i, d, mode, direction, sample_time) {
    ENTER(L_INFO);

    LEAVE;
  }

  void setup(void) {
    PidLeaf::setup();
    ENTER(L_NOTICE);

    this->start();
    LEAVE;
  }

#ifdef NOTNEEDED
  void loop(void) {
    PidLeaf::loop();
    //ENTER(L_NOTICE);
    //LEAVE;
  }

  void start() 
  {
    ENTER(L_DEBUG);
    PidLeaf::start();
    LEAVE;
  }
#endif
  
  bool mqtt_receive(String type, String name, String topic, String payload) {
    bool handled = PidLeaf::mqtt_receive(type, name, topic, payload);
    ENTER(L_DEBUG);

    WHEN("sometopic",{
	INFO("Received updated sometopic: %s", payload.c_str());
      })

    
    RETURN(handled);
  }

  void stop() 
  {
    ENTER(L_DEBUG);

    PidLeaf::stop();
    LEAVE;
  }

};


// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
