#pragma once

#include "Arduino.h"
#include "Client.h"
#include <memory>
#include <cbuf.h>

class IpClientSim7080 : public IpClientLTE, virtual public TraitDebuggable
{
public:
  IpClientSim7080(AbstractIpModemLeaf *modem, int slot) :
    IpClientLTE(modem, slot)
  {
  }
  
  virtual int connect(const char *host, uint16_t port)
  {
    char cmd[80];
    snprintf(cmd, sizeof(cmd), "AT+CAOPEN=0,%d,\"TCP\",\"%s\",%d",
	     slot, host, port);
    int result=-1;
    int cid;
    _connected = false;
    if (!modem->modemSendExpectIntPair(cmd, "+CAOPEN: ", &cid, &result, connect_timeout_ms, 2, HERE)) {
      // once cause of error might be a dangling connection, try closing and retry
      char cmd2[20];
      snprintf(cmd2, sizeof(cmd2), "AT+CACLOSE=%d", slot);
      if (modem->modemSendExpectOk(cmd2, HERE)) {
	LEAF_NOTICE("Closed a dangling connection");
	if (modem->modemSendExpectIntPair(cmd, "+CAOPEN: ", &cid, &result, connect_timeout_ms, 2, HERE)) {
	  _connected = true;
	}
      }
      if (!_connected) {
	LEAF_ALERT("CAOPEN failed: %d", result);
	return 0;
      }
    }
    LEAF_NOTICE("CAOPEN succeeded %d,%d", cid, result);
    if (cid != slot) {
      LEAF_ALERT("Wrong CID (%d) for connect result on slot %d", cid, slot);
    }
    _connected = true;
    return 1;
  }
  
  virtual size_t write(const uint8_t *buf, size_t size)
  {
    char cmd[40];
    snprintf(cmd, sizeof(cmd), "AT+CASEND=%d,%d", slot, size);
    if (!modem->modemSendExpectPrompt(cmd, 2000, HERE)) {
      LEAF_ALERT("AT+CASEND failed");
      return 0;
    }
    return modem->modemSendRaw(buf, size, HERE);
  }
  
  virtual void stop()
  {
    if (!modem->modemSendCmd(HERE, "AT+CACLOSE=%d", slot)) {
      LEAF_ALERT("CACLOSE failed");
    }
    else {
      _connected = false;
    }
  }

  virtual void dataIndication(int count) 
  {
    LEAF_ENTER_INT(L_NOTICE, count);
    char cmd[40];
    int room = rx_buffer->room();
    if (room == 0) {
      LEAF_WARN("RX buffer full");
      LEAF_VOID_RETURN;
    }

    // sim7080 chokes if buffer is over 1460
    if (room > 1460) room=1460;
    
    snprintf(cmd, sizeof(cmd), "AT+CARECV=%d,%d", slot, room);
    // Response will be +CARECV: <len>,<data>
    int len = 0;
    
    if (modem->modemSendExpectInlineInt(cmd, "+CARECV: ", &len, ',', -1, HERE)) {
      int got = modem->modemReadToBuffer(rx_buffer, len, HERE);
      if (got != len) {
	LEAF_WARN("CARECV short read");
      }
    }
    else {
      LEAF_WARN("CARECV error");
    }
  }

};



// Local Variables:
// mode: C++
// c-basic-offset: 2
// End:


