#include "log.h"

#ifdef LOG_ENABLED
  rtos::Mutex sioMutex;
#endif

void log_setup() {
#ifdef LOG_ENABLED
  if (!Serial) {
    Serial.begin(115200);      // initialize serial communication
    while(!Serial) {}
    logInfo("========================================");
  }
#endif
}

void writeChar(char c) {
  //not locking a single char write as there is no point, locking must occur at higher level
#ifdef LOG_ENABLED
  Serial.write(c);
#endif
}

