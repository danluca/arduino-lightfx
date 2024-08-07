/*
    _   ___ ___  _   _ ___ _  _  ___  _    ___   ___ 
   /_\ | _ \   \| | | |_ _| \| |/ _ \| |  / _ \ / __|
  / _ \|   / |) | |_| || || .` | (_) | |_| (_) | (_ |
 /_/ \_\_|_\___/ \___/|___|_|\_|\___/|____\___/ \___|
                                                     
  Log library for Arduino
  version 1.1.1
  https://github.com/thijse/Arduino-Log

Licensed under the MIT License <http://opensource.org/licenses/MIT>.

Permission is hereby  granted, free of charge, to any  person obtaining a copy
of this software and associated  documentation files (the "Software"), to deal
in the Software  without restriction, including without  limitation the rights
to  use, copy,  modify, merge,  publish, distribute,  sublicense, and/or  sell
copies  of  the Software,  and  to  permit persons  to  whom  the Software  is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE  IS PROVIDED "AS  IS", WITHOUT WARRANTY  OF ANY KIND,  EXPRESS OR
IMPLIED,  INCLUDING BUT  NOT  LIMITED TO  THE  WARRANTIES OF  MERCHANTABILITY,
FITNESS FOR  A PARTICULAR PURPOSE AND  NONINFRINGEMENT. IN NO EVENT  SHALL THE
AUTHORS  OR COPYRIGHT  HOLDERS  BE  LIABLE FOR  ANY  CLAIM,  DAMAGES OR  OTHER
LIABILITY, WHETHER IN AN ACTION OF  CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE  OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#include "ArduinoLog.h"

#ifndef DISABLE_LOGGING
rtos::Mutex serial_mtx;
static const char unknown[] PROGMEM = "N/A";
#endif

void Logging::begin(int level, Print* logOutput, bool showLevel)
{
#ifndef DISABLE_LOGGING
	setLevel(level);
	setShowLevel(showLevel);
	_logOutput = logOutput;
#endif
}

void Logging::setLevel(int level)
{
#ifndef DISABLE_LOGGING
	_level = constrain(level, LOG_LEVEL_SILENT, LOG_LEVEL_VERBOSE);
#endif
}

int Logging::getLevel() const
{
#ifndef DISABLE_LOGGING
	return _level;
#else
	return 0;
#endif
}

void Logging::setShowLevel(bool showLevel)
{
#ifndef DISABLE_LOGGING
	_showLevel = showLevel;
#endif
}

bool Logging::getShowLevel() const
{
#ifndef DISABLE_LOGGING
	return _showLevel;
#else
	return false;
#endif
}

void Logging::setPrefix(printfunction f)
{
#ifndef DISABLE_LOGGING
	_prefix = f;
#endif
}

void Logging::clearPrefix()
{
#ifndef DISABLE_LOGGING
	_prefix = nullptr;
#endif
}

void Logging::setSuffix(printfunction f)
{
#ifndef DISABLE_LOGGING
	_suffix = f;
#endif
}

void Logging::clearSuffix()
{
#ifndef DISABLE_LOGGING
	_prefix = nullptr;
#endif
}

void Logging::setAdditionalFormatting(printFmtFunc f) {
#ifndef DISABLE_LOGGING
    _addtlPrintFormat = f;
#endif
}

void Logging::clearAdditionalFormatting() {
#ifndef DISABLE_LOGGING
    _addtlPrintFormat = nullptr;
#endif
}

void Logging::print(const __FlashStringHelper *format, va_list args)
{
#ifndef DISABLE_LOGGING	  	
	PGM_P p = reinterpret_cast<PGM_P>(format);
// This copy is only necessary on some architectures (x86) to change a passed
// array in to a va_list.
#ifdef __x86_64__
	va_list args_copy;
	va_copy(args_copy, args);
#endif
	char c = pgm_read_byte(p++);
	for(;c != 0; c = pgm_read_byte(p++))
	{
		if (c == '%')
		{
			c = pgm_read_byte(p++);
#ifdef __x86_64__
			printFormat(c, &args_copy);
#else
			printFormat(c, &args);
#endif
		}
		else
		{
			_logOutput->print(c);
		}
	}
#ifdef __x86_64__
	va_end(args_copy);
#endif
#endif
}

/**
 * Local utility to count the significant nibbles (4-bit sets) of a value (converted to unsigned long) - helpful in printing hex
 * @param ul value to count significant nibbles of
 * @return how many nibbles are non-zero in the hex representation of this number
 */
