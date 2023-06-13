#define LEVEL_DEBUG  1
#define LEVEL_INFO   2
#define LEVEL_WARN   3
#define LEVEL_ERR    4

#ifdef DEBUG
  #include <LibPrintf.h>
  #define LOG_LEVEL LEVEL_INFO
#else 
  #define LOG_LEVEL   255
#endif

void log_setup() {
#ifdef DEBUG
  if (!Serial) {
    Serial.begin(115200);      // initialize serial communication
    while(!Serial) {}
    logInfo("========================================");
  }
#endif
}

void writeChar(char c) {
#ifdef DEBUG  
  Serial.write(c);
#endif
}

template<typename... Args> void logDebug(const char* format, Args... data) {
#if LOG_LEVEL <= LEVEL_DEBUG
  printf("D [%lu]: ", millis());
  printf(format, data...);
  Serial.println();
#endif
}

template<typename... Args> void logInfo(const char* format, Args... data) {
#if LOG_LEVEL <= LEVEL_INFO
  printf("I [%lu]: ", millis());
  // Serial.print("I [");
  // Serial.print(String(millis()));
  // Serial.print("]: ");
  printf(format, data...);
  Serial.println();
#endif
}

template<typename... Args> void logWarn(const char* format, Args... data) {
#if LOG_LEVEL <= LEVEL_WARN
  printf("W [%lu]: ", millis());
  // Serial.print("W [");
  // Serial.print(String(millis()));
  // Serial.print("]: ");
  printf(format, data...);
  Serial.println();
#endif
}

template<typename... Args> void logError(const char* format, Args... data) {
#if LOG_LEVEL <= LEVEL_ERR
  printf("ERR [%lu]: ", millis());
  printf(format, data...);
  Serial.println();
#endif
}
