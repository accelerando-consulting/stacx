//
//@**************************** class DHTLeaf ******************************
//
// This class encapsulates a TCP/IP interface such as the ESP32's built in
// wifi, or that provided by a cellular modem
//

class AbstractIpLeaf : public Leaf
{
public:

  AbstractIpLeaf(String name, String target, pinmask_t pins=NO_PINS) : Leaf("ip", name, pins) {
    this->target = target;
    do_heartbeat = false;
  }

  virtual void setup(void) {
    LEAF_ENTER(L_DEBUG);
    Leaf::setup();
    this->install_taps(target);
    LEAF_LEAVE;
  }

  virtual void startConfig() {}
    

  virtual bool isPresent() { return true; }
  virtual bool isConnected() { return connected; }
  virtual bool gpsConnected() { return false; }

  virtual void ip_config(void) {}

  virtual void loop(void) {
    Leaf::loop();
  }

  virtual void pull_update(String url)
  {
  }
  virtual void rollback_update(String url)
  {
  }

  virtual bool ftpPut(String host, String user, String pass, String path, const char *buf, int buf_len)
  {
    return false;
  }

  virtual int ftpGet(String host, String user, String pass, String path, char *buf, int buf_max)
  {
    return -1;
  }
protected:
  String target;
  bool connected = false;

};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