uint8_t Logging::countSignificantNibbles(unsigned long ul) {
    const uint8_t szLong = sizeof(unsigned long);
    ulong mask = 0x0F << (szLong*8-4);
    uint8_t sigNibbles = 0;
    for (uint x=0; x<szLong; x++) {
        if (ul & mask) {
            sigNibbles = (szLong-x)*2;
            break;
        }
        mask = mask >> 4;
        if (ul & mask) {
            sigNibbles = (szLong-x)*2-1;
            break;
        }
        mask = mask >> 4;
    }
    return sigNibbles;
}

void Logging::print(const char *format, va_list args) {
#ifndef DISABLE_LOGGING	  	
// This copy is only necessary on some architectures (x86) to change a passed
// array in to a va_list.
#ifdef __x86_64__
	va_list args_copy;
	va_copy(args_copy, args);
#endif
	for (; *format != 0; ++format)
	{
		if (*format == '%')
		{
			++format;
#ifdef __x86_64__
			printFormat(*format, &args_copy);
#else
			printFormat(*format, &args);
#endif
		}
		else
		{
			_logOutput->print(*format);
		}
	}
#ifdef __x86_64__
	va_end(args_copy);
#endif
#endif
}

void Logging::printFormat(const char format, va_list *args) {
#ifndef DISABLE_LOGGING
	if (format == '\0') return;
    switch (format) {
        case '%':
		    _logOutput->print(format);
            break;
        case 's': {
            char *s = (char *) va_arg(*args, int);
            _logOutput->print(s);
            break;
        }
        case 'S': {
            __FlashStringHelper *s = (__FlashStringHelper *) va_arg(*args, int);
            _logOutput->print(s);
            break;
        }
        case 'd':
        case 'i':
            _logOutput->print(va_arg(*args, int), DEC);
            break;
        case 'D':
        case 'F':
		    _logOutput->print(va_arg(*args, double));
            break;
        case 'x':
		    _logOutput->print(va_arg(*args, int), HEX);
            break;
        case 'X': {
            _logOutput->print("0x");
            unsigned long ul = va_arg(*args, unsigned long);
            if (countSignificantNibbles(ul)%2)
                _logOutput->print('0');
            _logOutput->print(ul, HEX);
            break;
        }
        case 'p': {
            Printable *obj = (Printable *) va_arg(*args, int);
            _logOutput->print(*obj);
            break;
        }
        case 'b':
		    _logOutput->print(va_arg(*args, int), BIN);
            break;
        case 'B': {
    		_logOutput->print("0b");
	    	_logOutput->print(va_arg(*args, int), BIN);
            break;
        }
        case 'l':
		    _logOutput->print(va_arg(*args, long), DEC);
            break;
        case 'L':
		    _logOutput->print(va_arg(*args, long), HEX);
            break;
        case 'u':
		    _logOutput->print(va_arg(*args, unsigned long), DEC);
            break;
        case 'U':
		    _logOutput->print(va_arg(*args, unsigned long), HEX);
            break;
        case 'c':
		    _logOutput->print((char) va_arg(*args, int));
            break;
        case 'C': {
            char c = (char) va_arg( *args, int );
            if (c>=0x20 && c<0x7F) {
                _logOutput->print(c);
            } else {
                _logOutput->print("0x");
                if (c<0xF) _logOutput->print('0');
                _logOutput->print(c, HEX);
            }
            break;
        }
        case 't': {
            if (va_arg(*args, int) == 1)
                _logOutput->print("T");
            else
                _logOutput->print("F");
            break;
        }
        case 'T': {
            if (va_arg(*args, int) == 1)
                _logOutput->print(F("true"));
            else
                _logOutput->print(F("false"));
            break;
        }
        default:
            if (_addtlPrintFormat == nullptr)
		        _logOutput->print("n/s");
            else
                _addtlPrintFormat(_logOutput, format, args);
            break;
    }
#endif
}
 
