
#define _POSIX_C_SOURCE 200809L

#if defined(__APPLE__)
#define _SC_NPROCESSORS_ONLN 58
// I have absolutely no idea why I have to define it
// and clang can't just figure out that it should just work
#endif

#ifdef __linux__
#include <features.h>
#include <bits/time.h>
#endif

#define NOB_IMPLEMENTATION
#include "../3rd-party/nob.h"

void set_minimal_nob_log_level(Nob_Log_Level level)
{
    nob_minimal_log_level = level;
}