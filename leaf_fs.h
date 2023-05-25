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
  size_t rotate_limit = 32768;
  

public:
  //
  // Leaf constructor method(s)
  // Call the superclass constructor to handle common arguments (type, name,
  // pins)
  //
  // Pass fs=NULL to initalise and use littlefs, otherwise pass your own FS pointer
  //
  FSLeaf(String name, fs::FS *fs=NULL, bool format=false)
    : Leaf("fs", name, (pinmask_t)0)
    , Debuggable(name)
  {
    this->fs = fs;
    format_on_fail = format;
  }

  //
  // Arduino Setup function
  // Use this to configure any IO resources
  //
  void setup(void) {
    Leaf::setup();
    LEAF_ENTER(L_INFO);

    if (!fs) {
#ifdef ESP8266
      if(!LittleFS.begin()) 
#else
      if(!LittleFS.begin(format_on_fail)) 
#endif
      {
	LEAF_ALERT("Filesystem Mount Failed");
	return;
      }
      fs = &LittleFS;
#ifdef ESP8266
      FSInfo info;
      if (LittleFS.info(info)) {
	LEAF_NOTICE("Total space: %lukB", (unsigned long)(info.totalBytes / 1024));
	LEAF_NOTICE("Used space: %lukB", (unsigned long)(info.usedBytes / 1024));
      }
#else
      LEAF_NOTICE("Total space: %lukB", (unsigned long)(LittleFS.totalBytes() / 1024));
      LEAF_NOTICE("Used space: %lukB", (unsigned long)(LittleFS.usedBytes() / 1024));
#endif
    }

    if (getDebugLevel() >= L_NOTICE) {
      listDir("/", 0, NULL, false);
    }
    registerLeafUlongValue("log_rotate_limit", &rotate_limit, "desired free space after log rotation (bytes)");

    registerCommand(HERE,"append/+", "append payload to a file");
    registerCommand(HERE,"appendl/+", "append payload to a file, adding a newline");
    registerCommand(HERE,"cat", "print the content of a file");
    registerCommand(HERE,"format", "format flash filesystem");
    registerCommand(HERE,"fsinfo", "print information about filesystem size and usage");
    registerCommand(HERE,"ls", "list a directory");
    registerCommand(HERE,"mv", "rename a file (oldname SPACE newname)");
    registerCommand(HERE,"rename", "rename a file (oldname SPACE newname)");
    registerCommand(HERE,"rm", "remove a file");
    registerCommand(HERE,"rotate", "rotate a log file");
    registerCommand(HERE,"store", "store data to a file");

    LEAF_LEAVE;
  }

  size_t getBytesFree() 
  {
    size_t total=0;
    size_t used=0;
#ifdef ESP8266
    FSInfo info;
    if (LittleFS.info(info)) {
      total = info.totalBytes;
      used = info.usedBytes;
    }
#else
    total = LittleFS.totalBytes();
    used = LittleFS.usedBytes();
#endif
    return total-used;
  }
  
  void listDir(const char * dirname, uint8_t levels, Stream *output=NULL, bool publish=true) {
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
      if  (file.isDirectory()) {
	LEAF_INFO("    %s <DIR>", file.name());
	if (publish) mqtt_publish("status/fs/dir", String(file.name()));
      }
      else {
	LEAF_INFO("    %s %d", file.name(), (int)file.size());
	if (publish) mqtt_publish("status/fs/file", String(file.name())+","+file.size());
      }
      if (output) {
	DBGPRINT("    ");
	DBGPRINT(file.name());
	DBGPRINT(" ");
	if (file.isDirectory()) {
	  DBGPRINTLN("<DIR>");
	}
	else {
	  DBGPRINTLN(file.size());
	}
      }
      file = root.openNextFile();
    }
  }

  void createDir(const char * path) {
    LEAF_NOTICE("Creating Dir: %s", path);
    if(fs->mkdir(path)){
      LEAF_NOTICE("Dir created");
    } else {
      LEAF_NOTICE("mkdir failed");
    }
  }

  void removeDir(const char * path) {
    LEAF_NOTICE("Removing Dir: %s", path);
    if(fs->rmdir(path)){
      LEAF_NOTICE("Dir removed");
    } else {
      LEAF_NOTICE("rmdir failed");
    }
  }

  void readFile(const char * path, Stream *output=NULL) {
    LEAF_NOTICE("Reading file: %s", path);

    File file = fs->open(path);
    if(!file){
      LEAF_NOTICE("Failed to open file for reading");
      return;
    }

    LEAF_NOTICE("Read from file: ");
    char buf[257];
    int a;
    int ln=0;
    while(a = file.available()){
      if (a>=sizeof(buf)) a=sizeof(buf)-1;
      int got = file.readBytesUntil('\n',buf, a);
      if (got==0) break;
      buf[got]='\0';
      ++ln;
      if (output) {
	output->println(buf);
      }
      else {
	mqtt_publish("status/file"+String(path)+"/"+ln, buf);
      }
    }
    file.close();
  }

  void writeFile(const char * path, const char * message) {
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

  // 
  // Given a filename, rename it to foo.old, and delete old files until there is at least highwater bytes free
  // 
  void rotateFile(const char *path, int highwater=0) 
  {
    LEAF_ENTER(L_NOTICE);
    char rotate_path[64];
    int generation = 0;
    int max_generation = 0;

    while (1) {
      snprintf(rotate_path, sizeof(rotate_path), "%s.%d", path, generation);
      if (fs->exists(rotate_path)) {
	++generation;
      }
      else {
	LEAF_WARN("Next available rotation for '%s' is '%s'", path, rotate_path);
	break;
      }
    }
    // postcondition: rotate_path is the next available filename, generation is its number
    max_generation = generation;

    // now if we have say generation=3, do app.log.2=>app.log.3 app.log.1=>app.log.2 app.log.0=>all.log.1
    while (generation >= 0) {
      char target_path[64];
      snprintf(target_path, sizeof(target_path), "%s.%d", path, generation);
      if (generation>0) {
	snprintf(rotate_path, sizeof(rotate_path), "%s.%d", path, generation-1);
      }
      else {
	snprintf(rotate_path, sizeof(rotate_path), "%s", path);
      }
	
      size_t free = getBytesFree();
      if ((highwater > 0) && (free < highwater)) {
	LEAF_WARN("Delete '%s' as free space (%d) is below limit (%d)", rotate_path, free, highwater);
	deleteFile(rotate_path);
      }
      else {
	LEAF_NOTICE("Rotate '%s' => '%s'", rotate_path, target_path);
	if (!fs->rename(rotate_path, target_path)) {
	  LEAF_WARN("Rename of '%s' => '%s' failed", rotate_path, target_path);
	}
      }
      --generation;
    }
    LEAF_LEAVE;
  }

  void appendFile(const char * path, const char * message, bool newline=false, int rotate_at=0){
    LEAF_NOTICE("Appending to file: %s", path);
    File file;

    if (fs->exists(path)) {
      file = fs->open(path, FILE_APPEND);
      if(!file){
	LEAF_ALERT("Failed to open file '%s' for appending", path);
	return;
      }
    }
    else {
      file = fs->open(path, FILE_WRITE);
      if(!file){
	LEAF_ALERT("Failed to open file '%s' for write", path);
	return;
      }
    }
    
    bool result;
    if (newline) {
      result = file.println(message);
    }
    else {
      result = file.print(message);
    }
    if(result){
      //LEAF_DEBUG("Message appended");
    } else {
      LEAF_ALERT("Append failed");
    }
    size_t new_size = file.size();
    file.close();

    if (rotate_at && (new_size >= rotate_at)) {
      rotateFile(path, rotate_at);
    }
  }

  bool renameFile(String &path1, String &path2){
    bool result=false;
    LEAF_NOTICE("Renaming file %s to %s", path1.c_str(), path2.c_str());
    if (fs->rename(path1, path2)) {
      LEAF_NOTICE("File renamed");
      result = true;

    } else {
      LEAF_ALERT("Rename of '%s' failed", path1.c_str());
    }
    return result;
  }

  void deleteFile(const char * path){
    LEAF_NOTICE("Deleting file: %s", path);
    if(fs->remove(path)){
      LEAF_NOTICE("File '%s' deleted", path);
    } else {
      LEAF_NOTICE("Delete of '%s' failed", path);
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
  //
  virtual bool commandHandler(String type, String name, String topic, String payload) {
    LEAF_HANDLER(L_INFO);

    WHEN("format", {
      LEAF_NOTICE("littlefs format");
      mqtt_publish("event/format", "begin");
      LittleFS.format();
      LEAF_NOTICE("littlefs format done");
      if (!LittleFS.begin()) {
	LEAF_ALERT("LittleFS mount failed");
	mqtt_publish("event/format", "fail");
      }
      else {
	mqtt_publish("event/format", "done");
      }
    })
    else if (topic.startsWith("append/") || topic.startsWith("appendl/")) {
      handled=true;
      bool newline = false;
      if (topic.startsWith("appendl/")) {
	topic.remove(0, strlen("appendl"));
	newline = true;
      }
      else {
	topic.remove(0, strlen("append"));
      }

      if (topic.length() < 2) {
	LEAF_ALERT("filename too short");
      } else {
	appendFile(topic.c_str(), payload.c_str(), newline, rotate_limit);
      }
    }
    ELSEWHEN("rotate", {
      size_t limit = rotate_limit;
      int pos = payload.indexOf(',');
      if (pos > 0) {
	limit = payload.substring(pos+1).toInt();
	payload.remove(pos);
      }
      rotateFile(payload.c_str(), limit);
    })
    ELSEWHEN("store",{
      File file = fs->open(payload.c_str(), "w");
      if(!file) {
	LEAF_ALERT("File not writable");
	return handled;
      }
      // Read from "stdin" (console)
      DBGPRINTLN("> (send CRLF.CRLF to finish)");
      while (debug_stream) {
	String line = debug_stream->readStringUntil('\n');
	if (line.startsWith(".\r") || line.startsWith(".\n")) {
	  break;
	}
	if (line=="") {
	  continue;
	}
	DBGPRINTLN(line);
	file.println(line);
      }
      file.close();
    })
    ELSEWHEN("ls",{
      if (payload == "") payload="/";
      listDir(payload.c_str(),1);
      })
    ELSEWHEN("cat",{
      readFile(payload.c_str());
    })
    ELSEWHEN("rm",{
      deleteFile(payload.c_str());
    })
    ELSEWHENEITHER("mv","rename",{
      int pos = payload.indexOf(" ");
      if (pos > 0) {
	String from = payload.substring(0,pos);
	String to = payload.substring(pos+1);
	renameFile(from, to);
      }
    })
    ELSEWHEN("fsinfo",{
      size_t total=0;
      size_t used=0;
      size_t free=0;
#ifdef ESP8266
      FSInfo info;
      if (LittleFS.info(info)) {
	total = info.totalBytes;
	used = info.usedBytes;
      }
#else
      total = LittleFS.totalBytes();
      used = LittleFS.usedBytes();
#endif
      free = total - used;
      mqtt_publish("status/fs_total_bytes", String(total));
      mqtt_publish("status/fs_used_bytes", String(used));
      mqtt_publish("status/fs_free_bytes", String(free));
    })
    else {
      handled = Leaf::commandHandler(type, name, topic, payload);
    }

    return handled;
  }

};


// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
