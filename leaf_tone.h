//@***************************** class ToneLeaf ******************************

#ifndef ESP8266
#include "ESP32Servo.h"
#include <Ticker.h>
#endif

#include "pitches.h"

class ToneLeaf;

class ToneLeaf *toneStopContext;

class ToneLeaf : public Leaf
{
public:
  bool state;
  int tonePin = -1;
  int freq;
  int duration;
  Ticker spkrTimer;
  bool do_test = false;
  int tempo=60;
  String tune="";

  ToneLeaf(String name, pinmask_t pins, int freq = 440, int duration=100, bool do_test=false)
    : Leaf("tone", name, pins)
    , Debuggable(name)
  {
    state = false;
    this->freq = freq;
    this->duration = duration;
    this->do_test = do_test;
  }

  virtual void setup(void) {
    Leaf::setup();
    FOR_PINS({tonePin=pin;});
    enable_pins_for_output();
    LEAF_NOTICE("%s claims pin %d as Speaker", describe().c_str(), tonePin);


    registerCommand(HERE, "note", "Play a note denoted by pitch and octave, eg C3");
    registerCommand(HERE, "tune", "Play a series of notes eg G4 A4 F4:2 R F3 C4:2");
    registerIntValue("tempo", &tempo, "Note tempo in beats per minute");
    registerCommand(HERE, "stop", "Stop playing tone(s)");
    registerCommand(HERE, "tone", "Play a tone given freq,duration");
    registerIntValue("freq", &freq, "Note frequency in Hz");
    registerIntValue("duration", &duration, "Default tone frequency in milliseconds");

  }


  bool isBusy() { return state; }
  void setBusy() { state=true; }
  void clearBusy() { state=false; }

  void stopTone()
  {
#ifdef ESP8266
    analogWrite(tonePin, 0);
#else
    noTone(tonePin);
#endif
    //LEAF_INFO("Silenced tone on pin %d", tonePin);
    if (tune.length()>0) {
      playTune(tune);
    }
    else {
      clearBusy();
    }
  }

  void playTone(int pitch, int len)
  {
    int pin = tonePin;
    if (len <= 1) len = duration;
    if (pitch == 0) pitch=freq;

    if (pitch > 0) {
      //LEAF_INFO("Playing %dHz tone for %dms on pin %d", pitch, len, pin);
#ifdef ESP8266
      analogWriteFreq(pitch);
      analogWrite(pin, 128);
#else
      tone(pin, pitch);
#endif
    }
    else {
      // pitch < 0 means a 'rest'
      //LEAF_INFO("Playing rest for %dms on pin %d", len, pin);
    }

    toneStopContext = this;
    spkrTimer.once_ms(len, [](){
      if (toneStopContext) {
	ToneLeaf *l = toneStopContext;
	toneStopContext = NULL;
	l->stopTone();
	// note stopTone() may reschedule-timer and re-set toneStopContext
      }
    });
  }

  void playNote(String note, float beats)
  {
    LEAF_ENTER(L_INFO);
    if (note.length()) {
      LEAF_NOTICE("playNote %s, %f", note.c_str(), beats);
    }
    else {
      LEAF_ALERT("Empty note definition");
      LEAF_VOID_RETURN;
    }

    setBusy();
    int pin = tonePin;
    int octave=1;
    int key;
    int pos=0;

    switch (toupper(note[pos++])) {
    case 'C':
      key=1;
      break;
    case 'D':
      key=3;
      break;
    case 'E':
      key=5;
      break;
    case 'F':
      key=6;
      break;
    case 'G':
      key=8;
      break;
    case 'A':
      key=10;
      break;
    case 'B':
      key=12;
      break;
    case 'R':
      key=-1;
      break;
    default:
      LEAF_ALERT("Invalid note [%s]", note.c_str());
      LEAF_VOID_RETURN;
    }
    switch (note[pos]) {
    case 'S':
      key++;
      pos++;
      break;
    case 'F':
      key--;
      pos++;
    }

    if (key >= 0) {
      // i.e not a rest
      if ((note[pos]<'0') || (note[pos]>'8')) {
	LEAF_ALERT("Invalid note [%s]", note.c_str());
	LEAF_VOID_RETURN;
      }
      octave = note[pos]-'0';

      while (octave < 1) {
	key-=12;
	octave++;
      }
      while (octave > 1) {
	key+=12;
	octave--;
      }
      if ((key < 0) || (key>88)) {
	LEAF_ALERT("Invalid note [%s]", note.c_str());
      }
    }


    int freq = (key==-1)?-1:sNotePitches[key];
    int ms = beats * 60000/tempo;
    //LEAF_DEBUG("Note %s is key %d (%dHz).  %.3f beats is %dms", note.c_str(), key, freq, beats, ms);
    playTone(freq, ms);
    LEAF_VOID_RETURN;
  }

  void playTune(String tune)
  {
    // this could be called from interrupt context, so be light on the logging, OK?
    LEAF_ENTER_STR(L_DEBUG, tune);
    String note;
    int pos;
    float beats;

    if ((pos = tune.indexOf(" ")) > 0) {
      note = tune.substring(0,pos);
      tune.remove(0,pos+1);
    }
    else {
      note = tune;
      tune="";
    }

    if ((pos = note.indexOf(",")) > 0) {
      beats = note.substring(pos+1).toFloat();
      note.remove(pos);
    }
    else {
      beats = 1;
    }

    setBusy();
    this->tune=String(tune);
    //LEAF_DEBUG("Playing note %s,%f, remainder of tune is %s", note.c_str(), beats, this->tune.c_str());
    playNote(note, beats);
    LEAF_LEAVE;
  }


  virtual void start(void)
  {
    Leaf::start();
    if (do_test) {
      LEAF_NOTICE("Test speaker");
      playTone(0,250);
    }
  }

  virtual bool commandHandler(String type, String name, String topic, String payload) {
    LEAF_HANDLER(L_INFO);

    WHEN("note",{
	if (isBusy()) {
	  LEAF_NOTICE("Speaker busy");
	}
	else {
	  int space = payload.indexOf(" ");
	  float beats = 1;
	  if (space>0) {
	    beats = payload.substring(space+1).toFloat();
	    payload.remove(space);
	  }
	  playNote(payload, beats);
	}
      })
    ELSEWHEN("tune",{
	if ((payload.length() == 0) || (payload=="1")) {
	  payload = "G4 A4 F4:2 R F3 C4:2";
	}
	playTune(payload);
      })
    ELSEWHEN("stop",{
	stopTone();
      })
    ELSEWHEN("tone",{
	if (isBusy()) {
	  LEAF_NOTICE("Speaker busy");
	}
	else {
	  int freq = 0;
	  int duration = 0;
	  int comma = payload.indexOf(",");
	  if (comma) {
	    freq = payload.toInt();
	    duration = payload.substring(comma+1).toInt();
	  }
	  setBusy();
	  playTone(freq, duration);
	  //LEAF_INFO("Tone playing in background");
	}
    })
    else {
      handled = Leaf::commandHandler(type, name, topic, payload);
    }

    LEAF_HANDLER_END;
  }

};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
