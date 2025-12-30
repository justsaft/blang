#include <stdint.h>
#include <stdbool.h>

#define SRC "src/"
#define BLD "build/"
#define DEBUG_O_FOLDER "debug/"
#define RELEASE_O_FOLDER "rel/"

#if (__GNUC__ >= 0) && (__APPLE__ != 1)
#define LESS_WARNS "-Wno-unused-function"
#define WARNS "-Wall", "-Wextra", "-Wno-missing-field-initializers"
#define DEBUG "-ggdb", "-DDEBUG"
#define OPT_DEBUG "-O0" /* -Og behaves differently since an update for some reason */
#define OPT_RELEASE "-O2"
#define CC "gcc", "-c", WARNS, "-std=c11", "-o"
#define CC_ "gcc", WARNS, "-std=c11", "-o"
#define CXXC "g++", "-c", WARNS, "-std=c++23", "-o"
#define CXXC_ "g++", WARNS, "-std=c++23", "-o"

#define LINK "g++", "-o"

#elif (__clang__ == 1) || (__APPLE__ == 1)
#define LESS_WARNS "-Wno-unused-function", "-Wno-self-assign"
#define WARNS "-Wno-deprecated-declarations", "-Wno-non-c-typedef-for-linkage", "-Wno-missing-field-initializers"
#define OPT_DEBUG "-O0"
#define OPT_RELEASE "-O2"
#define DEBUG "-glldb", "-DDEBUG"
#define CC "clang", "-c", WARNS, "-std=c11", "-o"
#define CC_ "clang", WARNS, "-std=c11", "-o"
#define CXXC "clang++", "-c", WARNS, "-std=c++23", "-o"
#define CXXC_ "clang++", WARNS, "-std=c++23", "-o"

#define LINK "clang++", "-o"

#elif defined(_MSC_VER)
#define WARNS "/W3"
#define DEBUG "/Zi", "/DDEBUG"
#define CC "cl.exe", "/EHsc", "/permissive-", WARNS, "/c", /*"/FS",*/ "/std:c11", "/Fo:"
#define CC_ "cl.exe", "/EHsc", "/permissive-", WARNS, "/std:c11", "/Fo:"
#define CXXC "cl.exe", "/EHsc", "/permissive-", WARNS, "/c", /*"/FS",*/ "/std:c++20", "/Fo:"
#define CXXC_ "cl.exe", "/EHsc", "/permissive-", WARNS, "/std:c++20", "/Fo:"
#define LINK "link.exe", "/NOLOGO", "/OUT:"

#endif

#define NOB_IMPLEMENTATION
#if !defined(_WIN32)
#define NOB_EXPERIMENTAL_DELETE_OLD
#endif
#include "3rd-party/nob.h"
// #define NOB_REBUILD_URSELF(binary_path, source_path) CC_, binary_path, source_path
// #undef NOB_GO_REBUILD_URSELF
// #define NOB_GO_REBUILD_URSELF(argc, argv) nob__go_rebuild_urself(argc, argv, __FILE__, "build.h", NULL)

typedef struct {
    Nob_Cmd* items;
    size_t count;
    size_t capacity;
} Nob_Cmds;

typedef struct {
    void (*cmd_extra)(Nob_Cmd*);
    char* src_file;
    char* o_file;
    bool rebuild;
} TU;

typedef struct {
    TU* items;
    size_t count;
    size_t capacity;
} TUs;

struct {
    bool debug;
    bool force_rebuild;

    // Special stuff
    bool testprog;
    bool compile_ok;
} flags = {
        .compile_ok = true,
};

void parse_cli(int argc, char** argv)
{
    for (int arg = 1; arg < argc; ++arg) {
        if (strcmp("--debug", argv[arg]) == 0) {
            flags.debug = true;
        } else if (strcmp("--test", argv[arg]) == 0) {
            flags.testprog = true;
        } else if (strcmp("-B", argv[arg]) == 0) {
            flags.force_rebuild = true;
        }
    }
}

char* strconcat(const char* s1, const char* s2)
{
    if (!s1 || !s2) NOB_UNREACHABLE("Null input to strconcat");

    size_t len = strlen(s1) + strlen(s2) + 1;
    char* result = (char*)malloc(len);

    if (!result) NOB_UNREACHABLE("Memory allocation failed in strconcat");

    strcpy(result, s1);
    strcat(result, s2);

    return result;
}

char* chop_extension(const char* filename)
{
    if (!filename) NOB_UNREACHABLE("Null input to cmp_extension");

    char* dot = strrchr(filename, '.');

    if (dot) {
        size_t len = dot - filename;
        char* result = malloc(len + 1);

        if (!result) NOB_UNREACHABLE("Memory allocation failed in chop_extension");

        strncpy(result, filename, len);
        result[len] = '\0';
        return result;
    } else return (char*)filename;
}

