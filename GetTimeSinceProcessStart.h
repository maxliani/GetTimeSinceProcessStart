/* Copyright 2024 Max Liani
   SPDX-License-Identifier: MIT

   GetTimeSinceProcessStart v1.0.0
   Single header implementation of a function to determine the time in seconds that
   passed since the start of the current process.  This differs from measuring time
   starting at main() because  GetTimeSinceProcessStart() accounts for computations
   performed after the process creation,  including OS loader activities,  standard
   library initialization,  memory allocation,  DLL/DSO loading,  and static symbol
   construction.
   This library is primarily designed to be called at the very first line of main()
   to measure the time spent  initializing the process..  After that, add that time
   to other time measurement you retrieve using your favorite timer implementation.
   Do not use this library as a general purpose timer.

   Intended audience: Developers focused on  high-performance  software who want to
   benchmark and optimize  process startup times,  for precise execution statistics
   reporting measured from the program itself.
   Typically,  the process startup time is a few milliseconds quick,  but sometimes
   your code, or some library, executes complex initialization, like the allocation
   of memory pools, or the computation of tables, or complex global objects...
   You don't want performance degradation to creep into the init of the executable,
   unaccounted for. Measure the initialization time, keep a record of it, benchmark
   it as your program  evolves.  If the measurement changes over time,  profile the
   program form the start with your sampling profiler of choice, bisect the changes
   and figure out what can be done about it.

   GetTimeSinceProcessStart currently has specific implementations for the main OSs
   including: Windows, Linux and Mac OSX. It can be called from C and C++ modules.

   To use this library, declare "#define GTSPS_IMPLEMENTATION" in a single C or C++
   file before including this header to generate the implementation.

   // Example: Including and using the library in a C++ file
   #include <stdio.h>
   #define GTSPS_IMPLEMENTATION
   #include "GetTimeSinceProcessStart.h"

   In a C++ project you can place the content of the library in a namespace of your
   choice:
   #define GTSPS_NAMESPACE YourCoolNamespace

   The implementation outputs any error to stderr using fprintf,  but if you prefer
   to disable any logging define  the following (the library returns 0.0 in case of
   error):
   #define GTSPS_DONT_LOG_ERRORS

   If instead you prefer to divert the errors logging to something else:
   #define GTSPS_LOG_ERROR(str) YourCoolLoggingFunction(str)
   The default definition is:
   #define GTSPS_LOG_ERROR(str) fprintf(stderr, str)

   To test if the  library works,  temporarily insert some  known wait time  in the
   creation of a global symbol and compare against a normal run. For example:

   #define GTSPS_IMPLEMENTATION
   #include "GetTimeSinceProcessStart.h"

   #warning remove me!!!
   int sooooTiredAlready = usleep(5000000); //< Sleep fot 5 seconds

   int main() {
       double timeInSeconds = GetTimeSinceProcessStart();
       printf("Startup time %f seconds\n", timeInSeconds);

       [...]
   }

   Beware the first measurement after recompiling an executable tends to be longer,
   due to caching, security scanning, and other OS checks. So, run the test several
   times.
*/

#pragma once

#ifdef GTSPS_NAMESPACE
#define GTSPS_NAMESPACE_BEGIN namespace GTSPS_NAMESPACE {
#define GTSPS_NAMESPACE_END }
#else
#define GTSPS_NAMESPACE_BEGIN
#define GTSPS_NAMESPACE_END
#endif

#ifndef GTSPS_IMPLEMENTATION
GTSPS_NAMESPACE_BEGIN

// @brief  Measures the time passed since the process start, this accounts for any
//         time spends  loading and configuring the address space,  global symbols,
//         the standard library and any other library the executable links against.
//         Call this as you first line of main(...)  to get the exact  measurement
//         up to that point.  Do not use this as a general-purpose timer,  because
//         the system calls required to obtain  the measurement can be slower than
//         the alternatives.
//
// @return time in seconds, or 0.0 in case of error.
double GetTimeSinceProcessStart();

GTSPS_NAMESPACE_END

#else
///////////////////////////////////////////////////////////////////////////////
// Implementation

#if defined(_WIN32)
#   ifndef WIN32_LEAN_AND_MEAN
#       define WIN32_LEAN_AND_MEAN
#   endif
#   ifndef NOMINMAX
#       define NOMINMAX
#   endif
#   include <Windows.h>
#elif defined(linux) || defined(__linux__) || defined(__LINUX__)
#   include <unistd.h>             //< for sysconf()
#   include <stdio.h>
#   include <stdint.h>
#elif defined(__APPLE__) || defined(MACOSX) || defined(__MACOSX__)
#   include <unistd.h>             //< for getpid()
#   include <libproc.h>
#endif

#ifdef GTSPS_DONT_LOG_ERRORS
#    define GTSPS_LOG_ERROR(str)
#elif ! defined(GTSPS_LOG_ERROR)
#    define GTSPS_LOG_ERROR(str) fprintf(stderr, str)
#    include <stdio.h>
#endif

GTSPS_NAMESPACE_BEGIN

double GetTimeSinceProcessStart()
{
#if defined(_WIN32)
    double startTime = 0.0;
    {
        FILETIME creationTime, exitTime, kernelTime, userTime;
        if (!GetProcessTimes(GetCurrentProcess(), &creationTime, &exitTime, &kernelTime, &userTime))
        {
            GTSPS_LOG_ERROR("Error: Failed to call GetProcessTimes\n");
            return 0.0;
        }

        ULARGE_INTEGER converter;
        converter.LowPart = creationTime.dwLowDateTime;
        converter.HighPart = creationTime.dwHighDateTime;
        startTime = (double)converter.QuadPart / 10000000.0; // Convert 100-ns intervals to seconds
    }

    double currentTime = 0.0;
    {
        FILETIME systemTime;
        GetSystemTimeAsFileTime(&systemTime); // Current time

        ULARGE_INTEGER converter;
        converter.LowPart = systemTime.dwLowDateTime;
        converter.HighPart = systemTime.dwHighDateTime;
        currentTime = (double)converter.QuadPart / 10000000.0; // Convert 100-ns intervals to seconds
    }

    double timeInSeconds = currentTime - startTime;
    return timeInSeconds;

#elif defined(linux) || defined(__linux__) || defined(__LINUX__)
    double processStartTimeInSeconds = 0.0;
    if (FILE* file = fopen("/proc/self/stat", "r"))
    {
        // Start time is the 22nd field (https://man7.org/linux/man-pages/man5/proc_pid_stat.5.html)
        uint64_t startTime = 0;
        int decoded = fscanf(file, "%*s (%*s %*s %*s %*s %*s %*s %*s %*s %*s %*s "
                                   "%*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %zu",
                             &startTime);
        fclose(file);
        if (decoded != 1)
        {
            GTSPS_LOG_ERROR("Error: Failed decoding /proc/self/stat.\n");
            return 0.0;
        }

        // Thypically this is 100Hz, it has nothing to do with CPU clock, rather with
        // interrupts and how the OS probes the process and increment timers. It gives
        // this measurement a resolution of 10 ms.
        double clockTicksPerSecond = (double)sysconf(_SC_CLK_TCK); 

        // Read start time in clock ticks (since kernel start time), convert it to seconds
        processStartTimeInSeconds = (double)startTime / clockTicksPerSecond;
    }
    else
    {
        GTSPS_LOG_ERROR("Error: Failed to open /proc/self/stat.\n");
        return 0.0;
    }

    double kernelUpTimeInSeconds = 0.0;
    if (FILE* file = fopen("/proc/uptime", "r"))
    {
        // Total kernel uptime in seconds is the first field
        int decoded = fscanf(file, "%lf", &kernelUpTimeInSeconds);
        fclose(file);
        if (decoded != 1)
        {
            GTSPS_LOG_ERROR("Error: Failed decoding /proc/uptime.\n");
            return 0.0;
        }
    }
    else
    {
        GTSPS_LOG_ERROR("Error: Failed to open /proc/uptime.\n");
        return 0.0;
    }

    double timeInSeconds = kernelUpTimeInSeconds - processStartTimeInSeconds;
    return timeInSeconds;

#elif defined(__APPLE__) || defined(MACOSX) || defined(__MACOSX__)
    struct timeval startTime = {};
    {
        // Get current process info, including the startup time
        pid_t pid = getpid();
        struct proc_bsdinfo task_info = {};
        if (proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &task_info, sizeof(task_info)) <= 0)
        {
            GTSPS_LOG_ERROR("Error: proc_pidinfo failed");
            return 0.0;
        }
        startTime.tv_sec  = task_info.pbi_start_tvsec;
        startTime.tv_usec = task_info.pbi_start_tvusec;
    }

    struct timeval currentTime = {};
    gettimeofday(&currentTime, NULL);

    double timeInSeconds = (double)(currentTime.tv_sec  - startTime.tv_sec ) +            //< Seconds
                           (double)(currentTime.tv_usec - startTime.tv_usec) / 1000000.0; //< Microseconds
    return timeInSeconds;
#else
    #warning unsupported platform
    return 0.0;
#endif
}

GTSPS_NAMESPACE_END
#undef GTSPS_NAMESPACE_BEGIN
#undef GTSPS_NAMESPACE_END
#undef GTSPS_LOG_ERROR

#endif
