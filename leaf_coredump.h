//
//@**************************** class SDCardLeaf *****************************
//
// Access to SD cards
//
#include "FS.h"
#include <LittleFS.h>
#include <SPI.h>
#include "esp_core_dump.h"

#ifndef COREDUMP_PARTITION_NAME
#define COREDUMP_PARTITION_NAME "coredump"
#endif

class CoreDumpLeaf : public Leaf
{

protected:
  //
  // Declare your leaf-specific instance data here
  //
  String partition_name;

  void check(String payload);
  void erase(String payload);
  void summary(String payload);
  void read(String payload);

  const esp_partition_t* findCoreDumpPartition();
  void readCoreDump(const esp_partition_t* part_CoreDump, char* content, long offset, long size);

public:
  //
  // Leaf constructor method(s)
  // Call the superclass constructor to handle common arguments (type, name,
  // pins)
  //
  // Pass fs=NULL to initalise and use littlefs, otherwise pass your own FS pointer
  //
  CoreDumpLeaf(String name, String partition_name = COREDUMP_PARTITION_NAME)
    : Leaf("coredump", name, (pinmask_t)0)
    , Debuggable(name)
    , partition_name(partition_name)
  {

  }

  virtual void setup(void);
  virtual bool commandHandler(String type, String name, String topic, String payload);
};

void CoreDumpLeaf::setup(void) {
  Leaf::setup();
  LEAF_ENTER(L_INFO);

  registerLeafStrValue("partition_name", &partition_name, "name of coredump partition");

  registerLeafCommand(HERE,"check", "check if there is a valid coredump present");
  registerLeafCommand(HERE,"erase", "erase any stored coredump");
  registerLeafCommand(HERE,"read", "read and print contents of any coredump");
  registerLeafCommand(HERE,"summary", "read and print summary of any coredump");

  LEAF_LEAVE;
}

void CoreDumpLeaf::check(String payload)
{
  LEAF_ENTER(L_NOTICE);
  esp_err_t err = esp_core_dump_image_check();
  bool valid = ( err == ESP_OK);
  mqtt_publish("status/core/present", TRUTH_lc(valid));
  LEAF_LEAVE;
}

void CoreDumpLeaf::erase(String payload)
{
  LEAF_ENTER(L_NOTICE);
  esp_err_t err = esp_core_dump_image_erase();
  bool valid = ( err == ESP_OK);
  mqtt_publish("status/core/erase", TRUTH_lc(valid));
  LEAF_LEAVE;
}

const esp_partition_t* CoreDumpLeaf::findCoreDumpPartition()
{
    esp_partition_type_t p_type = ESP_PARTITION_TYPE_DATA;
    esp_partition_subtype_t p_subtype = ESP_PARTITION_SUBTYPE_DATA_COREDUMP;
    const char *label = partition_name.c_str();

    return esp_partition_find_first(p_type, p_subtype, label);
}

void CoreDumpLeaf::readCoreDump(const esp_partition_t* part_CoreDump, char* content, long offset, long size)
{
	esp_partition_read(part_CoreDump, offset, content, size);
}

//
// @brief calculate the intel hex checksum of a hex string
//
uint8_t intelHexChecksumCalc(const char *buf, int len)
{
  uint8_t sum=0;
  char word[3];
  unsigned long val;
  for (int p=0;p<len;p+=2) {
    word[0] = buf[p];
    word[1] = buf[p+1];
    word[2]='\0';
    val = strtoul(word, NULL, 16);
    sum += (uint8_t)(val&0xFF);
  }
  return (~sum)+1;
}

bool intelHexChecksumVerify(const char *buf, int len)
{
  uint8_t sum=0;
  char word[3];
  unsigned long val;
  for (int p=0;p<len;p+=2) {
    word[0] = buf[p];
    word[1] = buf[p+1];
    word[2]='\0';
    val = strtoul(word, NULL, 16);
    sum += (uint8_t)(val&0xFF);
  }
  return (sum==0);
}

