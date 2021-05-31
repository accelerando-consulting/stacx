//
//@**************************** class WireFollowerLeaf *****************************
//
// You can copy this class to use as a boilerplate for new classes
//
#include <driver/i2c.h>
#include <rBase64.h>

class WireFollowerLeaf : public Leaf
{
protected:
  String target;
  i2c_port_t bus;
  int pin_sda;
  int pin_scl;
  byte address;
  size_t buffer_size = 256;
  unsigned long last_receive = 0;
  unsigned long receive_timeout_ms = 50;

  uint8_t *tmp_buffer=NULL;
  size_t tmp_size = 0;

  uint8_t *inbound_buffer=NULL;
  size_t inbound_size = 0;

  uint8_t *outbound_buffer=NULL;
  size_t outbound_size = 0;

  //char *msg_buffer;
  //size_t msg_buffer_size = 0;

public:
  //
  // Leaf constructor method(s)
  // Call the superclass constructor to handle common arguments (type, name, pins)
  //
  WireFollowerLeaf(String name, String target, int sda, int scl, int address=0x10, int message_size=0, const char magic[3]="I2C", int buffer_size=0, int bus=0) : Leaf("wirefollower", name, NO_PINS){
    this->target = target;
    this->bus = (i2c_port_t)bus;
    this->pin_sda = sda;
    this->pin_scl = scl;
    this->address = address;
    do_heartbeat=false;
    if (buffer_size != 0) {
      this->buffer_size = buffer_size;
    }
    inbound_buffer = (uint8_t *)calloc(1,this->buffer_size);
    tmp_buffer = (uint8_t *)calloc(1,this->buffer_size);
    outbound_buffer = (uint8_t *)calloc(1,this->buffer_size);
    memset(tmp_buffer, 0, this->buffer_size);
    //msg_buffer_size = buffer_size * 4 / 3 + 10;
    //msg_buffer = (char *)calloc(1, msg_buffer_size);

    // Set up for an empty outbound message initially
    outbound_buffer[0] = message_size;
    outbound_buffer[1] = magic[0];
    outbound_buffer[2] = magic[2];
    outbound_buffer[3] = magic[3];
    outbound_size = message_size;
  }

  ~WireFollowerLeaf()
  {
    if (tmp_buffer) free(tmp_buffer);
    if (inbound_buffer) free(inbound_buffer);
    if (outbound_buffer) free(outbound_buffer);
  }

  //
  // Arduino Setup function
  // Use this to configure any IO resources
  //
  void setup(void) {
    Leaf::setup();
    LEAF_ENTER(L_DEBUG);
    this->install_taps(target);

    esp_err_t err;
    i2c_config_t config;
    config.mode = I2C_MODE_SLAVE;
    config.slave.addr_10bit_en = 0;
    config.sda_io_num = gpio_num_t(pin_sda);
    config.scl_io_num = gpio_num_t(pin_scl);
    config.sda_pullup_en = GPIO_PULLUP_ENABLE;
    config.scl_pullup_en = GPIO_PULLUP_ENABLE;
    config.slave.slave_addr = address;
    ;
    LEAF_NOTICE("Configuring I2C bus %d", (int)bus);
    if ((err = i2c_param_config(bus, &config)) != ESP_OK) {
      LEAF_ALERT("I2C follower config of bus %d failed, error %d", (int)bus, (int)err);
      return;
    }

    LEAF_NOTICE("Setting I2C bus %d to slave mode", (int)bus);
    if ((err = i2c_driver_install(bus, I2C_MODE_SLAVE, buffer_size, buffer_size, 0)) != ESP_OK) {
      LEAF_ALERT("I2C follower driver initiate for bus %d failed, error %d", (int)bus, (int)err);
      return;
    }

    if (outbound_size > 0) {
      LEAF_NOTICE("Writing initial (empty) message to I2C bus %d output buffer", (int)bus);
      size_t sent = i2c_slave_write_buffer(bus, outbound_buffer, outbound_size, 0);
      if (sent != outbound_size) {
	LEAF_ALERT("i2c_follower_write on bus %d unable to queue output, wrote %d of %d", (int)bus, (int)sent, (int)outbound_size);
      }
      else {
	LEAF_NOTICE("Wrote initial (empty) configuration to I2C output buffer");
      }
    }

    LEAF_LEAVE;
  }

