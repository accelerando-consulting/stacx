#pragma once
//
//@**************************** class AbstractCamera ******************************
// 
// This class encapsulates a camera device such as AI-Thinker's ESP32-cam.
//

class AbstractCameraLeaf : public Leaf
{
public:
    
  AbstractCameraLeaf(String name, String target, bool run = true)
    : Leaf("camera", name) 
    , Debuggable(name) {
    do_heartbeat = false;
    this->target = target;
    this->run = run;
  }

  virtual void setup() {
    Leaf::setup();
    this->install_taps(target);
  }
  
  virtual void loop(void) {
    Leaf::loop();
  }

  //virtual int getImage(const char *buf, int buf_max)=0;
  virtual int getParameter(const char *param) = 0;
  virtual bool setParameter(const char *param, int value) = 0;
  
protected:
  String target;
	
};


// Local Variables:
// mode: C++
// c-basic-offset: 2
// End:
