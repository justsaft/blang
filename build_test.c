
#include "build.h"

#ifdef _WIN32
#define EXECNAME "test.exe"
#else
#define EXECNAME "test"
#endif

int main(int argc, char** argv)
{
    parse_cli(argc, argv);

    Nob_Procs procs = { 0 };
    Nob_Cmds cmds = { 0 };
    TUs tus = { 0 };

    add_tu(&tus, "test.cpp");
    add_tu(&tus, "output.c");
    add_tu(&tus, "nob.c");
    add_tu(&tus, "clex.c");
    add_tu(&tus, "clex_util.cpp");

    compile_all(&cmds, &tus, &procs);
    wait_barrier(&procs);
    if (!flags.compile_ok) exit(1);

    link_tus(&tus, &procs, EXECNAME);
    wait_barrier(&procs);
    if (!flags.compile_ok) exit(2);

    return 0;
}