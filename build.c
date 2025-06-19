
#define NOB_IMPLEMENTATION
#define NOB_EXPERIMENTAL_DELETE_OLD
#include "3rd-party/nob.h"

#define FLAGS "-c", "-Wall", "-Wextra", "-ggdb"
#define CLANGPPC "clang++", "-c", "-o"

#if _WIN32
// Windows:
#if defined(__GNUC__)
#define GCCC "gcc", FLAGS, "-o"
#define GPPC "g++", FLAGS, "-o"
#define GCCL "g++", "-o", "blang"
#elif defined(__clang__)
#define GCCC "clang", "-c", "-o"
#define GPPC CLANGPPC
// TODO: Clang currently doesn't get any flags. Might be nessessary for cross-compilation or debugging in the future
#elif defined(_MSC_VER)
#define GCCC "cl.exe", "/EHsc", "/c", "/W3", "/Zi", /*"/FS",*/ "/Fo:"
#define GPPC "cl.exe", "/EHsc", "/c", "/W3", "/Zi", /*"/FS",*/ "/Fo:"
#define GCCL "link.exe", "/NOLOGO", "/OUT:blang.exe"
#endif

#else
// Linux:
#define GCCC "gcc", FLAGS, "-o"
#define GPPC "g++", FLAGS, "-o"
#define GCCL "g++", "-o", "blang"

#endif


// Folders
#define SRC "src/"
#define BLD "build/"

// B Compiler
#define BC "blang.exe", "-o", BLD"main.ll"

int main(int argc, char** argv)
{
	bool compile_ok = true;

	NOB_GO_REBUILD_URSELF(argc, argv);

	if (!nob_mkdir_if_not_exists(BLD))
	{
		nob_log(NOB_ERROR, "Could not create directory %s", BLD);
		exit(1);
	}

#define a 8

	Nob_Cmd cmds[a] = { 0 };
	nob_cmd_append(&cmds[0], GPPC, 		BLD"main.o", 							SRC"main.cpp");
	nob_cmd_append(&cmds[1], GPPC, 		BLD"gen_ir.o", 							SRC"gen_ir.cpp");
	nob_cmd_append(&cmds[2], GPPC, 		BLD"state.o", 							SRC"state.cpp");
	nob_cmd_append(&cmds[3], GPPC, 		BLD"cli.o", 							SRC"cli.cpp");
	nob_cmd_append(&cmds[4], GPPC, 		BLD"clex_util.o", 						SRC"clex_util.cpp");
	nob_cmd_append(&cmds[5], GCCC, 		BLD"nob.o", 							SRC"nob.c");
	nob_cmd_append(&cmds[6], GCCC, 		BLD"clex.o", "-Wno-unused-function",	SRC"clex.c");
	nob_cmd_append(&cmds[7], GCCC, 		BLD"output.o", 							SRC"output.c");

	pid_t pids[a] = { 0 };
	size_t results[a] = { 0 };

	for (int i = 0; i < a; ++i)
	{
		pids[i] = nob_cmd_run_async(cmds[i]);
	}

	for (int i = 0; i < a; ++i)
	{
		int status = 0;
		if (pids[i] > 0)
		{
			waitpid(pids[i], &status, 0);
			if (status != 0)
			{
				compile_ok = false;
			}
		}
		else
		{
			nob_log(NOB_ERROR, "Failed to start process for %s", *cmds[i].items);
			compile_ok = false;
		}
	}

	if (compile_ok)
	{
		Nob_Cmd linkcmd;
		nob_cmd_append(&linkcmd, GCCL, BLD"main.o", BLD"cli.o", BLD"clex.o", BLD"clex_util.o", BLD"nob.o", BLD"output.o", BLD"gen_ir.o", BLD"state.o");
		nob_cmd_run_sync_and_reset(&linkcmd);
	}
	else
	{
		nob_log(NOB_ERROR, "Did not link");
		return 1;
	}
	return 0;
}