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


int shell_msg(int argc, char** argv)
{
  ENTER(L_NOTICE);
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
    else if (strcasecmp(argv[0],"mdm")==0) {
      pinMode(16, OUTPUT);
      if (Topic == "1") {
	ALERT("Set modem HIGH");
	digitalWrite(16,1);
      }
      else {	
	ALERT("Set modem LOW");
	digitalWrite(16,0);
      }
    }
    else if (strcasecmp(argv[0],"cmd")==0) {
      Topic = "cmd/"+Topic;
      NOTICE("Routing command %s", Topic.c_str());
    }
    else if (strcasecmp(argv[0],"do")==0) {
      Topic = "cmd/"+Topic;
      flags &= ~PUBSUB_LOOPBACK;
      NOTICE("Routing do command %s", Topic.c_str());
    }
    else if (strcasecmp(argv[0],"at")==0) {
      Topic = "cmd/at";
      if (argc >= 3) {
	Payload = String("AT")+String(argv[1])+" "+Payload;
      }
      else {
	Payload = String("AT")+String(argv[1]);
      }
      NOTICE("Routing AT command %s %s", Topic.c_str(), Payload.c_str());
    }

    if (pubsubLeaf) {
      pubsubLeaf->_mqtt_receive(Topic, Payload, flags);
      Serial.println(pubsubLeaf->getLoopbackBuffer());
    }
    else {
      ALERT("Can't locate pubsub leaf");
    }
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
    LEAF_ENTER(L_NOTICE);
    const char *shell_banner = "Stacx Command Shell";
    
    shell_init(shell_reader, shell_writer, (char *)shell_banner);

    // Add commands to the shell
    shell_register(shell_msg, PSTR("cmd"));
    shell_register(shell_msg, PSTR("slp"));
    shell_register(shell_msg, PSTR("mdm"));
    shell_register(shell_msg, PSTR("do"));
    shell_register(shell_msg, PSTR("get"));
    shell_register(shell_msg, PSTR("set"));
    shell_register(shell_msg, PSTR("at"));

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
