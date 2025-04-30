
#define _CRT_SECURE_NO_WARNINGS
#define NOB_IMPLEMENTATION
#include "../3rd-party/nob.h"

/* bool _return_defer(bool value)
{
    bool result = true;
    nob_return_defer(value);

defer:
    return result;
} */


//void _nob_append_sb_cstr(Nob_String_Builder* sb, const char* cstr, ...) {
//   va_list args;
//   va_start(args, cstr);
//   nob_sb_append_cstr(sb, cstr); // Fix: Pass `sb` directly instead of `&sb`
//   nob_sb_append_null(sb);
//   va_end(args);
//}