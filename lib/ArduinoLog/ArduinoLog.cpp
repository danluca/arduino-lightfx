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
//#include <malloc.h>
/** One character for each LOG_LEVEL_* definition  */
const char* Logging::levels PROGMEM = "FEWITV";
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

/**
 * This method and following few aren't really in the basic domain of providing logging capabilities to the system.
 * <p>Nonetheless, the stats they provide took deeper advantage of the logging infrastructure elements for an enhanced
 * look & feel - multi-line, spaced, information about threads, memory, system.</p>
 * @param threadId
 */
void Logging::logThreadInfo(osThreadId_t threadId) {
#ifndef DISABLE_LOGGING
    // Refs: ~\.platformio\packages\framework-arduino-mbed\libraries\mbed-memory-status\mbed_memory_status.cpp#print_thread_info
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
    print(F("    Stack:: start: %U end: %U size: %u used: %u free: %u"), (uint32_t)tcb->stack_mem, (uint32_t)(uint8_t *)tcb->stack_mem + stackSize, stackSize, stackUsed, stackSize-stackUsed);
    if (_suffix != nullptr) {
        _suffix(_logOutput, LOG_LEVEL_VERBOSE);
    }
    _logOutput->print(CR);
#endif
}

void Logging::logAllThreadInfo() {
#ifndef DISABLE_LOGGING
    // Refs: ~\.platformio\packages\framework-arduino-mbed\libraries\mbed-memory-status\mbed_memory_status.cpp#print_all_thread_info
    // Refs: mbed_stats.c - mbed_stats_stack_get_each()
    if (LOG_LEVEL_VERBOSE > _level) return;
    mbed::ScopedLock<rtos::Mutex> lock(serial_mtx);
    if (_prefix != nullptr)
        _prefix(_logOutput, LOG_LEVEL_VERBOSE);
    if (_showLevel) {
        _logOutput->print(levels[LOG_LEVEL_VERBOSE - 1]);
        _logOutput->print(": ");
    }
    print(F("THREAD INFO"));

    uint32_t     threadCount = osThreadGetCount();
    osThreadId_t threads[threadCount]; // g++ will throw a -Wvla on this, but it is likely ok.

    memset(threads, 0, threadCount * sizeof(osThreadId_t));
    threadCount = osThreadEnumerate(threads, threadCount);
    for (uint32_t i = 0; i < threadCount; i++) {
        osThreadId_t threadId = threads[i];
        if (!threadId) continue;

        os_thread_t * tcb = (os_thread_t *) threadId;
        uint32_t stackSize = osThreadGetStackSize(threadId);
        uint32_t stackUsed = osThreadGetStackSpace(threadId);

        _logOutput->print(CR);
        print(F("[%u] Thread:: name: %s id: %X entry: %X"), i, osThreadGetName(threadId) ? osThreadGetName(threadId) : unknown, (int)threadId, (int)tcb->thread_addr);
        _logOutput->print(CR);
        print(F("    Stack:: start: 0x%U end: 0x%U size: %u used: %u free: %u"), (uint32_t)tcb->stack_mem, (uint32_t)(uint8_t *)tcb->stack_mem + stackSize, stackSize, stackUsed, stackSize-stackUsed);
    }
    // CPU timing - ref: https://github.com/ARMmbed/mbed-os-example-cpu-stats/blob/mbed-os-6.7.0/main.cpp
    mbed_stats_cpu_t statsCPU;
    mbed_stats_cpu_get(&statsCPU);
    _logOutput->print(CR);
    print(F("CPU TIME"));
    _logOutput->print(CR);
    print(F("    CPU timing(us): up %u, idle %u, sleep %u, deepSleep %u"),
               statsCPU.uptime, statsCPU.idle_time, statsCPU.sleep_time, statsCPU.deep_sleep_time);
    if (_suffix != nullptr) {
        _suffix(_logOutput, LOG_LEVEL_VERBOSE);
    }
    _logOutput->print(CR);
#endif
}

