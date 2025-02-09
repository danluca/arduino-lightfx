/*
    _   ___ ___  _   _ ___ _  _  ___  _    ___   ___
   /_\ | _ \   \| | | |_ _| \| |/ _ \| |  / _ \ / __|
  / _ \|   / |) | |_| || || .` | (_) | |_| (_) | (_ |
 /_/ \_\_|_\___/ \___/|___|_|\_|\___/|____\___/ \___|

  Log library for Arduino
  version 1.1.1
  https://github.com/thijse/Arduino-Log

Licensed under the MIT License <http://opensource.org/licenses/MIT>.

*/
#pragma once
#include <cinttypes>
#include <cstdarg>
#include <pico/mutex.h>
#include "Arduino.h"

typedef void (*printfunction)(Print*, int);

typedef void (*printfunction)(Print *, int);

typedef void (*printFmtFunc)(Print *, const char, va_list *);

#ifndef DISABLE_LOGGING
extern mutex_t serial_mtx;
#endif

// *************************************************************************
//  Uncomment line below to fully disable logging, and reduce project size
// ************************************************************************
//#define DISABLE_LOGGING

#define LOG_LEVEL_SILENT  0
#define LOG_LEVEL_FATAL   1
#define LOG_LEVEL_ERROR   2
#define LOG_LEVEL_WARNING 3
#define LOG_LEVEL_INFO    4
#define LOG_LEVEL_NOTICE  4
#define LOG_LEVEL_TRACE   5
#define LOG_LEVEL_VERBOSE 6

#define CR "\n"
#define LOGGING_VERSION 1_1_1

/**
 * Logging is a helper class to output informations over RS232.
 * If you know log4j or log4net, this logging class is more or less similar ;-) <br>
 * Different loglevels can be used to extend or reduce output
 * All methods are able to handle any number of output parameters.
 * All methods print out a formated string (like printf).<br>
 * To reduce output and program size, reduce loglevel.
 * 
 * Output format string can contain below wildcards. Every wildcard must be start with percent sign (\%)
 * 
 * ---- Wildcards
 * 
 * %s	replace with an string (char*)
 * %c	replace with an character
 * %d	replace with an integer value
 * %l	replace with an long value
 * %x	replace and convert integer value into hex
 * %X	like %x but combine with 0x123AB
 * %b	replace and convert integer value into binary
 * %B	like %x but combine with 0b10100011
 * %t	replace and convert boolean value into "t" or "f"
 * %T	like %t but convert into "true" or "false"
 * %L   like %l but output is in hex
 * %D,%F   replace with a decimal double value (float is upcasted)
 * %p   replace with Printable's arg own representation (arg must be a Printable)
 * %u   replace with unsigned long
 * %U   like %u but output is in hex
 * %C   like %c but non-ASCII chars are output in hex
 *
 * ---- Loglevels
 * 
 * 0 - LOG_LEVEL_SILENT     no output
 * 1 - LOG_LEVEL_FATAL      fatal errors
 * 2 - LOG_LEVEL_ERROR      all errors
 * 3 - LOG_LEVEL_WARNING    errors and warnings
 * 4 - LOG_LEVEL_INFO       errors, warnings and notices
 * 4 - LOG_LEVEL_NOTICE     Same as INFO, kept for backward compatibility
 * 5 - LOG_LEVEL_TRACE      errors, warnings, notices, traces
 * 6 - LOG_LEVEL_VERBOSE    all
 */

class Logging {
public:
    /** default Constructor */
    Logging()
#ifndef DISABLE_LOGGING
            : _level(LOG_LEVEL_SILENT),
              _showLevel(true), _continuation(false),
              _logOutput(NULL)
#endif
    {

    }

    /**
     * Initializing, must be called as first. Note that if you use
     * this variant of Init, you need to initialize the baud rate
     * yourself, if printer happens to be a serial port.
     *
     * \param level - logging levels <= this will be logged.
     * \param printer - place that logging output will be sent to.
     * \return void
     *
     */
    void begin(int level, Print *output, bool showLevel = true);

    /**
     * Set the log level.
     *
     * \param level - The new log level.
     * \return void
     */
    void setLevel(int level);

    /**
     * Get the log level.
     *
     * \return The current log level.
     */
    int getLevel() const;

    /**
     * Set whether to show the log level.
     *
     * \param showLevel - true if the log level should be shown for each log
     *                    false otherwise.
     * \return void
     */
    void setShowLevel(bool showLevel);

    /**
     * Get whether the log level is shown during logging
     *
     * \return true if the log level is be shown for each log
     *         false otherwise.
     */
    bool getShowLevel() const;

    /**
     * Sets a function to be called before each log command.
     *
     * \param f - The function to be called
     * \return void
     */
    void setPrefix(printfunction f);

    /**
     * clears prefix.
     *
     * \return void
     */
    void clearPrefix();

    /**
     * Sets a function to be called after each log command.
     *
     * \param f - The function to be called
     * \return void
     */
    void setSuffix(printfunction f);

    /**
     * clears suffix.
     *
     * \return void
     */
    void clearSuffix();

    /**
     * Sets a function to be called for additional formatting, in excess of the formats handeld by <code>printFormat</code>
     * @param f function to handle extra formats
     */
    void setAdditionalFormatting(printFmtFunc f);

    /**
     * Clears the additional formatting function
     */
    void clearAdditionalFormatting();

    inline void endContinuation() {
#ifndef DISABLE_LOGGING
        _continuation = false;
#endif
    }

    static uint8_t countSignificantNibbles(unsigned long ul);

