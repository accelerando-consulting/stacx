#ifndef _LEAF_SHELL_H_
#define _LEAF_SHELL_H_

#define CONFIG_SHELL_MAX_INPUT 200
#define CONFIG_SHELL_MAX_COMMANDS 20

#define SHELL_LOOP_SEPARATE true
#define SHELL_LOOP_SHARED false

#ifndef USE_SHELL_BUFFER
#define USE_SHELL_BUFFER 0
#endif

#include "Shell.h"

#ifdef ESP32
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif

Stream *shell_stream = NULL;

static int shell_reader(char * data)
{
  if (!shell_stream) return 0;
  
  if (shell_stream->available()) {
    *data = shell_stream->read();
    return 1;
  }
  return 0;
}

#if USE_SHELL_BUFFER
static void shell_bwriter(char *data, uint8_t len)
{
  if (!shell_stream) return;
  shell_stream->write(data, len);
}
#else
static void shell_writer(char data)
{
  if (!shell_stream) return;
  shell_stream->write(data);
}
#endif

static void shell_loop(void *args) 
{
  shell_task();
}

int shell_help(int argc, char** argv) 
{
  shell_println("Commands are: cmd do get set slp");
  shell_println("         cmd: as if published to cmd/<arg1> <arg2>, with mqtt disabled");
  shell_println("         dbg: set debug to <arg1> (0=alert 1=notice 2=info 3=debug)");
  shell_println("          do: as if published to <arg1> <arg2>, with mqtt enabled");
  shell_println("         get: as if published to get/<arg1> <arg2>");
  shell_println("         msg: send to leaf <arg1> topic=<arg2> payload=<arg3>");
  shell_println("         pin: do GPIO. 'pin NUM {mode|write|read}' (mode=out/in)");
  shell_println("         set: as if published to set/<arg1> <arg2>");
  shell_println("         slp: <arg1>=(deep|light) <arg2>=SECONDS, eg 'slp deep 60'");
  shell_println("");
  return 0;
}
AbstractPubsubLeaf *shell_pubsub_leaf = NULL;
AbstractIpLeaf *shell_ip_leaf = NULL;

int debug_shell=0;
bool shell_force;

