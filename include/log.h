//
// Created by Dan on 06.21.2023.
//
#ifndef LIGHTFX_LOG_H
#define LIGHTFX_LOG_H

//#define LOG_ENABLED

#define LEVEL_DEBUG  1
#define LEVEL_INFO   2
#define LEVEL_WARN   3
#define LEVEL_ERR    4

#ifdef LOG_ENABLED
    #include <Arduino.h>
    #include <LibPrintf.h>
    #include <Mutex.h>
    #include <rtos.h>

    #define LOG_LEVEL LEVEL_INFO
    extern rtos::Mutex sioMutex;
#else
    #define LOG_LEVEL   255
#endif

void log_setup();
void writeChar(char c);
//template<typename... Args> void logDebug(const char* format, Args... data);
//template<typename... Args> void logWarn(const char* format, Args... data);
//template<typename... Args> void logError(const char* format, Args... data);

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
  printf("I [%lu] [%s %d]: ", millis(), rtos::ThisThread::get_name(), (int)rtos::ThisThread::get_id());
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

#endif //LIGHTFX_LOG_H
