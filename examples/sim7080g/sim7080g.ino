#pragma STACX_BOARD espressif:esp32:ttgo-t7-v13-mini32
#include "defaults.h"
#include "config.h"
#include "stacx.h"

#include "leaf_fs_preferences.h"
#include "leaf_fs.h"
#include "leaf_ip_esp.h"
#include "leaf_ip_simcom_sim7080.h"
#include "leaf_pubsub_mqtt_esp.h"
#include "leaf_pubsub_mqtt_simcom_sim7080.h"
#include "leaf_shell.h"
#include "abstract_app.h"

void shell_prompt(char *buf, uint8_t len) 
{
  int pos = 0;
  pos += snprintf(buf, len, "%s %s", device_id, comms_state_name[stacx_comms_state]);
  pos += snprintf(buf+pos, len-pos, "> ");
}

class AppLeaf : public AbstractAppLeaf
{
public:
  AppLeaf(String name)
    : AbstractAppLeaf(name)
    , Debuggable(name)
  {
	  }

  virtual void setup() 
  {
  }
};

Leaf *leaves[] = {
	new FSPreferencesLeaf("prefs"),
        new FSLeaf("fs", FS_DEFAULT, FS_ALLOW_FORMAT),
	new ShellLeaf("shell", "Stacx", shell_prompt),
	new IpSimcomSim7080Leaf(
		"lte","fs",
		/*uart*/1,
		/*rx*/18,
		/*tx*/19,
		115200,
		SERIAL_8N1,
		MODEM_PWR_PIN_NONE,
		/*pwrkey*/5,
		/*slppin*/-1,
		LEAF_RUN,
		true, /* autoprobe */
		false /* invert key */
		),

	new PubsubMqttSimcomSim7080Leaf(
		"ltemqtt",
		"lte",
		PUBSUB_SSL_DISABLE,
		PUBSUB_DEVICE_TOPIC_DISABLE
		),

	new IpEspLeaf("esp", NO_TAPS, LEAF_STOP),
	NULL
};
