/*
 * Macros for consistent use of trace output
 *
 * You should define DBG to be the stream you want to use (default: Serial)
 */

#define L_ALERT 0
#define L_NOTICE 1
#define L_INFO 2
#define L_DEBUG 3
#define L_TRACE 4

#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL L_NOTICE
#endif
#ifndef MAX_DEBUG_LEVEL
#define MAX_DEBUG_LEVEL L_DEBUG
#endif
#ifndef DEBUG_FLUSH
#define DEBUG_FLUSH false
#endif
#ifndef DEBUG_LINES
#define DEBUG_LINES false
#endif

int debug = DEBUG_LEVEL;
bool debug_lines = DEBUG_LINES;
bool debug_flush = DEBUG_FLUSH;
unsigned long debug_slow = 500;
int DBGWAIT = 0;


#ifndef DBG
#define DBG Serial
#endif

#if USE_BT_CONSOLE
#define DBGPRINTF(...) {			\
  DBG.printf(__VA_ARGS__);			\
  if (SerialBT && SerialBT->hasClient()) SerialBT->printf(__VA_ARGS__);	\
  }
#define DBGPRINTLN() { \
  DBG.println();       \
  if (SerialBT && SerialBT->hasClient()) SerialBT->println();	\
  }
#else
#define DBGPRINTF(...) DBG.printf(__VA_ARGS__)
#define DBGPRINTLN() DBG.println()
#endif

#define DBGMILLIS(l) {							\
  if (debug_lines) {							\
    DBGPRINTF("#%8lu %6s %s(%d) ", millis(), _level_str(l), __PRETTY_FUNCTION__, __LINE__); \
  } else {								\
    DBGPRINTF("#%8lu %6s ", millis(), _level_str(l)); \
  }									\
}									\


#define DBGINDENT DBGPRINTF("\t")

const char *_level_str(int l) {
  switch (l) {
  case L_ALERT: return "ALERT";
  case L_NOTICE: return "NOTICE";
  case L_INFO: return "INFO";
  case L_DEBUG: return "DEBUG";
  default: return "TRACE";
  }
}

#if USE_OLED
#define OLED_TEXT(x,y,t) oled_text(x,y,t)
#else
#define OLED_TEXT(x,y,t) {}
#endif

#define OLEDLINE(l,s,...) {			\
  char buf[65];		 \
  snprintf(buf, sizeof(buf), __VA_ARGS__);	\
  ALERT("%s(%d): %s", s, strlen(buf), buf);	\
  OLED_TEXT(0, l*11, buf);			\
  }						

//#define BANNER(...)  ALERT(__VA_ARGS__); OLEDLINE(0,__VA_ARGS__)
#define STATUS(...) OLEDLINE(0,"STATUS",__VA_ARGS__)
#define ACTION(...) OLEDLINE(1,"ACTION",__VA_ARGS__)
#define ERROR(...)  OLEDLINE(2,"ERROR",__VA_ARGS__)


#ifdef SYSLOG_flag

#ifndef SYSLOG_host
#define SYSLOG_host "notused"
#ifndef SYSLOG_IP
#define SYSLOG_IP 255,255,255,255
#endif
#endif

const unsigned int SYSLOG_port = 514;
WiFiUDP UDP;

void _udpsend(char *dst, unsigned int port, char *buf, unsigned int len)
{
  //Serial.print("udpsend "); Serial.print(dst); Serial.print(":");Serial.print(port);Serial.print(" => "); Serial.println(buf);

  static bool udpready = false;
#ifdef SYSLOG_IP
  static IPAddress syslogIP(SYSLOG_IP);
#else
  static IPAddress syslogIP;
#endif
  if (!udpready) {
    if (UDP.begin(SYSLOG_port)) {
      udpready = true;
    }
    else {
      return;
    }
#ifndef SYSLOG_IP
    WiFi.hostByName(dst, syslogIP);
#endif
  }
  UDP.beginPacket(syslogIP, port);
  UDP.write((uint8_t *)buf, len);
  UDP.endPacket();
}


#define SYSLOG(l,...)  \
  if (SYSLOG_flag) {	\
    char syslogbuf[512]; \
    int facility = 1; \
    int severity; \
    int offset = 0; \
    time_t now; \
    struct tm localtm; \
    switch (l) { \
    case L_ALERT: severity=1; break; \
    case L_NOTICE: severity=5; break; \
    case L_INFO: severity=6; break; \
    default: severity=7; \
    } \
    snprintf(syslogbuf+offset, sizeof(syslogbuf)-offset, "<%d>", (facility<<3)+severity); \
    offset = strlen(syslogbuf); \
    time(&now); \
    localtime_r(&now, &localtm); \
    strftime(syslogbuf+offset, sizeof(syslogbuf)-offset, "%b %e %T ", &localtm); \
    offset = strlen(syslogbuf); \
    /*snprintf(syslogbuf+offset, sizeof(syslogbuf)-offset, "%s ", device_id);*/ \
    snprintf(syslogbuf+offset, sizeof(syslogbuf)-offset, "%s %6s @%s:L%d ", device_id, _level_str(l), __PRETTY_FUNCTION__, (int)__LINE__); \
    offset = strlen(syslogbuf); \
    snprintf(syslogbuf+offset, sizeof(syslogbuf)-offset, __VA_ARGS__); \
    _udpsend(SYSLOG_host, SYSLOG_port, syslogbuf, strlen(syslogbuf)); \
  }

#else
#define SYSLOG(l,...) {}
#endif

