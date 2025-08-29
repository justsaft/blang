
#ifndef _OUTPUT_H
#define _OUTPUT_H

#include <stdbool.h>

enum Stage {
    COMPILE,
    LINK
};

char* swap_extension(const char* filename, const char* new_extension);
char* chop_extension(const char* filename);
char* strconcat(const char* s1, const char* s2);
bool run_clang(const char* output_file, const char* args);
bool write_ll_file(const char* original_file, const char* ir, size_t ir_size);


#endif