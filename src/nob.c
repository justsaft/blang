#define _POSIX_C_SOURCE 200809L
#define _CRT_SECURE_NO_WARNINGS
#define NOB_IMPLEMENTATION
#include "../3rd-party/nob.h"

void set_minimal_nob_log_level(Nob_Log_Level level)
{
    nob_minimal_log_level = level;
}