int shell_msg(int argc, char** argv)
{
  int was = debug_level;
  //debug_level += debug_shell;
  ENTER(L_DEBUG);
  DEBUG("shell_msg argc=%d", argc);
  for (int i=0; i<argc;i++) {
    DEBUG("shell_msg argv[%d]=%s", i, argv[i]);
  }
  shell_stream->flush();

  if (strcasecmp(argv[0],"exit")==0) {
    // exit the shell
    shell_force = false;
    return 0;
  }


  String Topic;
  String Payload="";

  if (argc >= 3) {
    for (int i=2; i<argc; i++) {
      Payload += String(argv[i]);
      if (i < (argc-1)) {
	Payload += " ";
      }
    }
  }
  else {
    Payload = "";
  }
  int flags = PUBSUB_LOOPBACK|PUBSUB_SHELL;

  if (argc < 2) {
    ALERT("Invalid command");
  }
  else {
    Topic = String(argv[1]);
    if (strcasecmp(argv[0],"get")==0) {
      // fix a wart, pref syntax is get pref foo and set pref/foo value
      // auto-convert the wrong-but-tempting syntax "get pref/foo" to "get pref foo"
      if (Topic.startsWith("pref/")) {
	int pos = Topic.indexOf("/");
	Payload = Topic.substring(pos+1);
	Topic.remove(pos);
      }
      Topic = "get/"+Topic;
      if (shell_pubsub_leaf && shell_pubsub_leaf->hasPriority()) Topic = shell_pubsub_leaf->getPriority() + "/" + Topic;
    }
    else if (strcasecmp(argv[0],"set")==0) {
      Topic = "set/"+Topic;
      if (shell_pubsub_leaf && shell_pubsub_leaf->hasPriority()) Topic = shell_pubsub_leaf->getPriority() + "/" + Topic;
    }
    else if (strcasecmp(argv[0],"dbg")==0) {
      if ((argc>2) && (strcasecmp(argv[1], "shell")==0)) {
	debug_shell = atoi(argv[2]);
	ALERT("debug_shell set to %d", debug_shell);
      }
      else {
	debug_level = was = Payload.toInt();
	ALERT("debug_level set to %d", debug_level);
      }
    }
#ifdef ESP32
    else if (strcasecmp(argv[0],"slp")==0) {
      if (Topic == "deep") {
	int sec;

#ifndef ARDUINO_ESP32C3_DEV
	esp_sleep_enable_ext0_wakeup((gpio_num_t)0, 0);
#endif
	if (argc >= 3) {
	  int sec = Payload.toInt();
	  ALERT("Will enter deep sleep for %dsec", sec);
#ifndef ARDUINO_ESP32C3_DEV
	  esp_sleep_enable_timer_wakeup(sec * 1000000ULL);
#endif
	}
	else {
	  ALERT("Will enter deep sleep indefinitely");
	}

	// Apply sleep in reverse order, highest level leaf first
	int leaf_index;
	for (leaf_index=0; leaves[leaf_index]; leaf_index++);
	for (leaf_index--; leaf_index<=0; leaf_index--) {
	  leaves[leaf_index]->pre_sleep(sec);
	}
	ALERT("Initiating deep sleep.  Over and out.");
	shell_stream->flush();

	esp_deep_sleep_start();
      }
      else if (Topic == "light") {
#ifndef ARDUINO_ESP32C3_DEV
	esp_sleep_enable_ext0_wakeup((gpio_num_t)0, 0);
#endif
	esp_light_sleep_start();
      }
    }
#endif
    else if (strcasecmp(argv[0],"cmd")==0) {
      Topic = "cmd/"+Topic;
      if (shell_pubsub_leaf && shell_pubsub_leaf->hasPriority()) Topic = shell_pubsub_leaf->getPriority() + "/" + Topic;
      INFO("Routing command %s", Topic.c_str());
    }
    else if (strcasecmp(argv[0],"do")==0) {
      Topic = "cmd/"+Topic;
      flags &= ~PUBSUB_LOOPBACK;
      INFO("Routing do command %s", Topic.c_str());
    }
    else if (strcasecmp(argv[0],"ena")==0) {
      INFO("Enabling preference %s", Topic.c_str());
      Topic = "set/pref/"+Topic;
      Payload = "on";
    }
    else if (strcasecmp(argv[0],"dis")==0) {
      INFO("Disabling preference %s", Topic.c_str());
      Topic = "set/pref/"+Topic;
      Payload = "off";
    }
    else if (strcasecmp(argv[0],"pre")==0) {
      if (argc == 2) {
	INFO("Fetching preference %s", Topic.c_str());
	Topic = "get/pref";
	Payload = Topic;
      }
      else {
	NOTICE("Setting preference %s <= [%s]", Topic.c_str(), Payload.c_str());
	Topic = "set/pref/"+Topic;
      }
    }
    else if (strcasecmp(argv[0],"at")==0) {
      Topic = "cmd/at";
      if (argc >= 3) {
	Payload = String("AT")+String(argv[1])+" "+Payload;
      }
      else {
	Payload = String("AT")+String(argv[1]);
      }
      INFO("Routing AT command %s %s", Topic.c_str(), Payload.c_str());
    }
    else if ((argc>2) && (strcasecmp(argv[0],"msg")==0)) {
      //flags &= ~PUBSUB_LOOPBACK;
      String rcpt = argv[1];
      Topic = argv[2];
      if (argc <= 3) {
	Payload="1";
      }
      else {
	Payload="";
	for (int i=3; i<argc; i++) {
	  Payload += String(argv[i]);
	  if (i < (argc-1)) {
	    Payload += " ";
	  }
	}
      }
      INFO("Finding leaf named '%s'", rcpt.c_str());
      Leaf *tgt = Leaf::get_leaf_by_name(leaves, rcpt);
      if (!tgt) {
	ALERT("Did not find leaf named %s", rcpt.c_str());
      }
      else if (!shell_pubsub_leaf) {
	ALERT("Can't locate pubsub leaf");
      }
      else {
	NOTICE("Messaging %s: %s <= [%s]", tgt->describe().c_str(), Topic.c_str(), Payload.c_str());
	shell_pubsub_leaf->enableLoopback();
	tgt->mqtt_receive("shell", "shell", Topic, Payload);
	String buf = shell_pubsub_leaf->getLoopbackBuffer();
	if (buf.length()) {
	  shell_stream->println("\nShell result:");
	  shell_stream->println(buf);
	}
	shell_pubsub_leaf->clearLoopbackBuffer();
	shell_pubsub_leaf->cancelLoopback();
	goto _done;
      }
    }
    else if (strcasecmp(argv[0],"tsk")==0) {
      // char tasks[512];
      //vTaskList(tasks);
      //shell_stream->println(tasks);
      goto _done;
    }

    if (shell_pubsub_leaf) {
      INFO("Injecting fake receive %s <= [%s]", Topic.c_str(), Payload.c_str());
      shell_pubsub_leaf->_mqtt_receive(Topic, Payload, flags);
      String buf = shell_pubsub_leaf->getLoopbackBuffer();
      if (buf.length()) {
	shell_stream->println("\nShell result:");
	shell_stream->println(buf);
      }
      shell_pubsub_leaf->clearLoopbackBuffer();
    }
    else {
      ALERT("Can't locate pubsub leaf");
    }
  }

_done:
  LEAVE;
  //debug_level = was;
  return SHELL_RET_SUCCESS;
}

int shell_dbg(int argc, char** argv)
{
  ENTER(L_INFO);
  INFO("shell_dbg argc=%d", argc);
  for (int i=0; i<argc;i++) {
    INFO("shell_dbg argv[%d]=%s", i, argv[i]);
  }
  shell_stream->flush();

  if (argc < 2) {
    ALERT("Invalid command");
  }

  if (strcasecmp(argv[0],"dbg")==0) {
    debug_level = atoi(argv[1]);
  }

  LEAVE;
  return SHELL_RET_SUCCESS;
}

int shell_pin(int argc, char** argv)
{
  ENTER(L_INFO);
  INFO("shell_dbg argc=%d", argc);
  for (int i=0; i<argc;i++) {
    INFO("shell_dbg argv[%d]=%s", i, argv[i]);
  }
  shell_stream->flush();

  if (argc < 3) {
    ALERT("Invalid command. pin NUM {mode|write|read}");
  }

  int pin = atoi(argv[1]);
  int val;
  const char *value;

  const char *verb = argv[2];
  if ((argc>=4) && (strcasecmp(verb, "mode") == 0)) {
    value = argv[3];
    if (strcasecmp(value, "out")==0) {
      pinMode(pin, OUTPUT);
    }
    else if (strcasecmp(value, "in")==0) {
      pinMode(pin, INPUT);
    }
    else if (strcasecmp(value, "inp")==0) {
      pinMode(pin, INPUT_PULLUP);
    }
    else {
      shell_stream->println("usage: pin NUM mode {out|in|inp");
    }
  }
  else if (strcasecmp(verb, "read") == 0) {
    val = digitalRead(pin);
    shell_stream->println(val);
  }
  else if ((argc>=4) && (strcasecmp(verb, "write") == 0)) {
    val = atoi(argv[3]);
    digitalWrite(pin, val);
  }
  else {
    ALERT("Usage: pin NUM {mode|write|read}");
  }
  
  LEAVE;
  return SHELL_RET_SUCCESS;
}


class ShellLeaf : public Leaf
{
protected:
  String banner = "Stacx Command Shell";
  shell_prompter_t prompt_cb = NULL;
#ifdef ESP32
  bool own_loop = false;
  TaskHandle_t leaf_task_handle = NULL;
#endif
public:
  ShellLeaf(String name, const char *banner=NULL, shell_prompter_t prompter = NULL, bool own_loop = false)
    : Leaf("shell", name)
  {
    if (banner) this->banner=banner;
    if (prompter) this->prompt_cb = prompter;
#ifdef ESP32
    this->own_loop = own_loop;
#endif

  }

  virtual void setup(void)
  {
    Leaf::setup();
    LEAF_ENTER(L_INFO);
    shell_stream = debug_stream;

#ifdef ESP32
    getBoolPref("shell_own_loop", &own_loop, "Use a separate thread for shell");
#endif

#if FORCE_SHELL
    shell_force = true;
#else
    getBoolPref("shell_force", &shell_force, "Force drop into shell during setup");
#endif

    if (shell_force) {
      prefsLeaf->start();
      start();
      Serial.printf("\n\nEntering debug shell.  Type \"exit\" to continue\n\n");
      while (shell_force) {
	shell_task();
      }
    }
    
    LEAF_LEAVE;
  }

  virtual void start(void) 
  {
    Leaf::start();
    shell_pubsub_leaf = pubsubLeaf;
    shell_ip_leaf = ipLeaf;
#if USE_SHELL_BUFFER
    shell_init(shell_reader, NULL, (char *)(banner.c_str()));
    shell_use_buffered_output(&shell_bwriter);
#else
    shell_init(shell_reader, shell_writer, (char *)(banner.c_str()));
#endif
    if (prompt_cb) {
      shell_register_prompt(prompt_cb);
    }

    // Add commands to the shell
    shell_register(shell_dbg, PSTR("dbg"));
    shell_register(shell_pin, PSTR("pin"));
    shell_register(shell_help, PSTR("help"));
    shell_register(shell_msg, PSTR("cmd"));
    //shell_register(shell_msg, PSTR("slp"));
    shell_register(shell_msg, PSTR("do"));
    shell_register(shell_msg, PSTR("get"));
    shell_register(shell_msg, PSTR("set"));
    shell_register(shell_msg, PSTR("ena"));
    shell_register(shell_msg, PSTR("dis"));
    shell_register(shell_msg, PSTR("pre"));
    shell_register(shell_msg, PSTR("slp"));
    shell_register(shell_msg, PSTR("mdm"));
    shell_register(shell_msg, PSTR("at"));
    shell_register(shell_msg, PSTR("msg"));
    shell_register(shell_msg, PSTR("tsk"));
    shell_register(shell_msg, PSTR("exit"));

    getIntPref("debug_shell", &debug_shell, "Additional trace detail increase during shell commands");


#ifdef ESP32
    if (own_loop) {
      LEAF_ALERT("Starting task for shell");
      xTaskCreateUniversal(&shell_loop, "shell_loop", 8192, this, 1, &leaf_task_handle, ARDUINO_RUNNING_CORE);
    }
#endif
  }

  virtual void loop(void)
  {
#ifdef ESP32
    if (!own_loop) {
      shell_task();
    }
#else
    shell_task();
#endif
  }


};

#endif
// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