  //
  // Arduino loop function
  // (Superclass function will take care of heartbeats)
  //
  void loop(void) {
    Leaf::loop();

    int len;
    size_t rcvd;
    unsigned long now,elapsed;
    int msg_buffer_size = buffer_size * 4 / 3 + 10;
    char msg_buffer[msg_buffer_size];

    now = millis();
    rcvd = i2c_slave_read_buffer(bus, tmp_buffer+tmp_size, buffer_size, 50);
    elapsed = millis() - now;

    if (rcvd == 0) {
      LEAF_DEBUG("nothing to see here (elapsed %lu)",elapsed);

      if (tmp_size > 0) {
	uint8_t len = tmp_buffer[0];
	if (tmp_size < len) {
	  // If we have received a partial message, but then no subsequent message
	  // for a while, clear the receive buffer (i.e. presume that we missed the remainder)
	  unsigned long cutoff = last_receive + receive_timeout_ms;
	  if (now > cutoff) {
	    LEAF_ALERT("Clearing receive buffer of partial message (%d) after timeout", (int)tmp_size);
	    tmp_size = 0;
	  }
	}
      }
      return;
    }

    //
    // It is possible to receive only part of a message from the master, so
    // it might take several reads to accumulate a full buffer.
    //
    last_receive = now;
    tmp_size += rcvd;
    uint8_t message_size = tmp_buffer[0];

    if (tmp_size < message_size) {
      LEAF_DEBUG("Partial message of %d bytes received (elapsed %lu), bringing total to %d.  Wait for more.",(int)rcvd, elapsed, (int)tmp_size);
      //DumpHex(L_DEBUG, "RCVD PART", tmp_buffer, tmp_size);
      return;
    }

    while ((message_size > 0) && (tmp_size >= message_size)) {

      if ((message_size == inbound_size) &&
	  (tmp_size >= inbound_size) &&
	  (memcmp(inbound_buffer, tmp_buffer, message_size) == 0)) {

	LEAF_DEBUG("Ignore duplicate message (size=%d)", message_size);
	// We have at least one complete message, but its a duplicate.
	//
	// We only treat the received message as NEW if it differs from the last
	// (use a message ID counter if that is a problem for your application)

	if (tmp_size == inbound_size) {
	  LEAF_DEBUG("Dropped entire inbound buffer");
	  tmp_size = 0; // consider the buffer empty, as we discarded its entire content
	  message_size = 0;
	}
	else {
	  // drop one message from the inbound buffer, shift the buffer content
	  memmove(tmp_buffer, tmp_buffer + message_size, tmp_size - message_size);
	  tmp_size -= message_size;
	  message_size = tmp_buffer[0];
	  LEAF_DEBUG("Shifted inbound buffer, now have %d left (new message_size=%d)", tmp_size, message_size);
	}
	continue;
      }


      if (tmp_size == message_size) {
	//
	// We have received exactly one complete message
	//
	//
	LEAF_DEBUG("Received %d bytes from I2C leader (elapsed %lu)", (int)tmp_size, elapsed);
	DumpHex(L_DEBUG, "RCVD", tmp_buffer, tmp_size);

	memcpy(inbound_buffer, tmp_buffer, tmp_size);
	inbound_size = tmp_size;
	tmp_size = 0;
      }
      else {
	// we have more than one complete message, process the first one only on
	// this loop
	LEAF_NOTICE("Overflow in RX buffer (message_size=%d buffer_size=%d), process one message", message_size, tmp_size);

	memcpy(inbound_buffer, tmp_buffer, message_size);
	inbound_size = message_size;
	memmove(tmp_buffer, tmp_buffer + message_size, tmp_size - message_size);
	tmp_size -= message_size;
	message_size = tmp_buffer[0];
	LEAF_DEBUG("After shifting input buffer, next message size=%d, buffer_size=%d", message_size, tmp_size);
      }

      //
      // We now have exactly one complete new message in inbound_buf
      //
      i2c_reset_rx_fifo(bus);
      LEAF_DEBUG("Received %d bytes from I2C leader (elapsed %lu)", (int)inbound_size, elapsed);
      //DumpHex(L_DEBUG, "RCVD", inbound_buffer, inbound_size);
      len = rbase64_encode(msg_buffer, (char *)inbound_buffer, inbound_size);
      msg_buffer[len] = '\0';
      publish("event/i2c_follower_receive", String(msg_buffer));
    }

    LEAF_DEBUG("No more complete messages in input buffer (%d)", tmp_size);

  }

  virtual void pre_sleep(int duration=0) 
  {
    LEAF_ENTER(L_NOTICE);
#ifdef BREAK_CAMERA
    i2c_driver_delete(bus);
#endif
    LEAF_LEAVE;
  }
  



  //
  // MQTT message callback
  // (Use the superclass callback to ignore messages not addressed to this leaf)
  //
  bool mqtt_receive(String type, String name, String topic, String payload) {
    LEAF_ENTER(L_DEBUG);
    bool handled = Leaf::mqtt_receive(type, name, topic, payload);
    size_t sent;
    //LEAF_NOTICE("MR %s %s", topic.c_str(), payload.c_str());

    WHEN("cmd/i2c_follower_write",{
	LEAF_DEBUG("Commanded to send %s", payload.c_str());
	if (payload.length() > (buffer_size * 4 / 3 + 2)) {
	  LEAF_ALERT("i2c_follower_write payload too long");
	}
	else {
	  outbound_size = rbase64_decode((char *)outbound_buffer, (char *)payload.c_str(), payload.length());
	  LEAF_DEBUG("Queueing %d bytes for I2C leader", outbound_size);
	  DumpHex(L_DEBUG, "DATA", outbound_buffer, outbound_size);

	  i2c_reset_tx_fifo(bus);
	  if ((sent = i2c_slave_write_buffer(bus, outbound_buffer, outbound_size, 0)) != outbound_size) {
	    LEAF_ALERT("i2c_follower_write on bus %d unable to queue output, wrote %d of %d", (int)bus, (int)sent, (int)outbound_size);
	  }
	  else {
	    LEAF_DEBUG("Queued %d bytes successfully", sent);
	  }
	}
      })
      ELSEWHEN("cmd/i2c_follower_flush",{
	  size_t got = 0;
	  do {
	    got = i2c_slave_read_buffer(bus, tmp_buffer, buffer_size, 0);
	    if (got) {
	      LEAF_NOTICE("Flush requested on bus %d, discarding I2C follower read buffer", (int)bus);
	    }
	  } while (got > 0);
	  tmp_size = 0;
	});




    return handled;
  }



};


// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
