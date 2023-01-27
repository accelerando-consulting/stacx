//
//@************************* class PidExampleLeaf **************************
//
// This class encapsulates the Example pid loop
//
#include "abstract_pid.h"


class ExampleLeaf : public PidLeaf
{
protected:

public:
  ExampleLeaf(String name, String target, double p, double i, double d, int mode = P_ON_M, int direction = DIRECT, int sample_time = 500 )
    : PidLeaf(name, p, i, d, mode, direction, sample_time) {
    LEAF_ENTER(L_INFO);

    LEAF_LEAVE;
  }

  void setup(void) {
    PidLeaf::setup();
    LEAF_ENTER(L_INFO);

    this->start();
    LEAF_LEAVE;
  }

#ifdef NOTNEEDED
  void loop(void) {
    PidLeaf::loop();
    //LEAF_ENTER(L_NOTICE);
    //LEAF_LEAVE;
  }

  void start()
  {
    LEAF_ENTER(L_DEBUG);
    PidLeaf::start();
    LEAF_LEAVE;
  }
#endif

  virtual bool mqtt_receive(String type, String name, String topic, String payload, bool direct=false) {
    bool handled = false;

    LEAF_ENTER(L_DEBUG);

    WHEN("sometopic",{
	//LEAF_INFO("Received updated sometopic: %s", payload.c_str());
      })
    else {
      handled = PidLeaf::mqtt_receive(type, name, topic, payload, direct);
    }

    RETURN(handled);
  }

  void stop()
  {
    LEAF_ENTER(L_DEBUG);

    PidLeaf::stop();
    LEAF_LEAVE;
  }

};


// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