void Logging::logThreadInfo(osThreadId_t threadId) {
    // Refs: rtx_lib.h - #define os_thread_t osRtxThread_t
    //       rtx_os.h  - typedef struct osRtxThread_s { } osRtxThread_t

    if (!threadId) return;

    os_thread_t * tcb = (os_thread_t *) threadId;

    uint32_t stackSize = osThreadGetStackSize(threadId);
    uint32_t stackUsed = osThreadGetStackSpace(threadId);

    if (LOG_LEVEL_VERBOSE > _level) return;
    mbed::ScopedLock<rtos::Mutex> lock(serial_mtx);
    if (_prefix != nullptr)
        _prefix(_logOutput, LOG_LEVEL_VERBOSE);
    if (_showLevel) {
        _logOutput->print(levels[LOG_LEVEL_VERBOSE - 1]);
        _logOutput->print(": ");
    }
    print(F("Thread:: name: %s id: %x entry: %x"), osThreadGetName(threadId) ? osThreadGetName(threadId) : unknown, (int)threadId, (int)tcb->thread_addr);
    _logOutput->print(CR);
    print(F("  Stack:: start: %U end: %U size: %u used: %u free: %u"), (uint32_t)tcb->stack_mem, (uint32_t)(uint8_t *)tcb->stack_mem + stackSize, stackSize, stackUsed, stackSize-stackUsed);
    if (_suffix != nullptr) {
        _suffix(_logOutput, LOG_LEVEL_VERBOSE);
    }
    _logOutput->print(CR);
}

void Logging::logAllThreadInfo() {
    // Refs: mbed_stats.c - mbed_stats_stack_get_each()

    uint32_t     threadCount = osThreadGetCount();
    osThreadId_t threads[threadCount]; // g++ will throw a -Wvla on this, but it is likely ok.

    // osThreadId_t * threads = malloc(sizeof(osThreadId_t) * threadCount);
    // MBED_ASSERT(NULL != threads);

    memset(threads, 0, threadCount * sizeof(osThreadId_t));

    // This will probably only work if the number of threads remains constant
    // (i.e. the number of thread control blocks remains constant)
    //
    // This is probably the case on a deterministic realtime embedded system
    // with limited SRAM.

    osKernelLock();

    threadCount = osThreadEnumerate(threads, threadCount);

    for (uint32_t i = 0; i < threadCount; i++)
    {
        // There seems to be a Heisenbug when calling print_thread_info()
        // inside of osKernelLock()!

        // This error may appear on the serial console:
        // mbed assertation failed: os_timer->get_tick() == svcRtxKernelGetTickCount(), file: .\mbed-os\rtos\TARGET_CORTEX\mbed_rtx_idle.c

        // The RTOS seems to be asserting an idle constraint violation due
        // to the slowness of sending data through the serial port, but it
        // does not happen consistently.
        logThreadInfo(threads[i]);
    }

    osKernelUnlock();

    // free(threads);
}

void Logging::logHeapAndStackInfo() {
    extern unsigned char * mbed_heap_start;
    extern uint32_t        mbed_heap_size;
    extern uint32_t        mbed_stack_isr_size;
    extern unsigned char * mbed_stack_isr_start;

    mbed_stats_heap_t      heap_stats;

    mbed_stats_heap_get(&heap_stats);

    printLevel(LOG_LEVEL_VERBOSE, true, "     heap ( start: %x end: %x size: %x used: %x free: %x )  alloc ( ok: %x fail: %x ) isr_stack ( start: %x end: %x size: %x )",
               mbed_heap_start, mbed_heap_start + mbed_heap_size, mbed_heap_size, heap_stats.max_size, mbed_heap_size-heap_stats.current_size, heap_stats.alloc_cnt, heap_stats.alloc_fail_cnt, mbed_stack_isr_start, mbed_stack_isr_start + mbed_stack_isr_size, mbed_stack_isr_size);

}

Logging Log = Logging();