char* swap_extension(const char* filename, const char* new_extension)
{
    if (!filename || !new_extension) NOB_UNREACHABLE("Null input to cmp_extension");

    char* base = chop_extension(filename);

    if (!base) NOB_UNREACHABLE("Failed to chop extension in swap_extension");

    size_t len = strlen(base) + strlen(new_extension) + 2;
    char* result = (char*)malloc(len);

    if (!result) NOB_UNREACHABLE("Memory allocation failed in swap_extension");

    sprintf(result, "%s.%s", base, new_extension);
    free(base);
    return result;
}

void add_tu(TUs* tus, const char* filename, void (*cmd_extra)(Nob_Cmd*))
{
    TU tu = { .cmd_extra = cmd_extra != NULL ? cmd_extra : NULL };

    tu.src_file = strconcat(SRC, filename);
    tu.o_file = strconcat(BLD, flags.debug ? DEBUG_O_FOLDER : RELEASE_O_FOLDER);
    tu.o_file = strconcat(tu.o_file, swap_extension(filename, "o"));
    tu.rebuild = flags.force_rebuild || nob_needs_rebuild1(tu.o_file, tu.src_file);
    nob_da_append(tus, tu);
}

bool cmp_extension(const char* filename, const char* extension)
{
    if (!filename || !extension) NOB_UNREACHABLE("Null input to cmp_extension");

    int extlen = strlen(extension);
    int srclen = strlen(filename);

    if (srclen <= extlen) NOB_UNREACHABLE("Filename too short to have a valid extension");

    const char* ext_start = &filename[srclen - extlen];
    return strcmp(ext_start, extension) == 0;
}

void compile_all(Nob_Cmds* cmds, TUs* tus, Nob_Procs* procs)
{
    for (size_t i = 0; i < tus->count; ++i) {
        TU* tu = &tus->items[i];
        if (!tu->rebuild) continue;

        nob_da_append(cmds, (Nob_Cmd)
        {
            0
        });

        Nob_Cmd* cmd = &cmds->items[i];

        if (cmp_extension(tu->src_file, ".cpp"))
            nob_cmd_append(cmd, CXXC);

        else if (cmp_extension(tu->src_file, ".c"))
            nob_cmd_append(cmd, CC);

        else NOB_UNREACHABLE("Don't know what Compiler to append");

        nob_cmd_append(cmd, tu->o_file);
        if ((tu->cmd_extra) != NULL) tu->cmd_extra(cmd);
        nob_cmd_append(cmd, tu->src_file);

        if (flags.debug) nob_cmd_append(cmd, DEBUG, OPT_DEBUG);
        else nob_cmd_append(cmd, OPT_RELEASE);

        if (!nob_cmd_run(cmd, .async = procs, .max_procs = 6))
            flags.compile_ok = false;
    }
}

void link_tus(TUs* tus, Nob_Procs* procs, const char* execname)
{
    Nob_Cmd linkcmd = { 0 };

    nob_cmd_append(&linkcmd, LINK, execname);

    bool skip = true;
    nob_da_foreach(TU, it, tus)
    {
        nob_cmd_append(&linkcmd, it->o_file);
        if (it->rebuild) skip = false;
    }

    if (!skip) flags.compile_ok = nob_cmd_run(&linkcmd, .async = procs, .max_procs = 3);
    else nob_log(NOB_INFO, "Nothing to do. Use `-B` to force a rebuild.");
    nob_cmd_free(linkcmd);
}

void wait_barrier(Nob_Procs* procs)
{
    if (!nob_procs_wait_and_reset(procs)) flags.compile_ok = false;
}

void run_sub_recipe(int argc, char** argv, const char* recipe)
{
#ifdef _WIN32
    const char* output = swap_extension(recipe, ".exe");
#else
    const char* output = chop_extension(recipe);
#endif

    Nob_Cmd cmd = { 0 };

    nob_cc(&cmd);
    nob_cc_output(&cmd, output);
    nob_cc_inputs(&cmd, recipe);
    if (!nob_cmd_run_sync_and_reset(&cmd)) exit(69);

    nob_cmd_append(&cmd, strconcat("./", output));

    for (int i = 1; i < argc; ++i)
        nob_cmd_append(&cmd, argv[i]);

    if (!nob_cmd_run_sync_and_reset(&cmd)) exit(69);

    nob_cmd_free(cmd);
}

void run_sub_recipe_async(int argc, char** argv, const char* recipe, Nob_Procs* procs)
{
#ifdef _WIN32
    const char* output = swap_extension(recipe, ".exe");
#else
    const char* output = chop_extension(recipe);
#endif

    Nob_Cmd cmd = { 0 };

    nob_cc(&cmd);
    nob_cc_output(&cmd, output);
    nob_cc_inputs(&cmd, recipe);
    nob_cmd_run_sync_and_reset(&cmd);

    nob_cmd_append(&cmd, strconcat("./", output));

    for (int i = 1; i < argc; ++i)
        nob_cmd_append(&cmd, argv[i]);

    if (!nob_cmd_run(&cmd, .async = procs, .max_procs = 3)) exit(69);

    nob_cmd_free(cmd);
}