void CoreDumpLeaf::read(String payload)
{
  LEAF_ENTER(L_NOTICE);

  esp_err_t err = esp_core_dump_image_check();
  if (err != ESP_OK) {
    LEAF_ALERT("Core dump image check failed: %d", (int)err);
    mqtt_publish("status/core/dump", "invalid");
    LEAF_VOID_RETURN;
  }
  size_t core_addr;
  size_t core_size;

  err = esp_core_dump_image_get(&core_addr, &core_size);
  if (err != ESP_OK) {
    LEAF_ALERT("Core dump image lookup failed: %d", (int)err);
    mqtt_publish("status/core/dump", "error");
    LEAF_VOID_RETURN;
  }

  LEAF_NOTICE("core dump of %d bytes located at %d", (int)core_size, (int)core_addr);
  char buf[32];
  snprintf(buf, sizeof(buf),"0x%X:0x%X", (int)core_addr, (int)core_size);
  mqtt_publish("status/core/addr", buf);

  const esp_partition_t* partition = findCoreDumpPartition();
  if (partition == NULL) {
    mqtt_publish("status/core/data", "notfound");
    LEAF_VOID_RETURN;
  }

  int chunk_size = 32;
  char topic_buf[80];
  char hex_buf[80];
  int hexpos = 0;
  int rec_start;
  int rec_len;

  // intel hex extended linear address record
  rec_start = hexpos;
  hexpos += snprintf(hex_buf+hexpos, sizeof(hex_buf)-hexpos, ":02000004%04X",
		     (core_addr>>16));
  hexpos += snprintf(hex_buf+hexpos, sizeof(hex_buf)-hexpos, "%02x",
		     (int)intelHexChecksumCalc(hex_buf+rec_start+1,
					       hexpos-rec_start));
  snprintf(topic_buf, sizeof(topic_buf), "status/core/dump/head");
  mqtt_publish(topic_buf, hex_buf, 0, false, L_NOTICE, HERE);

  for (int pos=0; pos<core_size; pos+=chunk_size) {
    int bite = (core_size > chunk_size)?chunk_size:core_size;

    readCoreDump(partition, buf, pos, bite);
    hexpos=0;

    // intel hex data record
    rec_start=hexpos;
    hexpos += snprintf(hex_buf+hexpos, sizeof(hex_buf)-hexpos, ":%02X%04X00",
		       (int)bite, (int)pos&0xFFFF);
    for (int i=0; i<bite; i++) {
      hexpos += snprintf(hex_buf+hexpos, sizeof(hex_buf)-hexpos, "%02X", buf[i]);
    }
    hexpos += snprintf(hex_buf+hexpos, sizeof(hex_buf)-hexpos, "%02x",
		       (int)intelHexChecksumCalc(hex_buf+rec_start+1,
						 hexpos-rec_start));
    hex_buf[hexpos]='\0';
    snprintf(topic_buf, sizeof(topic_buf), "status/core/dump/%04x", pos);
    mqtt_publish(topic_buf, hex_buf, 0, false, L_NOTICE, HERE);
  }

  // intel hex end of file record
  snprintf(topic_buf, sizeof(topic_buf), "status/core/dump/tail");
  snprintf(hex_buf,sizeof(hex_buf),":00000001FF");
  mqtt_publish(topic_buf, hex_buf, 0, false, L_NOTICE, HERE);

}

void CoreDumpLeaf::summary(String payload)
{
  LEAF_ENTER(L_NOTICE);
  esp_core_dump_summary_t summary;

  esp_err_t err = esp_core_dump_get_summary(&summary);
  if (err != ESP_OK) {
    LEAF_ALERT("Core dump image inspection failed: %d", (int)err);
    String result = String("error ")+(int)err;
    mqtt_publish("status/core/summary", result);
    LEAF_VOID_RETURN;
  }

  char buf[32];
  mqtt_publish("status/core/summary/task", summary.exc_task);
  snprintf(buf, sizeof(buf), "%08x", summary.exc_pc);
  mqtt_publish("status/core/summary/pc", buf);
  LEAF_NOTIDUMP("Backtrace", summary.exc_bt_info.stackdump, summary.exc_bt_info.dump_size);
  LEAF_LEAVE;
}


bool CoreDumpLeaf::commandHandler(String type, String name, String topic, String payload) {
  LEAF_HANDLER(L_INFO);

  WHEN("check", check(payload))
  ELSEWHEN("erase", erase(payload))
  ELSEWHEN("read", read(payload))
  ELSEWHEN("summary", summary(payload))
  else {
    handled = Leaf::commandHandler(type, name, topic, payload);
  }

  LEAF_HANDLER_END;
}





// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
