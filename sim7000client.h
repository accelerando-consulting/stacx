#include "Arduino.h"
#include "Client.h"
#include <memory>
#include <cbuf.h>


class Sim7000Client : public Client 
{
protected:
  Sim7000Modem *modem = NULL;
  int slot;
  bool _connected;
  cbuf *rx_buffer = NULL;

public:
  Sim7000Client(Sim7000Modem *modem, int slot) 
  {
    this->modem = modem;
    this->slot = slot;
    this->_connected = false;
    this->rx_buffer = new cbuf(4096);
  }

  ~Sim7000Client() 
  {
    delete this->rx_buffer;
  }
  

  int getSlot() { return slot; }

  int readToBuffer(size_t size) 
  {
    return modem->readToBuffer(rx_buffer, size);
  }

  int connect(IPAddress ip, uint16_t port)
  {
    return this->connect(ip.toString().c_str(), (int)port);
  }
  
  int connect(const char *host, uint16_t port)
  {
    char cmd[40];
    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=%d,\"TCP\",\"%s\",%d",
	     slot, host, (int)port);
    if (!modem->sendCheckReply(cmd, "OK")) {
      ALERT("CIPSTART failed");
      return 0;
    }
    _connected = true;
    return 1;
  }
  
  size_t write(uint8_t c)
  {
    char cmd[40];
    return write(&c, 1);
  }
  
  size_t write(const uint8_t *buf, size_t size)
  {
    int len;
    char cmd[40];
    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d,%d", slot, size);
    if (!modem->sendExpectIntReply(cmd,"DATA ACCEPT: ", &len)) {
      ALERT("CIPSTART failed");
      return 0;
    }
    if (len < size) {
      return modem->write(buf, len);
    }
    return modem->write(buf, size);
  }
  
  int available()
  {
    return rx_buffer->available();
  }
  
  int read()
  {
    return rx_buffer->read();
  }
  
  int read(uint8_t *buf, size_t size)
  {
    return rx_buffer->read((char *)buf, size);
  }
  
  int peek()
  {
    return rx_buffer->peek();
  }
  
  void flush()
  {
  }
  
  void stop()
  {
    char cmd[40];
    snprintf(cmd, sizeof(cmd), "AT+CIPCLOSE=%d", slot);
    if (!modem->sendCheckReply(cmd, "OK")) {
      ALERT("CIPCLOSE failed");
    }
    else {
      _connected = false;
    }
  }
  
  uint8_t connected()
  {
    return _connected;
  }
  
  operator bool() 
  {
    return connected();
  }
  
};


// Local Variables:
// mode: C++
// c-basic-offset: 2
// End:


