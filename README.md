GetTimeSinceProcessStart v1.0.0
=====

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

### Intended audience
Developers focused on  high-performance  software who want to
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

### Usage
To use this library, define GTSPS_IMPLEMENTATION in a single C or C++
file before including this header to generate the implementation.

```cpp
// Example: Including and using the library in a C++ file
#include <stdio.h>
#define GTSPS_IMPLEMENTATION
#include "GetTimeSinceProcessStart.h"
```

In a C++ project you can place the content of the library in a namespace of your
choice:
```cpp
#define GTSPS_NAMESPACE YourCoolNamespace
```

The implementation outputs any error to stderr using fprintf,  but if you prefer
to disable any logging define  the following (the library returns 0.0 in case of
error):
```cpp
#define GTSPS_DONT_LOG_ERRORS
```

If instead you prefer to divert the errors logging to something else:
```cpp
#define GTSPS_LOG_ERROR(str) YourCoolLoggingFunction(str)
// The default definition is:
// #define GTSPS_LOG_ERROR(str) fprintf(stderr, str)
```

To test if the  library works,  temporarily insert some  known wait time  in the
creation of a global symbol and compare against a normal run. For example:

```cpp
#define GTSPS_IMPLEMENTATION
#include "GetTimeSinceProcessStart.h"

#warning remove me!!!
int sooooTiredAlready = usleep(5000000); //< Sleep fot 5 seconds

int main() {
   double timeInSeconds = GetTimeSinceProcessStart();
   printf("Startup time %f seconds\n", timeInSeconds);

   [...]
}
```

Beware the first measurement after recompiling an executable tends to be longer,
due to caching, security scanning, and other OS checks. So, run the test several
times.

Credits
-------
Developed by [Max Liani](https://maxliani.wordpress.com/)
