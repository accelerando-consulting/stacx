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

#ifdef ARDUINO_ESP32C3_DEV
#define MODEM_UART 0
#define MODEM_RX 5
#define MODEM_TX 6
#define MODEM_KEY 8
#define MODEM_SLP 7
#else
#define MODEM_UART 1
#define MODEM_RX 19
#define MODEM_TX 18
#define MODEM_KEY 5
#define MODEM_SLP 23
#endif

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
		MODEM_UART,
		MODEM_RX,
		MODEM_TX,
		115200,
		SERIAL_8N1,
		MODEM_PWR_PIN_NONE,
		MODEM_KEY,
		MODEM_SLP,
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
