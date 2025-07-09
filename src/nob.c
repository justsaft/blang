
#define _CRT_SECURE_NO_WARNINGS
#define NOB_IMPLEMENTATION
#include "../3rd-party/nob.h"


bool nob_delete_file_silent(const char* path)
{
#ifdef _WIN32
    if (!DeleteFileA(path)) {
        nob_log(NOB_ERROR, "Could not delete file %s: %s", path, nob_win32_error_message(GetLastError()));
        return false;
    }
    return true;
#else
    if (remove(path) < 0) {
        nob_log(NOB_ERROR, "Could not delete file %s: %s", path, strerror(errno));
        return false;
    }
    return true;
#endif // _WIN32
}