    /**
     * Output a fatal error message. Output message contains
     * F: followed by original message
     * Fatal error messages are printed out at
     * loglevels >= LOG_LEVEL_FATAL
     *
     * \param msg format string to output
     * \param ... any number of variables
     * \return void
     */
    template<class T, typename... Args>
    void fatal(T msg, Args... args) {
#ifndef DISABLE_LOGGING
        printLevel(LOG_LEVEL_FATAL, false, msg, args...);
#endif
    }

    template<class T, typename... Args>
    void fatalln(T msg, Args... args) {
#ifndef DISABLE_LOGGING
        printLevel(LOG_LEVEL_FATAL, true, msg, args...);
#endif
    }

    /**
     * Output an error message. Output message contains
     * E: followed by original message
     * Error messages are printed out at
     * loglevels >= LOG_LEVEL_ERROR
     *
     * \param msg format string to output
     * \param ... any number of variables
     * \return void
     */
    template<class T, typename... Args>
    void error(T msg, Args... args) {
#ifndef DISABLE_LOGGING
        printLevel(LOG_LEVEL_ERROR, false, msg, args...);
#endif
    }

    template<class T, typename... Args>
    void errorln(T msg, Args... args) {
#ifndef DISABLE_LOGGING
        printLevel(LOG_LEVEL_ERROR, true, msg, args...);
#endif
    }

    /**
     * Output a warning message. Output message contains
     * W: followed by original message
     * Warning messages are printed out at
     * loglevels >= LOG_LEVEL_WARNING
     *
     * \param msg format string to output
     * \param ... any number of variables
     * \return void
     */
    template<class T, typename... Args>
    void warning(T msg, Args...args) {
#ifndef DISABLE_LOGGING
        printLevel(LOG_LEVEL_WARNING, false, msg, args...);
#endif
    }

    template<class T, typename... Args>
    void warningln(T msg, Args...args) {
#ifndef DISABLE_LOGGING
        printLevel(LOG_LEVEL_WARNING, true, msg, args...);
#endif
    }

    /**
     * Output a notice message. Output message contains
     * I: followed by original message
     * Notice messages are printed out at
     * loglevels >= LOG_LEVEL_NOTICE
     *
     * \param msg format string to output
     * \param ... any number of variables
     * \return void
     */
    template<class T, typename... Args>
    void notice(T msg, Args...args) {
#ifndef DISABLE_LOGGING
        printLevel(LOG_LEVEL_NOTICE, false, msg, args...);
#endif
    }

    template<class T, typename... Args>
    void noticeln(T msg, Args...args) {
#ifndef DISABLE_LOGGING
        printLevel(LOG_LEVEL_NOTICE, true, msg, args...);
#endif
    }

    template<class T, typename... Args>
    void info(T msg, Args...args) {
#ifndef DISABLE_LOGGING
        printLevel(LOG_LEVEL_INFO, false, msg, args...);
#endif
    }

    template<class T, typename... Args>
    void infoln(T msg, Args...args) {
#ifndef DISABLE_LOGGING
        printLevel(LOG_LEVEL_INFO, true, msg, args...);
#endif
    }

    /**
     * Output a trace message. Output message contains
     * T: followed by original message
     * Trace messages are printed out at
     * loglevels >= LOG_LEVEL_TRACE
     *
     * \param msg format string to output
     * \param ... any number of variables
     * \return void
    */
    template<class T, typename... Args>
    void trace(T msg, Args... args) {
#ifndef DISABLE_LOGGING
        printLevel(LOG_LEVEL_TRACE, false, msg, args...);
#endif
    }

    template<class T, typename... Args>
    void traceln(T msg, Args... args) {
#ifndef DISABLE_LOGGING
        printLevel(LOG_LEVEL_TRACE, true, msg, args...);
#endif
    }

    /**
     * Output a verbose message. Output message contains
     * V: followed by original message
     * Debug messages are printed out at
     * loglevels >= LOG_LEVEL_VERBOSE
     *
     * \param msg format string to output
     * \param ... any number of variables
     * \return void
     */
    template<class T, typename... Args>
    void verbose(T msg, Args... args) {
#ifndef DISABLE_LOGGING
        printLevel(LOG_LEVEL_VERBOSE, false, msg, args...);
#endif
    }

    template<class T, typename... Args>
    void verboseln(T msg, Args... args) {
#ifndef DISABLE_LOGGING
        printLevel(LOG_LEVEL_VERBOSE, true, msg, args...);
#endif
    }

private:
    void print(const char *format, va_list args);
    void print(const __FlashStringHelper *format, va_list args);
    void print(const Printable &obj, va_list args) {
#ifndef DISABLE_LOGGING
        _logOutput->print(obj);
#endif
    }

    void printFormat(const char format, va_list *args) const;

    template<class T>
    void printLevel(int level, bool cr, T msg, ...) {
#ifndef DISABLE_LOGGING
        if (level > _level) {
            return;
        }

        mutex_enter_blocking(&serial_mtx);

        if (level < LOG_LEVEL_SILENT) {
            level = LOG_LEVEL_SILENT;
        }

        if (!_continuation) {
            if (_prefix != nullptr) {
                _prefix(_logOutput, level);
            }

            if (_showLevel) {
                _logOutput->print(levels[level - 1]);
                _logOutput->print(": ");
            }
        }

        va_list args;
        va_start(args, msg);
        print(msg, args);

        if (_suffix != nullptr && !_continuation) {
            _suffix(_logOutput, level);
        }
        if (cr) {
            _logOutput->print(CR);
            _continuation = false;
        } else
            _continuation = true;
        mutex_exit(&serial_mtx);
#endif
    }

#ifndef DISABLE_LOGGING
    int _level;
    bool _showLevel;
    bool _continuation;
    Print *_logOutput;

    printfunction _prefix = nullptr;
    printfunction _suffix = nullptr;
    printFmtFunc _addtlPrintFormat = nullptr;
    static const char* levels;
#endif
};

extern Logging Log;