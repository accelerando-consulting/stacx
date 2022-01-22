#ifndef _TRAIT_PID_H
#define _TRAIT_PID_H

#include <PID_v1.h>

class Pid
{
protected:
  double input, output, setpoint;
  double p, i, d;
  int mode = P_ON_M;
  int direction = DIRECT;
  int sample_time = 100;
  PID *pid;

  void pid_setup(double p, double i, double d, int mode = P_ON_M, int direction = DIRECT, int sample_time=500) 
  {
    this->p = p;
    this->i = i;
    this->d = d;
    this->mode = mode;
    this->direction = direction;
    this->pid = new PID(&input, &output, &setpoint, p, i, d, mode, direction);
    this->pid->SetSampleTime(this->sample_time);
    this->pid->SetMode(AUTOMATIC);
  }
  

  bool pid_compute(double input, double *r_output) 
  {
    this->input = input;
    bool rc = pid->Compute();
    if (r_output) *r_output=output;
    return rc;
  }
};


#endif
// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
