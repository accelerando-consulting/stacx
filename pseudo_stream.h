#pragma once

#include <StreamString.h>

class PseudoStream: public Stream
{
public:
  //
  // This is the "slave pty" interface, looks like a Stream to the caller,
  // while some "master pty" interacts with the "other end" of the stream
  //
  size_t write(const uint8_t *buffer, size_t size) { return fromSlave.write(buffer, size); }
  size_t write(uint8_t data) {
	  int rc = fromSlave.write(data);
	  NOTICE("Slave pty byte, buf now has %d", fromSlave.length());
	  DumpHex(L_NOTICE, "fromSlave", fromSlave.c_str(), fromSlave.length());
	  return rc;
  }

  int available() {
    int result = toSlave.length();
    if (result) {
      //INFO("PseudoStream avail=%d", result);
    }
    return  result;
  }
  
  int read() {
    int result = toSlave.read();
    //INFO("PseudoStream:read %x", result);
    return result;
  }

  int peek() { 
    int result = toSlave.peek();
    //INFO("PseudoStream:peek %x", result);
    return result;
  }
  void flush() { fromSlave.flush(); }

  //
  // This is the 'master pty' interface
  //
  size_t master_write(const uint8_t *buffer, size_t size) { return toSlave.write(buffer, size); }
  size_t master_write(uint8_t data) { return toSlave.write(data); }
  int master_available() { return fromSlave.length(); }
  int master_read() { return fromSlave.read(); }
  int master_peek() { return fromSlave.peek(); }
  void master_flush() { toSlave.flush(); }

  //
  // Under the hood the "pseudo tty" is implemented as two StringStream objects
  //
  // Slave should not access these, but master could mess with them if
  // going through the master_foo instance of the Stream interface is
  // too tedious
  StreamString toSlave;
  StreamString fromSlave;
};