void Logging::logHeapAndStackInfo() {
#ifndef DISABLE_LOGGING
    // Refs: ~\.platformio\packages\framework-arduino-mbed\libraries\mbed-memory-status\mbed_memory_status.cpp#print_heap_and_isr_stack_info
    if (LOG_LEVEL_VERBOSE > _level) return;
    mbed::ScopedLock<rtos::Mutex> lock(serial_mtx);

    extern unsigned char * mbed_heap_start;
    extern uint32_t        mbed_heap_size;
    extern uint32_t        mbed_stack_isr_size;
    extern unsigned char * mbed_stack_isr_start;
//    extern char __StackLimit, __bss_end__;

    if (_prefix != nullptr)
        _prefix(_logOutput, LOG_LEVEL_VERBOSE);
    if (_showLevel) {
        _logOutput->print(levels[LOG_LEVEL_VERBOSE - 1]);
        _logOutput->print(": ");
    }
    print(F("HEAP & ISR STACK INFO"));

    mbed_stats_heap_t      heap_stats;
    mbed_stats_heap_get(&heap_stats);
    //another way to get free heap
//    uint32_t totalHeap = &__StackLimit - &__bss_end__;      //this matches mbed_heap_size
//    uint32_t freeHeap = totalHeap - mallinfo().uordblks;    //this matches mbed_heap_size-heap_stats.current_size-heap_stats.overhead_size

    _logOutput->print(CR);
    print(F("    Heap:: start: %X end: %X size: %u used: %u free: %u"), mbed_heap_start, mbed_heap_start+mbed_heap_size, mbed_heap_size,
          heap_stats.max_size, mbed_heap_size-heap_stats.current_size-heap_stats.overhead_size);
    _logOutput->print(CR);
    print(F("           maxUsed: %u, currentUse: %u, totalAllocated: %u, reserved: %u, overhead: %u"), heap_stats.max_size, heap_stats.current_size, heap_stats.total_size, heap_stats.reserved_size, heap_stats.overhead_size);
    _logOutput->print(CR);
    print(F("    Alloc:: ok: %u fail: %u"), heap_stats.alloc_cnt, heap_stats.alloc_fail_cnt);
    _logOutput->print(CR);
    print(F("    ISR_Stack:: start: %X end: %X size: %u"), mbed_stack_isr_start, mbed_stack_isr_start+mbed_stack_isr_size, mbed_stack_isr_size);

    if (_suffix != nullptr) {
        _suffix(_logOutput, LOG_LEVEL_VERBOSE);
    }
    _logOutput->print(CR);
#endif
}

//#if !defined(MBED_CPU_STATS_ENABLED) || !defined(DEVICE_LPTICKER) || !defined(DEVICE_SLEEP)
//#error [NOT_SUPPORTED] test not supported
//#endif
//#if !defined(MBED_SYS_STATS_ENABLED)
//#error [NOT_SUPPORTED] System statistics not supported
//#endif

void Logging::logSystemInfo() {
#ifndef DISABLE_LOGGING
    // Refs: https://os.mbed.com/docs/mbed-os/v6.16/apis/mbed-statistics.html
    //       https://github.com/ARMmbed/mbed-os-example-sys-info/blob/mbed-os-6.7.0/main.cpp

    if (LOG_LEVEL_INFO > _level) return;
    mbed::ScopedLock<rtos::Mutex> lock(serial_mtx);

    // System info
    mbed_stats_sys_t statsSys;
    mbed_stats_sys_get(&statsSys);

    /* CPUID Register information
    [31:24]Implementer      0x41 = ARM
    [23:20]Variant          Major revision 0x0  =  Revision 0
    [19:16]Architecture     0xC  = Baseline Architecture
                            0xF  = Constant (Mainline Architecture)
    [15:4]PartNO            0xC20 = Cortex-M0
                            0xC60 = Cortex-M0+
                            0xC23 = Cortex-M3
                            0xC24 = Cortex-M4
                            0xC27 = Cortex-M7
                            0xD20 = Cortex-M23
                            0xD21 = Cortex-M33
    [3:0]Revision           Minor revision: 0x1 = Patch 1

     Compiler IDs
        ARM     = 1
        GCC_ARM = 2
        IAR     = 3

     Compiler versions:
       ARM: PVVbbbb (P = Major; VV = Minor; bbbb = build number)
       GCC: VVRRPP  (VV = Version; RR = Revision; PP = Patch)
       IAR: VRRRPPP (V = Version; RRR = Revision; PPP = Patch)
    */
    if (_prefix != nullptr)
        _prefix(_logOutput, LOG_LEVEL_INFO);
    if (_showLevel) {
        _logOutput->print(levels[LOG_LEVEL_INFO - 1]);
        _logOutput->print(": ");
    }
    print(F("SYSTEM INFO"));
    _logOutput->print(CR);
    print(F("    Mbed OS version %u"), statsSys.os_version);
    _logOutput->print(CR);
    print(F("    CPU ID %u"), statsSys.cpu_id);
    _logOutput->print(CR);
    print(F("    Compiler ID %u"), statsSys.compiler_id);
    _logOutput->print(CR);
    print(F("    Compiler Version %u"), statsSys.compiler_version);
    _logOutput->print(CR);
    print(F("  RAM"));
    /* RAM / ROM memory start and size information */
    for (int i = 0; i < MBED_MAX_MEM_REGIONS; i++) {
        if (statsSys.ram_size[i] != 0) {
            _logOutput->print(CR);
            print(F("    RAM%i: Start %X Size: %X"), i, statsSys.ram_start[i], statsSys.ram_size[i]);
        }
        if (statsSys.rom_size[i] != 0) {
            _logOutput->print(CR);
            print(F("    ROM%i: Start %X Size: %X"), i, statsSys.rom_start[i], statsSys.rom_size[i]);
        }
    }

    if (_suffix != nullptr) {
        _suffix(_logOutput, LOG_LEVEL_INFO);
    }
    _logOutput->print(CR);

#endif
}

Logging Log = Logging();