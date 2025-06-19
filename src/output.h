
#ifndef _OUTPUT_H
#define _OUTPUT_H

enum Stage {
    COMPILE,
    LINK
};

char* swap_extension(const char* filename, const char* new_extension);
char* chop_extension(const char* filename);
char* strconcat(const char* s1, const char* s2);
/* void replace_char(char* str, char old_char, char new_char); */
bool run_clang(const char* output_file, const char* original_file, int stage);



#endif