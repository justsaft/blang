
#define NOB_IMPLEMENTATION
#if !defined(_WIN32)
#define NOB_EXPERIMENTAL_DELETE_OLD
#endif
#include "3rd-party/nob.h"


#if defined(__GNUC__)
#define LESS_WARNS "-Wno-unused-function"
#define WARNS "-Wall", "-Wextra", "-Wno-missing-field-initializers"
#define FLAGS "-c"
#define DEBUG "-ggdb", "-DDEBUG"
#define CC "gcc", FLAGS, WARNS, "-std=c11", "-o"
#define CXXC "g++", FLAGS, WARNS, "-std=c++23", "-o"

#ifdef _WIN32
#define LINK "g++", "-o", "blang.exe"
#else
#define LINK "g++", "-o", "blang"
#endif

#elif defined(__clang__)
#define LESS_WARNS "-Wno-unused-function"
#define WARNS "-Wno-deprecated-declarations", "-Wno-non-c-typedef-for-linkage", "-Wno-missing-field-initializers"
#define FLAGS "-c"
#define DEBUG "-g", "-DDEBUG"
#define CC "clang", FLAGS, WARNS, "-std=c11", "-o"
#define CXXC "clang++", FLAGS, WARNS, "-std=c++23", "-o"

#ifdef _WIN32
#define LINK "clang++", "-o", "blang.exe"
#else
#define LINK "clang++", "-o", "blang"
#endif

#elif defined(_MSC_VER)
#define WARNS "/W3"
#define DEBUG "/Zi", "/DDEBUG"
#define CC "cl.exe", "/EHsc", "/permissive-", WARNS, "/c", /*"/FS",*/ "/std:c11", "/Fo:"
#define CXXC "cl.exe", "/EHsc", "/permissive-", WARNS, "/c", /*"/FS",*/ "/std:c++20", "/Fo:"
#define LINK "link.exe", "/NOLOGO", "/OUT:blang.exe"

#endif


// Folders
#define SRC "src/"
#define BLD "build/"


#define a 8
Nob_Procs procs = { 0 };
Nob_Cmd cmds[a] = { 0 };
Nob_Cmd linkcmd = { 0 };

// B Compiler
#define BC "blang.exe", "-o", BLD"main.ll", "b_src/main.b"

int main(int argc, char** argv)
{
	NOB_GO_REBUILD_URSELF(argc, argv);

	bool compile_ok = true;
	bool debug = false;

	for (int arg = 1; arg < argc; ++arg) {
		if (strcmp("--debug", argv[arg]) == 0) {
			debug = true;
		}
	}

	if (!nob_mkdir_if_not_exists(BLD)) {
		nob_log(NOB_ERROR, "Could not create directory %s", BLD);
		exit(1);
	}

	nob_cmd_append(&cmds[0], CXXC, BLD"main.o", SRC"main.cpp");
	nob_cmd_append(&cmds[1], CXXC, BLD"gen_ir.o", SRC"gen_ir.cpp");
	nob_cmd_append(&cmds[2], CXXC, BLD"cli.o", SRC"cli.cpp");
	nob_cmd_append(&cmds[3], CXXC, BLD"clex_util.o", SRC"clex_util.cpp");
	nob_cmd_append(&cmds[4], CC, BLD"output.o", SRC"output.c");
	nob_cmd_append(&cmds[5], CC, BLD"nob.o", SRC"nob.c");
	nob_cmd_append(&cmds[6], CC, BLD"clex.o", LESS_WARNS, SRC"clex.c");
	nob_cmd_append(&cmds[7], CXXC, BLD"backend.o", SRC"backend.cpp");

	for (int i = 0; i < a; ++i) {
		if (debug)
			nob_cmd_append(&cmds[i], DEBUG);

		if (!nob_cmd_run(&cmds[i], .async = &procs))
			compile_ok = false;
	}

	nob_cmd_append(&linkcmd, LINK, BLD"main.o", BLD"backend.o", BLD"cli.o", BLD"clex.o", BLD"clex_util.o", BLD"nob.o", BLD"output.o", BLD"gen_ir.o");

	for (int i = 0; i < a; ++i) {
		if (!nob_procs_wait_and_reset(&procs)) {
			compile_ok = false;
		}
	}

	if (!compile_ok) {
		nob_log(NOB_ERROR, "Did not link");
		return -1;
	}

	nob_cmd_run_sync_and_reset(&linkcmd);
	return 0;
}