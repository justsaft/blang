
#include "build.h"

#ifdef _WIN32
#define BLANG "blang.exe"
#else
#define BLANG "blang"
#endif

int main(int argc, char** argv)
{
	NOB_GO_REBUILD_URSELF(argc, argv);
	parse_cli(argc, argv);

	if (!nob_mkdir_if_not_exists(BLD) || !(flags.debug ? nob_mkdir_if_not_exists(BLD DEBUG_O_FOLDER) : nob_mkdir_if_not_exists(BLD RELEASE_O_FOLDER))) return 5;

	Nob_Cmds cmds = { 0 };
	Nob_Procs procs = { 0 };
	TUs tus = { 0 };

	if (flags.testprog)
		run_sub_recipe_async(argc, argv, "build_test.c", &procs);

	add_tu(&tus, "backend.cpp");
	add_tu(&tus, "clex_util.cpp");
	add_tu(&tus, "clex.c");
	add_tu(&tus, "cli.cpp");
	add_tu(&tus, "gen_ir.cpp");
	add_tu(&tus, "main.cpp");
	add_tu(&tus, "nob.c");
	add_tu(&tus, "output.c");

	compile_all(&cmds, &tus, &procs);
	wait_barrier(&procs);

	if (!flags.compile_ok) {
		nob_log(NOB_WARNING, "Skipped linking step due to compilation errors.");
		return 1;
	}

	link_tus(&tus, &procs, BLANG);
	wait_barrier(&procs);
	return 0;
}