#define ENTER(l)  int enterlevel=l; if (debug>=l) __DEBUG__(l,">%s", __func__)
#define LEAVE  __DEBUG__(enterlevel,"<%s", __func__)
#define RETURN(x) LEAVE; return(x)


#define __DEBUG__(l,...) { \
    if(debug>=l){ \
      DBGMILLIS(l); \
      if (debug_flush) DBG.flush();   \
      if (DBGWAIT>0) {delay(DBGWAIT);} \
      DBGPRINTF(__VA_ARGS__); \
      DBGPRINTLN();	      \
      if (debug_flush) DBG.flush();		\
      SYSLOG(l,__VA_ARGS__);\
    } \
  }

bool inline_fresh = true;
#define __DEBUGINLINE__(l,fmt,...) {		\
    if(debug>=l){ \
      if (inline_fresh) {DBGMILLIS(l);}  \
      DBGPRINTF(fmt,__VA_ARGS__);		\
      inline_fresh = (strchr(fmt,'\n')?true:false);	\
      if (inline_fresh) { \
	if (debug_flush) DBG.flush();  \
	if (DBGWAIT>0) {delay(DBGWAIT);}	\
      }						\
      SYSLOG(l,__VA_ARGS__);\
    } \
  }

#define __LEAF_DEBUG__(l,...) { \
    if(debug>=l){ \
      DBGMILLIS(l); \
      if (debug_flush) DBG.flush();   \
      if (DBGWAIT>0) {delay(DBGWAIT);} \
      DBGPRINTF("[%s] ", leaf_name.c_str());	\
      DBGPRINTF(__VA_ARGS__); \
      DBGPRINTLN();	      \
      if (debug_flush) DBG.flush();		\
      SYSLOG(l,__VA_ARGS__);\
    } \
  }

#define ALERT( ...) __DEBUG__(L_ALERT ,__VA_ARGS__)

#if MAX_DEBUG_LEVEL >= L_NOTICE
#define NOTICE(...) __DEBUG__(L_NOTICE,__VA_ARGS__)
#else
#define NOTICE(...) {}
#endif

#if MAX_DEBUG_LEVEL >= L_INFO
#define INFO(  ...) __DEBUG__(L_INFO  ,__VA_ARGS__)
#else
#define INFO(...) {}
#endif

#if MAX_DEBUG_LEVEL >= L_DEBUG
#define DEBUG( ...) __DEBUG__(L_DEBUG ,__VA_ARGS__)
#else
#define DEBUG(...) {}
#endif

#define LEAF_ENTER(l)  int enterlevel=l; unsigned long entertime=millis(); if (debug>=l) __LEAF_DEBUG__(l,">%s", __func__)
#define LEAF_LEAVE  unsigned long leavetime=millis();\
  __LEAF_DEBUG__(enterlevel,"<%s", __func__);\
  if ((leavetime-entertime) > debug_slow) {  \
  bool save=debug_lines;		     \
  debug_lines = true;			     \
  LEAF_NOTICE("SLOW EXECUTION %lums", (leavetime-entertime));	\
  debug_lines = save;							\
  }									\


#define LEAF_RETURN(x)  LEAF_LEAVE;return(x);
#define LEAF_VOID_RETURN  LEAF_LEAVE;return;


#define LEAF_ALERT( ...) __LEAF_DEBUG__(L_ALERT ,__VA_ARGS__)

#if MAX_DEBUG_LEVEL >= L_NOTICE
#define LEAF_NOTICE(...) __LEAF_DEBUG__(L_NOTICE,__VA_ARGS__)
#else
#define LEAF_NOTICE(...) {}
#endif

#if MAX_DEBUG_LEVEL >= L_INFO
#define LEAF_INFO(...)   __LEAF_DEBUG__(L_INFO  ,__VA_ARGS__)
#else
#define LEAF_INFO(...) {}
#endif

#if MAX_DEBUG_LEVEL >= L_DEBUG
#define LEAF_DEBUG(...)  {}
#else
#define LEAF_DEBUG(...) {}
#endif

#define STATE(s) (s:"HIGH":"LOW")
#define TRUTH(b) (b?"TRUE":"FALSE")
#define truth(b) (b?"true":"false")

void DumpHex(int level, const char *leader, const void* data, size_t size) {
	if (debug < level) return;
	int save_lines = debug_lines;
	debug_lines = 0;

	char ascii[17];
	size_t i, j;
	int leader_len = strlen(leader)+2;
	ascii[16] = '\0';
	DBGMILLIS(level);
	DBGPRINTF("%s: ", leader);
	for (i = 0; i < size; ++i) {
		DBGPRINTF("%02X ", ((unsigned char*)data)[i]);
		if (((unsigned char*)data)[i] >= ' ' && ((unsigned char*)data)[i] <= '~') {
			ascii[i % 16] = ((unsigned char*)data)[i];
		} else {
			ascii[i % 16] = '.';
		}
		if ((i+1) % 8 == 0 || i+1 == size) {
			DBGPRINTF(" ");
			if ((i+1) % 16 == 0) {
				DBGPRINTF("|  %s \n", ascii);
				if (i+1 != size) {
					for (int ldr=0;ldr<leader_len;ldr++) {
						DBGPRINTF(" ");
					}
				}
			} else if (i+1 == size) {
				ascii[(i+1) % 16] = '\0';
				if ((i+1) % 16 <= 8) {
					DBGPRINTF(" ");
				}
				for (j = (i+1) % 16; j < 16; ++j) {
					DBGPRINTF("   ");
				}
				DBGPRINTF("|  %s \n", ascii);
			}
		}
	}
	debug_lines = save_lines;
}

// Local Variables:
// mode: C++
// c-basic-offset: 2
// End:
