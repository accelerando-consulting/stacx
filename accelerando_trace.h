/* 
 * Macros for consistent use of trace output
 *
 * You should define DBG to be the stream you want to use (default: Serial)
 */

#define L_ALERT 0
#define L_NOTICE 1
#define L_INFO 2
#define L_DEBUG 3

#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL L_NOTICE
#endif

int debug = DEBUG_LEVEL;
bool debug_lines = false;
bool debug_flush = false;
int DBGWAIT = 0;


#ifndef DBG
#define DBG Serial
#endif
#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL L_NOTICE
#endif

#if USE_BT_CONSOLE
#define DBGPRINTF(...) { 		        \
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



#ifdef SYSLOG_flag

const unsigned int SYSLOG_port = 514;
WiFiUDP UDP;

void _udpsend(char *dst, unsigned int port, char *buf, unsigned int len)
{
  //Serial.print("udpsend "); Serial.print(dst); Serial.print(":");Serial.print(port);Serial.print(" => "); Serial.println(buf);

  static bool udpready = false;
  static IPAddress syslogIP;
  if (!udpready) {
    if (UDP.begin(SYSLOG_port)) {
      udpready = true;
    }
    else {
      return;
    }
    WiFi.hostByName(dst, syslogIP);
  }
  UDP.beginPacket(syslogIP, port);
  UDP.write((uint8_t *)buf, len);
  UDP.endPacket();
}


#define SYSLOG(l,...)  \
  if (SYSLOG_flag && (SYSLOG_host != "")) {	\
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
    getLocalTime(&localtm); \
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
#define NOTICE(...) __DEBUG__(L_NOTICE,__VA_ARGS__)
#define INFO(  ...) __DEBUG__(L_INFO  ,__VA_ARGS__)
#define DEBUG( ...) __DEBUG__(L_DEBUG ,__VA_ARGS__)

#define LEAF_ENTER(l)  int enterlevel=l; if (debug>=l) __LEAF_DEBUG__(l,">%s", __func__)
#define LEAF_LEAVE  __LEAF_DEBUG__(enterlevel,"<%s", __func__)

#define LEAF_ALERT( ...) __LEAF_DEBUG__(L_ALERT ,__VA_ARGS__)
#define LEAF_NOTICE(...) __LEAF_DEBUG__(L_NOTICE,__VA_ARGS__)
#define LEAF_INFO(...)   __LEAF_DEBUG__(L_INFO  ,__VA_ARGS__)
#define LEAF_DEBUG(...)  __LEAF_DEBUG__(L_DEBUG ,__VA_ARGS__)

#define STATE(s) (s:"HIGH":"LOW")
#define TRUTH(b) (b?"TRUE":"FALSE")
#define truth(b) (b?"true":"false")


// Local Variables:
// mode: C++
// c-basic-offset: 2
// End:
