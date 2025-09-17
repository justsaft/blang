
#define _CRT_SECURE_NO_WARNINGS
// To shut up MSVC

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "output.h"

#define CLANGC "clang", "-o"
#define LINKER "clang", "-o"

#define NOB_STRIP_PREFIX
#include "../3rd-party/nob.h"

void set_minimal_nob_log_level(Nob_Log_Level level);


// TODO: (no) linking with libc
//       -nostdlib: Disables linking with both libc and the standard startup files.
//       -nodefaultlibs : Disables linking with libc but still includes the startup files.
//       -llibstd++
//       ld -s hello.o crt2.o -o hello.exe libstdc++.a libgcc.a libmingw32.a libmingwex.a libmsvcrt.a libkernel32.a


bool write_ll_file(const char* original_file, const char* ir, size_t ir_size)
{
	bool result = true;

	if ((!ir) || (ir_size == 0))
		UNREACHABLE("IR is nullptr or size is 0");

	const char* ll_file = swap_extension(original_file, "ll");

	if (!nob_write_entire_file(ll_file, ir, ir_size)) { // LLVM-IR Output
		nob_log(NOB_ERROR, "Failed to write LLVM-IR for %s", original_file);
		return_defer(false);
	}

defer:
	return result;
}

char* strconcat(const char* s1, const char* s2)
{
	if (!s1 || !s2) {
		nob_log(NOB_ERROR, "Null input to strconcat");
		return NULL;
	}

	size_t len = strlen(s1) + strlen(s2) + 1;
	char* result = (char*)malloc(len);
	if (!result) {
		nob_log(NOB_ERROR, "Memory allocation failed in strconcat");
		return NULL;
	}

	strcpy(result, s1);
	strcat(result, s2);

	return result;
}

const char* get_extension(const char* filename)
{
	if (!filename) {
		nob_log(NOB_ERROR, "Null input to get_extension");
		return NULL;
	}

	const char* dot = strrchr(filename, '.');
	if (!dot || dot == filename)
		return filename; // No extension found or filename starts with a dot

	return dot + 1; // Return the extension without the dot
}

char* chop_extension(const char* filename)
{
	if (!filename) {
		nob_log(NOB_ERROR, "Null input to chop_extension");
		return NULL;
	}

	char* dot = (char*)strrchr(filename, '.');

	if (dot) {
		size_t len = dot - filename;
		char* result = (char*)malloc(len + 1);
		if (!result) {
			nob_log(NOB_ERROR, "Memory allocation failed in chop_extension");
			return NULL;
		}
		strncpy(result, filename, len);
		result[len] = '\0';
		return result;
	}
	return (char*)filename; // No extension found, return a copy of the original string
}

char* swap_extension(const char* filename, const char* new_extension)
{
	if (!filename || !new_extension) {
		nob_log(NOB_ERROR, "Null input to swap_extension");
		return NULL;
	}
	char* base = chop_extension(filename);
	if (!base) {
		nob_log(NOB_ERROR, "Failed to chop extension in swap_extension");
		return NULL;
	}
	size_t len = strlen(base) + strlen(new_extension) + 2; // +1 for the dot and +1 for the null-terminator
	char* result = (char*)malloc(len);
	if (!result) {
		nob_log(NOB_ERROR, "Memory allocation failed in swap_extension");
		free(base);
		return NULL;
	}
	sprintf(result, "%s.%s", base, new_extension);
	free(base);
	return result;
}

bool run_clang(const char* output_file, const char* args)
{
	bool result = true;

	static Cmd c = { 0 };

	cmd_append(&c, CLANGC, output_file, args);

	if (!cmd_run_sync_and_reset(&c))
		return_defer(false);

defer:
	c.count = 0;
	return result;
}

// TODO: maybe compile the .ll file .o using llc instead