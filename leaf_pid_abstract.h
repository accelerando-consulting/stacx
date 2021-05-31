// 
// This class encapsulates an abstract pid loop
//

#include <PID_v1.h>

class PidLeaf : public Leaf
{
protected:
  bool run = false;
  String target;
  double input, output, setpoint;
  double p, i, d;
  int mode, direction, sample_time;
  PID *pid;

public:
  PidLeaf(String name, String target, double p, double i, double d,
	  int mode = P_ON_M, int direction = DIRECT, int sample_time = 500)
	  : Leaf("pid", name, 0) {
    LEAF_ENTER(L_INFO);
    this->target = target;
    this->p = p;
    this->i = i;
    this->d = d;
    this->mode = mode;
    this->direction = direction;
    this->sample_time = sample_time;
    
    LEAF_LEAVE;
  }

  virtual void setup(void) {
    Leaf::setup();
    LEAF_ENTER(L_NOTICE);
    this->pid = new PID(&input, &output, &setpoint, p, i, d, mode, direction);
    this->pid->SetSampleTime(this->sample_time);
    this->install_taps(target);
    this->start();
    //this->stop();
    LEAF_LEAVE;
  }

  virtual void loop(void) {
    Leaf::loop();
    //LEAF_ENTER(L_NOTICE);
    //LEAF_LEAVE;
  }

  virtual void start(void) 
  {
    LEAF_ENTER(L_NOTICE);
    Leaf::start();
    pid->SetMode(AUTOMATIC);
    LEAF_LEAVE;
  }
  
  virtual void stop(void) 
  {
    LEAF_ENTER(L_DEBUG);
    Leaf::stop();
    pid->SetMode(MANUAL);
    LEAF_LEAVE;
  }

  bool isAuto() 
  {
    //return (pid->GetMode() == AUTOMATIC);
    return run;
  }
  
protected:
  float compute(float input) 
  {
    this->input = input;
    pid->Compute();
    LEAF_INFO("PID status for %s: setpoint=%.1f input=%.1f output=%.1f",
	 this->leaf_name.c_str(), setpoint, input, output);
    return output;
  }
  
};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:


