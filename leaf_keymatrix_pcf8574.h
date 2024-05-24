//@**************************** class KeyMatrixPCF8574Leaf ******************************
//
// This class encapsulates a keyboard matrix scanned using the PCF8574 io expander
//
#pragma once

#include <Wire.h>
#include "trait_pollable.h"
#include "trait_wirenode.h"

// TODO: make a generic key matrix superclass

class KeyMatrixPCF8574Leaf : public Leaf, public WireNode, public Pollable
{
protected:
  // TODO generalise from 16-key matrix to any-size using multiple expanders
  uint8_t row_count;
  uint8_t column_count;
  uint8_t strobe_count;
  uint8_t input_count;
  uint16_t scancode_max = 0;
  bool scan_by_column = true;
  bool strobe_is_lsb = true;
  bool strobe_level = HIGH;
  bool input_level = LOW;
  
  uint16_t *last_input_states=NULL;
  uint32_t strobe_bits = 0;
  uint32_t result_bits = 0;
  uint16_t read_timeout_ms = 200;

  uint8_t autoread = 0;
  uint32_t autoread_bits = 0;
  

  bool found;
  String key_name_config;
  String **key_names=NULL;
public:
  static constexpr const bool STROBE_IS_LSB = true;
  static constexpr const bool STROBE_IS_MSB = false;
  static constexpr const bool SCAN_BY_COLUMN = true;
  static constexpr const bool SCAN_BY_ROW = false;

  KeyMatrixPCF8574Leaf(
    String name, int address=0,
    int row_count=4,
    int column_count=4,
    String names="",
    bool strobe_is_lsb = STROBE_IS_LSB,
    bool scan_by_column= SCAN_BY_COLUMN
    )
    : Leaf("keymatrix", name, NO_PINS)
    , WireNode(name, address)
    , Pollable(-1, -1)
    , Debuggable(name)
  {
    LEAF_ENTER(L_INFO);
    found = false;
    this->row_count = row_count;
    this->column_count = column_count;
    this->scan_by_column = scan_by_column;
    this->strobe_is_lsb = strobe_is_lsb;
    if (scan_by_column) {
      strobe_count = column_count;
      input_count = row_count;
    }
    else {
      strobe_count = row_count;
      input_count = column_count;
    }
    scancode_max = row_count * column_count;
    this->key_name_config = names;
    key_names = (String **)calloc(scancode_max, sizeof(String *));
    last_input_states = (uint16_t *)calloc(strobe_count, sizeof(uint16_t));

    // input states and names will be populated after config is parsed in {setup}

    LEAF_LEAVE;
  }

  virtual void setup(void) {
    Leaf::setup();

    LEAF_ENTER(L_INFO);

    registerLeafIntValue("poll_interval_ms", &sample_interval_ms, "Keymatrix poll interval in milliseconds");

    registerLeafByteValue("i2c_addr", &address, "I2C address override for pin extender (decimal)");
    registerLeafByteValue("row_count", &row_count, "Number of scan rows");
    registerLeafByteValue("column_count", &column_count, "Number of scan columns");
    registerLeafBoolValue("scan_by_column", &scan_by_column, "Set true if columns are strobed");
    registerLeafBoolValue("strobe_is_lsb", &strobe_is_lsb, "Set true if strobe lines are the least significant bits");
    registerLeafBoolValue("strobe_level", &strobe_level, "Logic level to set on strobe line");
    registerLeafBoolValue("input_level", &input_level, "Logic level corresponding to keypress");
    registerLeafStrValue("key_names", &key_name_config, "Key names");
    

    registerLeafCommand(HERE, "poll");
    registerLeafCommand(HERE, "strobe");
    registerLeafCommand(HERE, "write");
    registerLeafCommand(HERE, "read");
    registerLeafCommand(HERE, "autoread");

    if ((address == 0) && probe(0x20)) {
      LEAF_NOTICE("   PCF8574 auto-detected at 0x20");
      address = 0x20;
    }
    delay(100);
    // not an else case to the above, we want that delay in any case
    if ((address == 0) && probe(0x38)) {
      LEAF_NOTICE("   PCF8574 auto-detected at 0x38");
      address = 0x38;
    }
    delay(100);

    if (!probe(address)) {
      LEAF_ALERT("   PCF8574 NOT FOUND at 0x%02x", (int)address);
      stop();
      LEAF_VOID_RETURN;
    }
    found=true;

    for (int i=0; i < input_count; i++) {
      if (strobe_is_lsb) {
	result_bits |= 1 << (strobe_count + i);
      }
      else {
	result_bits |= 1 << i;
      }
    }
    for (int s=0; s < strobe_count; s++) {
      last_input_states[s] = input_level?~result_bits:result_bits;
      if (strobe_is_lsb) {
	strobe_bits |= (1 << s);
      }
      else {
	strobe_bits |= (1 << (input_count + s));
      }
    }
    setKeyNames(key_name_config);// comma-sep string parsed out into array of names


    LEAF_NOTICE("%s claims I2C addr 0x%02x", describe().c_str(), address);
    LEAF_NOTICE("  %d strobes (%s, %cSB, active %s) with %d result inputs (active %s)",
		strobe_count,
		scan_by_column?"columns":"rows",
		strobe_is_lsb?'L':'M',
		STATE(strobe_level),
		input_count,
		STATE(input_level));
    LEAF_NOTICE("  strobe mask=0x%x 0b%s", strobe_bits, binary_string(strobe_bits));
    LEAF_NOTICE("  result mask=0x%x 0b%s", result_bits, binary_string(result_bits));

    // regardless of strobe order, we always name keys left-to-right top-to-bottom
    for (int row = 0; row < row_count; row++) {
      for (int col = 0; col < column_count; col++) {
	int pos = col + row*column_count;
	if (key_names[pos] != NULL) {
	  LEAF_NOTICE("  key %02d is named %s", pos, (key_names[pos])->c_str());
	}
      }
    }
    LEAF_LEAVE;
  }

  virtual bool commandHandler(String type, String name, String topic, String payload) {
    LEAF_HANDLER(L_INFO);
    WHEN("poll", poll())
    ELSEWHEN("strobe", strobe(payload.toInt()))
    ELSEWHEN("write", write_matrix(strtoul(payload.c_str(), NULL, 16)))
    ELSEWHEN("read", read_matrix())
    ELSEWHEN("autoread", {
	autoread = payload.toInt();
	if (autoread) {
	  LEAF_WARN("Enable auto-read for testing on strobe %d", (int)autoread-1);
	  strobe(autoread-1);
	}
	else {
	  LEAF_WARN("diasable auto-read");
	  write_matrix(result_bits);
	}
    })
    else Leaf::commandHandler(type, name, topic, payload);
    LEAF_HANDLER_END;
  }

  void setKeyNames(String names="")
  {
    LEAF_ENTER_STR(L_NOTICE, names);

    // names are in row-major order (eg. for 2x2 matrix r1c1,r1c2,r2c1,r2c2)
    for (int row = 0; row < row_count; row++) {
      for (int col = 0; col < column_count; col++) {
	int pos = col + row*column_count;
	if (key_names[pos] == NULL) {
	  key_names[pos] = new String();
	}

	if (names == "") {
	  *(key_names[pos]) = String("r")+(row+1)+"c"+(col+1);
	}
	else {
	  int pos = names.indexOf(',');
	  if (pos >= 0) {
	    *(key_names[pos]) = names.substring(0,pos);
	    names.remove(0,pos+1);
	  }
	  else {
	    *(key_names[pos]) = names;
	    names = "";
	  }
	}
      }
    }
    
    LEAF_LEAVE;
  }

  virtual void status_pub()
  {
    if (!found) return;
    LEAF_ENTER(L_DEBUG);

    char msg[64];
    for (int s=0; s < strobe_count; s++) {
      uint16_t last_state = last_input_states[s];
      for (int i=0; i < input_count; i++) {
	int scancode = get_scancode_for_input(s, i);
	if (scancode < 0) continue;

	bool level = test_input_bit(last_state, i);
	bool pressed = (level==input_level);
	const char *state = pressed?"pressed":"released";
	// when doing a shell command (pubsub_loopback) print everything. Otherwise only changed.
	if (pubsub_loopback || pressed) {
	  snprintf(msg, sizeof(msg), "status/%s", key_names[scancode]->c_str());
	  mqtt_publish(msg, state, 0, false, L_INFO, HERE);
	}
      }
    }
    LEAF_LEAVE;
  }


protected:
  inline void set_strobe_bit(uint32_t *bits, int pos, bool state) 
  {
    if (!bits) return;
    LEAF_DEBUG("set_strobe_bit %d %s", pos, STATE(state));
    uint32_t mask = 1 << (strobe_is_lsb?pos:(input_count+pos));
    LEAF_DEBUG("set_strobe_bit input is %s", binary_string(*bits));
    LEAF_DEBUG("set_strobe_bit mask is %s", binary_string(mask));
      
    if (state) {
      LEAF_DEBUG("Setting bit %d in mask", pos);
      *bits |= mask;
    }
    else {
      LEAF_DEBUG("Clearing bit %d in mask", pos);
      *bits &= ~mask;
    }

    LEAF_DEBUG("set_strobe_bit output is %s", binary_string(*bits));
  }

  inline bool test_input_bit(uint32_t bits, int pos) 
  {
    uint32_t mask = 1 << (strobe_is_lsb?(strobe_count+pos):pos);

    return bits & mask;
  }
      
  inline void clear_strobe_bits(uint32_t *bits) 
  {
    if (!bits) return;
    for (int s=0; s<strobe_count; s++) {
      set_strobe_bit(bits, s, !strobe_level);
    }
  }

