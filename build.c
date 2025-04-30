
#define NOB_STRIP_PREFIX
#define NOB_IMPLEMENTATION
#include "3rd-party/nob.h"

//#define MSVCC "cl.exe", "/EHsc", "/c", "/W3", "/Zi", /*"/FS",*/ "/Fo:"
//#define MSVCL "link.exe", "/NOLOGO", "/OUT:"OUTPUT
#define SRC "src/"
#define BLD "build/"
#define FLAGS "-c", "-Wall", "-Wextra", "-ggdb"
#define GCCC "gcc", FLAGS, "-o"
#define GPPC "g++", FLAGS, "-o"
#define GCCL "g++", "-o", "blang"
#define BC "blang.exe", "-o", BLD"main.ll"
#define CLANGC "clang", "-c", "-o"

int main(int argc, char** argv)
{
	const bool run_blang = false;
	bool compile_ok = true;

	NOB_GO_REBUILD_URSELF(argc, argv);

	if (!mkdir_if_not_exists(BLD)) {
		nob_log(NOB_ERROR, "Could not create directory %s", BLD);
		exit(1);
	}

	Cmd c = { 0 };
	cmd_append(&c, GPPC, BLD"main.o", SRC"main.cpp");
	if (!nob_cmd_run_sync_and_reset(&c)) {
		nob_log(NOB_ERROR, "Could not compile %s", *c.items);
		compile_ok = false;
	}

	cmd_append(&c, GCCC, BLD"clex.o", SRC"clex.c");
	if (!nob_cmd_run_sync_and_reset(&c)) {
		nob_log(NOB_ERROR, "Could not compile %s", *c.items);
		compile_ok = false;
	}

	cmd_append(&c, GCCC, BLD"nob.o", SRC"nob.c");
	if (!nob_cmd_run_sync_and_reset(&c)) {
		nob_log(NOB_ERROR, "Could not compile %s", *c.items);
		compile_ok = false;
	}

	cmd_append(&c, GCCC, BLD"output.o", SRC"output.c");
	if (!nob_cmd_run_sync_and_reset(&c)) {
		nob_log(NOB_ERROR, "Could not compile %s", *c.items);
		compile_ok = false;
	}

	cmd_append(&c, GPPC, BLD"ir_gen.o", SRC"gen_ir.cpp");
	if (!nob_cmd_run_sync_and_reset(&c)) {
		nob_log(NOB_ERROR, "Could not compile %s", *c.items);
		compile_ok = false;
	}

	cmd_append(&c, GPPC, BLD"state.o", SRC"state.cpp");
	if (!nob_cmd_run_sync_and_reset(&c)) {
		nob_log(NOB_ERROR, "Could not compile %s", *c.items);
		compile_ok = false;
	}

	if (compile_ok) {
		cmd_append(&c, GCCL, BLD"main.o", BLD"clex.o", BLD"nob.o", BLD"output.o", BLD"ir_gen.o", BLD"state.o");
		nob_cmd_run_sync_and_reset(&c);

		if (run_blang) {
			nob_log(NOB_INFO, "Running: ---------------\n");
			cmd_append(&c, BC, SRC"main.b");
			nob_cmd_run_sync_and_reset(&c);
		}
	}
	else
	{
		nob_log(NOB_ERROR, "Did not link");
		return 1;
	}
	return 0;
}