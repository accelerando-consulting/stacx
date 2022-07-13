//
//@**************************** class SDCardLeaf *****************************
// 
// Access to SD cards
// 
#include "FS.h"
#include <LittleFS.h>
#include <SPI.h>

#define FS_ALLOW_FORMAT true
#define FS_DONOT_FORMAT false
#define FS_DEFAULT NULL

class FSLeaf : public Leaf
{
  fs::FS *fs;

  // 
  // Declare your leaf-specific instance data here
  //
  bool format_on_fail = false;
  
public:
  // 
  // Leaf constructor method(s)
  // Call the superclass constructor to handle common arguments (type, name,
  // pins)
  //
  // Pass fs=NULL to initalise and use littlefs, otherwise pass your own FS pointer
  //
  FSLeaf(String name, fs::FS *fs=NULL, bool format=false) : Leaf("littlefs", name, (pinmask_t)0){
    this->fs = fs;
    format_on_fail = format;
  }

  //
  // Arduino Setup function
  // Use this to configure any IO resources
  //
  void setup(void) {
    Leaf::setup();
    LEAF_ENTER(L_NOTICE);

    if (!fs) {
      if(!LittleFS.begin(format_on_fail)) {
	LEAF_ALERT("Filesystem Mount Failed");
	return;
      }
      fs = &LittleFS;
      LEAF_NOTICE("Total space: %lukB", (unsigned long)(LittleFS.totalBytes() / 1024));
      LEAF_NOTICE("Used space: %lukB", (unsigned long)(LittleFS.usedBytes() / 1024));
    }

    listDir("/", 0);
    
    LEAF_LEAVE;
  }
  
  void listDir(const char * dirname, uint8_t levels){
    LEAF_NOTICE("Listing directory: %s", dirname);

    File root = fs->open(dirname);
    if(!root){
      LEAF_NOTICE("Failed to open directory");
      return;
    }
    if(!root.isDirectory()){
      LEAF_NOTICE("Not a directory");
      return;
    }

    File file = root.openNextFile();
    while(file){
      if(file.isDirectory()){
	LEAF_NOTICE("  DIR: %s", file.name());
	if(levels){
	  listDir(file.name(), levels -1);
	}
      } else {
	LEAF_NOTICE("  FILE: %s  SIZE: %d", file.name(), file.size());
      }
      file = root.openNextFile();
    }
  }

  void createDir(const char * path){
    LEAF_NOTICE("Creating Dir: %s", path);
    if(fs->mkdir(path)){
      LEAF_NOTICE("Dir created");
    } else {
      LEAF_NOTICE("mkdir failed");
    }
  }

  void removeDir(const char * path){
    LEAF_NOTICE("Removing Dir: %s", path);
    if(fs->rmdir(path)){
      LEAF_NOTICE("Dir removed");
    } else {
      LEAF_NOTICE("rmdir failed");
    }
  }

  void readFile(const char * path){
    LEAF_NOTICE("Reading file: %s", path);

    File file = fs->open(path);
    if(!file){
      LEAF_NOTICE("Failed to open file for reading");
      return;
    }

    LEAF_NOTICE("Read from file: ");
    char buf[257];
    int a;
    while(a = file.available()){
      if (a>=sizeof(buf)) a=sizeof(buf)-1;
      int got = file.readBytesUntil('\n',buf, a);
      if (got==0) break;
      buf[got]='\0';
      LEAF_NOTICE("%s", buf);
    }
    file.close();
  }

  void writeFile(const char * path, const char * message){
    LEAF_NOTICE("Writing file: %s", path);

    File file = fs->open(path, FILE_WRITE);
    if(!file){
      LEAF_NOTICE("Failed to open file for writing");
      return;
    }
    if(file.print(message)){
      LEAF_NOTICE("File written");
    } else {
      LEAF_NOTICE("Write failed");
    }
    file.close();
  }

  void appendFile(const char * path, const char * message){
    LEAF_INFO("Appending to file: %s", path);

    File file = fs->open(path, FILE_APPEND);
    if(!file){
      LEAF_NOTICE("Failed to open file for appending");
      return;
    }
    if(file.print(message)){
      LEAF_DEBUG("Message appended");
    } else {
      LEAF_ALERT("Append failed");
    }
    file.close();
  }

  void renameFile(const char * path1, const char * path2){
    LEAF_NOTICE("Renaming file %s to %s", path1, path2);
    if (fs->rename(path1, path2)) {
      LEAF_NOTICE("File renamed");
    } else {
      LEAF_NOTICE("Rename failed");
    }
  }

  void deleteFile(const char * path){
    LEAF_NOTICE("Deleting file: %s", path);
    if(fs->remove(path)){
      LEAF_NOTICE("File deleted");
    } else {
      LEAF_NOTICE("Delete failed");
    }
  }

  void testFileIO(const char * path){
    File file = fs->open(path);
    static uint8_t buf[512];
    size_t len = 0;
    uint32_t start = millis();
    uint32_t end = start;
    if(file){
      len = file.size();
      size_t flen = len;
      start = millis();
      while(len){
	size_t toRead = len;
	if(toRead > 512){
	  toRead = 512;
	}
	file.read(buf, toRead);
	len -= toRead;
      }
      end = millis() - start;
      LEAF_NOTICE("%u bytes read for %u ms", flen, end);
      file.close();
    } else {
      LEAF_NOTICE("Failed to open file for reading");
    }


    file = fs->open(path, FILE_WRITE);
    if(!file){
      LEAF_NOTICE("Failed to open file for writing");
      return;
    }

    size_t i;
    start = millis();
    for(i=0; i<2048; i++){
      file.write(buf, 512);
    }
    end = millis() - start;
    LEAF_NOTICE("%u bytes written for %u ms", 2048 * 512, end);
    file.close();
  }


  // 
  // MQTT message callback
  // (Use the superclass callback to ignore messages not addressed to this leaf)
  //
  bool mqtt_receive(String type, String name, String topic, String payload) {
    LEAF_ENTER(L_DEBUG);
    bool handled = Leaf::mqtt_receive(type, name, topic, payload);

    do {
    if (topic.startsWith("cmd/append/")) {
      handled=true;
      topic.remove(0, strlen("cmd/append"));
      if (topic.length() < 2) {
	LEAF_ALERT("filename too short");
	break;
      }
      appendFile(topic.c_str(), payload.c_str());
    }
    ELSEWHEN("cmd/ls",{
      if (payload == "") payload="/";
      listDir(payload.c_str(),1);
      })
      ELSEWHEN("cmd/cat",{
      readFile(payload.c_str());
	})
      ELSEWHEN("cmd/rm",{
      deleteFile(payload.c_str());
	})
      ELSEWHEN("cmd/mv",{
      int pos = payload.indexOf(" ");
      if (pos < 0) break;
      String from = payload.substring(0,pos);
      String to = payload.substring(pos+1);
      rename(from.c_str(), to.c_str());
	});
		  

//     WHEN("cmd/foo",{cmd_foo()})
//      ELSEWHEN("set/other",{set_other(payload)});

      } while(0);
		  
    return handled;
  }
    
};


// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
