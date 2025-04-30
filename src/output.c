
#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NOB_STRIP_PREFIX
#include "../3rd-party/nob.h"

#include "output.h"

#define CLANGC "clang", "-o"
#define LINKER "clang", "-o"

// TODO: (no) linking with libc
//       -nostdlib: Disables linking with both libc and the standard startup files.
//       -nodefaultlibs : Disables linking with libc but still includes the startup files.
//       -llibstd++
//       ld -s hello.o crt2.o -o hello.exe libstdc++.a libgcc.a libmingw32.a libmingwex.a libmsvcrt.a libkernel32.a

/* bool compile_ll_file(const char* original_file, const char* output_file)
{
	bool result = true;

defer:
	return result;
} */

/* const char* get_platform_triple(void)
{
#ifdef _WIN32
	return get_target_triple(0);
#else
	return get_target_triple(3);
#endif
} */

bool write_ll_file(const char* original_file, const char* ir, size_t ir_size)
{
	bool result = true;

	if (!ir || (ir_size == 0)) {
		nob_log(NOB_ERROR, "IR is empty");
		return_defer(false);
	}

	const char* ll_file = swap_extension(original_file, "ll");
	/* nob_log(NOB_INFO, "LL file name: %s", ll_file); */

	if (!nob_write_entire_file(ll_file, ir, ir_size)) { // LLVM-IR Output
		nob_log(NOB_ERROR, "Failed to write LLVM-IR for file %s", original_file);
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

	int len = strlen(s1) + strlen(s2) + 1; // +1 for the null-terminator
	char* result = malloc(len);
	if (!result) {
		nob_log(NOB_ERROR, "Memory allocation failed in strconcat");
		return NULL;
	}

	strcpy(result, s1);
	strcat(result, s2);

	// Do not free s1 and s2 here; the caller is responsible for managing their memory.
	return result;
}

/* const char* get_extension(const char* filename)
{
	if (!filename) return NULL;

	const char* dot = strrchr(filename, '.');
	if (!dot || dot == filename) return ""; // No extension found or filename starts with a dot
	return dot + 1; // Return the extension without the dot
} */

char* chop_extension(const char* filename)
{
	if (!filename) {
		nob_log(NOB_ERROR, "Null input to chop_extension");
		return NULL;
	}

	char* dot = strrchr(filename, '.');

	if (dot) {
		size_t len = dot - filename;
		char* result = malloc(len + 1);
		if (!result) {
			nob_log(NOB_ERROR, "Memory allocation failed in chop_extension");
			return NULL;
		}
		strncpy(result, filename, len);
		result[len] = '\0';
		return result;
	}
	return strdup(filename); // No extension found, return a copy of the original string
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
	char* result = malloc(len);
	if (!result) {
		nob_log(NOB_ERROR, "Memory allocation failed in swap_extension");
		free(base);
		return NULL;
	}
	sprintf(result, "%s.%s", base, new_extension);
	free(base);
	return result;
}

/* void replace_char(char* str, char old_char, char new_char)
{
   if (!str) {
       nob_log(NOB_ERROR, "Null input to replace_char");
       return;
   }

   while (*str) {
       if (*str == old_char) {
           *str = new_char;
       }
       str++;
   }
} */

bool run_clang(const char* output_file, const char* original_file, int stage)
{
	bool result = true;
	static Nob_Cmd c = { 0 };

	switch (stage) {
	case COMPILE:
		nob_cmd_append(&c, CLANGC, output_file, original_file); // treat original_file as a string containing the file name to compile
		break;

	case LINK: // Handles linking input files together
		nob_cmd_append(&c, LINKER, output_file, original_file); // treat original_file as a string containing all the file names to link together
		break;

	default:
		nob_log(NOB_ERROR, "Invalid stage for run_clang");
		exit(1);
	}

	// After we created the command:...
	if (!cmd_run_sync_and_reset(&c)) {
		nob_log(NOB_ERROR, "Couldn't run command: %s", *c.items);
		return_defer(false);
	}

defer:
	c.count = 0; // Reset the command for the next run
	return result;
}

/* nob_log(NOB_INFO, "Compiling %s to object file", file);
nob_cmd_append(&c, "llc", "-filetype=obj", "-o", file, ll_file); */
// TODO: maybe compile the .ll file .o using llc instead

