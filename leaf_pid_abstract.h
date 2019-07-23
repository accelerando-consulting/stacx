// 
// This class encapsulates an abstract pid loop
//
#pragma once

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
    ENTER(L_INFO);
    this->target = target;
    this->p = p;
    this->i = i;
    this->d = d;
    this->mode = mode;
    this->direction = direction;
    this->sample_time = sample_time;
    
    LEAVE;
  }

  virtual void setup(void) {
    Leaf::setup();
    ENTER(L_NOTICE);
    this->pid = new PID(&input, &output, &setpoint, p, i, d, mode, direction);
    this->pid->SetSampleTime(this->sample_time);
    this->install_taps(target);
    this->start();
    //this->stop();
    LEAVE;
  }

  virtual void loop(void) {
    Leaf::loop();
    //ENTER(L_NOTICE);
    //LEAVE;
  }

  virtual void start(void) 
  {
    ENTER(L_DEBUG);
    pid->SetMode(AUTOMATIC);
    run = true;
    LEAVE;
  }
  
  virtual void stop(void) 
  {
    ENTER(L_DEBUG);
    run = false;
    pid->SetMode(MANUAL);
    LEAVE;
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
    NOTICE("PID status for %s: setpoint=%.1f input=%.1f output=%.1f",
	 this->leaf_name.c_str(), setpoint, input, output);
    return output;
  }
  
};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:


