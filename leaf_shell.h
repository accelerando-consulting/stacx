#define CONFIG_SHELL_MAX_INPUT 200
#define CONFIG_SHELL_MAX_COMMANDS 10

#include "Shell.h"

static int shell_reader(char * data)
{
  if (Serial.available()) {
    *data = Serial.read();
    return 1;
  }
  return 0;
}

static void shell_writer(char data)
{
  Serial.write(data);
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

int debug_shell=0;
int shell_msg(int argc, char** argv)
{
  int was = debug_level;
  debug_level += debug_shell;
  ENTER(L_INFO);
  INFO("shell_msg argc=%d", argc);
  for (int i=0; i<argc;i++) {
    INFO("shell_msg argv[%d]=%s", i, argv[i]);
  }
  Serial.flush();

  String Topic;
  String Payload="";

  AbstractPubsubLeaf *pubsubLeaf = (AbstractPubsubLeaf *)Leaf::get_leaf_by_type(leaves, String("pubsub"));

  if (argc >= 3) {
    for (int i=2; i<argc; i++) {
      Payload += String(argv[i]);
      if (i < (argc-1)) {
	Payload += " ";
      }
    }
  }
  else {
    Payload = "1";
  }
  int flags = PUBSUB_LOOPBACK;

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
    }
    else if (strcasecmp(argv[0],"set")==0) {
      Topic = "set/"+Topic;
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

	esp_sleep_enable_ext0_wakeup((gpio_num_t)0, 0);
	if (argc >= 3) {
	  int sec = Payload.toInt();
	  ALERT("Will enter deep sleep for %dsec", sec);
	  esp_sleep_enable_timer_wakeup(sec * 1000000ULL);
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
	Serial.flush();

	esp_deep_sleep_start();
      }
      else if (Topic == "light") {
	esp_sleep_enable_ext0_wakeup((gpio_num_t)0, 0);
	esp_light_sleep_start();
      }
    }
#endif
    else if (strcasecmp(argv[0],"cmd")==0) {
      Topic = "cmd/"+Topic;
      INFO("Routing command %s", Topic.c_str());
    }
    else if (strcasecmp(argv[0],"do")==0) {
      flags &= ~PUBSUB_LOOPBACK;
      INFO("Routing do command %s", Topic.c_str());
    }
    else if ((argc>2) && (strcasecmp(argv[0],"msg")==0)) {
      flags &= ~PUBSUB_LOOPBACK;
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
      NOTICE("Finding leaf named '%s'", rcpt.c_str());
      Leaf *tgt = Leaf::get_leaf_by_name(leaves, rcpt);
      if (!tgt) {
	ALERT("Did not find leaf named %s", rcpt.c_str());
      }
      else {
	NOTICE("Injecting fake message to %s: %s <= [%s]", tgt->describe().c_str(), Topic.c_str(), Payload.c_str());
	tgt->mqtt_receive("shell", "shell", Topic, Payload);
	goto _done;
      }
    }

    if (pubsubLeaf) {
      INFO("Injecting fake receive %s <= [%s]", Topic.c_str(), Payload.c_str());
      pubsubLeaf->_mqtt_receive(Topic, Payload, flags);
      Serial.println(pubsubLeaf->getLoopbackBuffer());
    }
    else {
      ALERT("Can't locate pubsub leaf");
    }
  }

_done:
  LEAVE;
  debug_level = was;
  return SHELL_RET_SUCCESS;
}

int shell_dbg(int argc, char** argv)
{
  ENTER(L_INFO);
  INFO("shell_dbg argc=%d", argc);
  for (int i=0; i<argc;i++) {
    INFO("shell_dbg argv[%d]=%s", i, argv[i]);
  }
  Serial.flush();

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
  Serial.flush();

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
      Serial.println("usage: pin NUM mode {out|in|inp");
    }
  }
  else if (strcasecmp(verb, "read") == 0) {
    val = digitalRead(pin);
    Serial.println(val);
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
public:
  ShellLeaf(String name) : Leaf("shell", name)
  {
  }

  virtual void setup(void)
  {
    LEAF_ENTER(L_INFO);
    const char *shell_banner = "Stacx Command Shell";

    debug_shell = getIntPref("debug_shell", debug_shell);
    shell_init(shell_reader, shell_writer, (char *)shell_banner);

    // Add commands to the shell
    shell_register(shell_dbg, PSTR("dbg"));
    shell_register(shell_pin, PSTR("pin"));
    shell_register(shell_help, PSTR("help"));
    shell_register(shell_msg, PSTR("cmd"));
    //shell_register(shell_msg, PSTR("slp"));
    shell_register(shell_msg, PSTR("do"));
    shell_register(shell_msg, PSTR("get"));
    shell_register(shell_msg, PSTR("set"));
    shell_register(shell_msg, PSTR("msg"));

    LEAF_LEAVE;
  }


  virtual void loop(void)
  {
    shell_task();
  }


};


// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