  char *binary_string(uint32_t bits) 
  {
    // because this uses static storage, you cannot use this function twice in one statement (eg printfs)!
    static char binary_string_buf[33];
    
    int bit_count = row_count + column_count;
    char *c = binary_string_buf;
    for (int i=bit_count-1; i>=0; i--) {
      *c++ = (bits & (1<<i))?'1':'0';
    }
    *c = '\0';
    return binary_string_buf;
  }

  virtual void start(void)
  {
    Leaf::start();
    LEAF_ENTER(L_INFO);
    LEAF_NOTICE("Set outputs logically off at start");
    write_matrix(0); // all strobes logically "off" initially
    LEAF_LEAVE;
  }

  virtual void loop(void)
  {
    LEAF_ENTER(L_TRACE);

    Leaf::loop();

    if (autoread) {
      int was = class_debug_level;
      class_debug_level = L_INFO;
      uint32_t bits = read_matrix();
      class_debug_level = was;
      if (bits != autoread_bits) {
	LEAF_DEBUG("autoread was 0x08%x 0b%s", autoread_bits, binary_string(autoread_bits));
	LEAF_DEBUG("      => now 0x08%x 0b%s", bits, binary_string(bits));
	autoread_bits = bits;
      }
    }
    else {
      pollable_loop();
    }
    
    LEAF_LEAVE;
  }

  bool write_matrix(uint32_t bits) 
  {
    LEAF_DEBUG("write_matrix 0x%x 0b%s", bits, binary_string(bits));
    // TODO: multiple transmissions would be needed to support greater than 4x4 matrices
    wire->beginTransmission(address);

    int rc = Wire.write(bits);
    if (rc != 1) {
      LEAF_ALERT("PCF8574 pin write failed, returned %02x", rc);
      return false;
      
    }
    uint8_t err = wire->endTransmission(true);
    if (err != 0) {
      LEAF_ALERT("PCF8574 transaction failed (%d)", (int)err);
      return false;
    }
    return true;
  }
  
  uint32_t read_matrix() 
  {
    // TODO: multiple requests would be needed to support greater than 4x4 matrices
    
    Wire.requestFrom((uint8_t)address, (uint8_t)1);
    unsigned long then = millis();
    while (!Wire.available()) {
      unsigned long now = millis();
      if ((now - then) > read_timeout_ms) {
	ALERT("Timeout waiting for I2C byte\n");
	return 0;
      }
    }
    uint32_t inputs = Wire.read();
    LEAF_DEBUG("read_matrix <= 0x%x 0b%s", inputs, binary_string(inputs));
    return inputs;
    
  }

  int get_scancode_for_input(int strobe, int input) 
  {
    int scancode = -1;
    // regardless of wiring, keys are always assigned codes by row, then column
    if (scan_by_column) {
      scancode = input * row_count + strobe;
    }
    else {
      scancode = strobe * column_count + input;
    }
    return scancode;
  }

  int strobe(uint8_t strobe)
  {
    LEAF_ENTER_BYTE(L_DEBUG, strobe);

    // result inputs are all set {!input_level}
    // strobe outputs are all inactive EXCEPT one single strobed line 
    // A keypress will thus read as {input_level}
    uint32_t bits = result_bits;
    //
    // set one strobe active, all others inactive
    //
    for (int s=0; s<strobe_count; s++) {
      set_strobe_bit(&bits, s, (s==strobe)?strobe_level:!strobe_level);
    }
    
    LEAF_DEBUG("Setting strobe %d %s (bitmask 0x%x 0b%s)",
	       (int)strobe, STATE(strobe_level), bits, binary_string(bits));
    write_matrix(bits);

    LEAF_RETURN(0);
  }

  virtual bool poll()
  {
    LEAF_ENTER(L_TRACE);
    int strobes=0;
    int inputs=0;
    bool changed = false;
    
    for (int s=0; s < strobe_count; s++) {
      strobe(s);
      uint32_t result_inputs = read_matrix() & result_bits;
      uint32_t last_inputs = last_input_states[s];
      bool inputs_changed = (result_inputs != last_inputs);
      
      LEAF_DEBUG("Result inputs for strobe %d are 0x%x 0b%s (was 0x%x)",
		 s, result_inputs, binary_string(result_inputs), last_inputs);

      if (inputs_changed) {
	for (int i = 0; i < input_count; i++) {

	  bool level_was = test_input_bit(last_inputs, i);
	  bool level = test_input_bit(result_inputs, i);
	  String event = "release";

	  if (level_was == level) {
	    // no change for this bit
	    continue;
	  }

	  if (level == input_level) {
	    event = "press";
	  }
	  LEAF_INFO("Key %s event for key at [%d,%d]", event, s, i);
	  int scancode = get_scancode_for_input(s, i);

	  if (scancode >= 0) {
	    mqtt_publish(String("event/key/")+*key_names[scancode], event, 0, false, L_INFO, HERE);
	  }
	} // end for inputs
	last_input_states[s] = result_inputs;

      } // endif change

    } // end for strobes
    
    LEAF_BOOL_RETURN(changed);
  }

};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
