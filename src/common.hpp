
#ifndef _COMMON_HPP
#define _COMMON_HPP

#include "types.hpp"



void compilation_error(Returns e, const char* compiler_file = nullptr, const int file_line = -1);

#define local thread_local static

#define Compilation_error(e) do { compilation_error((e), __FILE__, __LINE__); } while (false)
#define CurrentFile input_files[file].filepath
#define CurrentFileState input_files[file].state
// #define CurrentFileState(x) do { input_files[file].state = (x); } while (false)

#define FILE_DELETIONS ENABLE
#define COMPILATION_RECAP ENABLE

#if defined(_WIN32)
#define strdup _strdup
#define popen _popen
#define pclose _pclose
#endif

#define falsefalse 0b00
#define falsetrue 0b01
#define truefalse 0b10
#define truetrue 0b11

#endif