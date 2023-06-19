#define LEVEL_DEBUG  1
#define LEVEL_INFO   2
#define LEVEL_WARN   3
#define LEVEL_ERR    4

#ifdef DEBUG
  #include <LibPrintf.h>
  #include <Mutex.h>
  #define LOG_LEVEL LEVEL_INFO
  rtos::Mutex sioMutex;
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
  //not locking a single char write as there is no point, locking must occur at higher level
#ifdef DEBUG
  Serial.write(c);
#endif
}

template<typename... Args> void logDebug(const char* format, Args... data) {
#if LOG_LEVEL <= LEVEL_DEBUG
  sioMutex.lock();
  printf("D [%lu] [%s %d]: ", millis(), rtos::ThisThread::get_name(), rtos::ThisThread::get_id());
  printf(format, data...);
  Serial.println();
  sioMutex.unlock();
#endif
}

template<typename... Args> void logInfo(const char* format, Args... data) {
#if LOG_LEVEL <= LEVEL_INFO
  sioMutex.lock();
  printf("I [%lu] [%s %d]: ", millis(), rtos::ThisThread::get_name(), rtos::ThisThread::get_id());
  printf(format, data...);
  Serial.println();
  sioMutex.unlock();
#endif
}

template<typename... Args> void logWarn(const char* format, Args... data) {
#if LOG_LEVEL <= LEVEL_WARN
  sioMutex.lock();
  printf("W [%lu] [%s]: ", millis(), rtos::ThisThread::get_name());
  // Serial.print("W [");
  // Serial.print(String(millis()));
  // Serial.print("]: ");
  printf(format, data...);
  Serial.println();
  sioMutex.unlock();
#endif
}

template<typename... Args> void logError(const char* format, Args... data) {
#if LOG_LEVEL <= LEVEL_ERR
  sioMutex.lock();
  printf("ERR [%lu] [%s]: ", millis(), rtos::ThisThread::get_name());
  printf(format, data...);
  Serial.println();
  sioMutex.unlock();
#endif
}
