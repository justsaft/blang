
#ifndef _COMMON_HPP
#define _COMMON_HPP

#include "types.hpp"



void compilation_error(Returns e, const char* compiler_file = nullptr, const int file_line = -1);

#define Compilation_error(e) do { compilation_error((e), __FILE__, __LINE__); } while (false)
#define Filename input_files[file]

#define ENABLE_FILE_DELETIONS
#undef ENABLE_FILE_DELETIONS


#endif