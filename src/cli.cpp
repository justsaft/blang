
extern "C" {
#include "../3rd-party/nob.h"
}

#include <iostream>
#include <string.h>
#include <string>
#include <array>
#include <memory>
#include <cstdio>

#include "types.hpp"
#include "common.hpp"

void usage(const char* program_name)
{
    fprintf(stderr, "Usage: %s <Options | Input Files> ...\n", program_name);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -h, --help       Show this help message\n");
    fprintf(stderr, "  -v, --version    Show version information\n");
    fprintf(stderr, "  -o, --output     Specify output file\n");
    fprintf(stderr, "  -t, --target     Specify the target triple\n");
    fprintf(stderr, "  --print-target   Prints the target triple\n");
    fprintf(stderr, "  --emit-ir        Emit only the intermediate representation\n");
    fprintf(stderr, "  --keep-ir        Keep the generated intermediate representation\n");
}

void version(void)
{
    fprintf(stderr, "Version: 0.0.2\n");
    fprintf(stderr, "Build date: %s\n", __DATE__);
    fprintf(stderr, "Build time: %s\n", __TIME__);
}

void parse_cli_arguments(int argc, char** argv, std::string& target_override, std::string& output_override, CStrings& input_files, IR_Output& ir_output)
{
#if !defined(DEBUG)
    if (argc < 2) {
        nob_log(NOB_ERROR, "No arguments or input files.\n");
        usage(argv[0]);
        exit(NoArgumentsGiven);
    }
#endif

    bool print_target_flag = false;

    for (uint8_t arg = 1; arg < argc; ++arg) {
        if (strcmp(argv[arg], "-v") == 0 || strcmp(argv[arg], "--version") == 0) { // -v flag
            version();
            exit(Success);
        } else if (strcmp(argv[arg], "-h") == 0 || strcmp(argv[arg], "--help") == 0) { // -h flag
            usage(argv[0]);
            exit(Success);
        } else if (strcmp(argv[arg], "-o") == 0 || strcmp(argv[arg], "--output") == 0) { // -o flag
            static bool duplicate_o = false;
            if (duplicate_o) {
                nob_log(NOB_ERROR, "There cannot be two outputs specified.");
                exit(UnexpectedArguments);
            }
            ++arg;
            if (argc <= arg) {
                nob_log(NOB_ERROR, "No output file name provided after `--output` or `-o`.");
                exit(UnexpectedArguments);
            }
            output_override = argv[arg];
            duplicate_o = true;
        } else if (strcmp(argv[arg], "-t") == 0 || strcmp(argv[arg], "--target") == 0) { // -t flag
            static bool duplicate_t = false;
            if (duplicate_t) {
                nob_log(NOB_ERROR, "There cannot be two targets specified.");
                exit(UnexpectedArguments);
            }
            ++arg;
            if (argc <= arg) { // TODO: This check is weird
                nob_log(NOB_ERROR, "No target triple provided after `--target` or `-t`.");
                exit(UnexpectedArguments);
            }
            duplicate_t = true;
            target_override = argv[arg];
            if (strncmp(target_override.data(), "/dev/", 5) == 0) {
                ir_output = DontCompile;
            }
        } else if (strcmp(argv[arg], "--print-target") == 0) {
            print_target_flag = true;
        } else if (strcmp(argv[arg], "--emit-ir") == 0) {
            ir_output = DontCompile;
        } else if (strcmp(argv[arg], "--keep-ir") == 0) {
            ir_output = KeepIrAfterCompile;
        } else { // input files
            const char* file = argv[arg];
            if (nob_file_exists(file)) {
                input_files.push_back(file);
            } else { // Not appending to list of files to compile:
                nob_log(NOB_WARNING, "File doesn't exist: `%s`", file);
            }
        }
    }

    if (print_target_flag) {
        // This feels like a hack² but I feel it should
        // always print the target, even if it is supplied.
        fprintf(stdout, "%s\n", target_override.c_str());
        exit(Success);
    }
}


/* std::array<std::string, 4> supported_targets = {
    "x86_64-pc-linux-gnu",
    "x86_64-pc-windows-gnu",
    "aarch64-pc-linux-gnu",
    "aarch64-pc-windows-gnu",
}; */

std::string get_target_triple_clang(void)
{
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, int (*)(FILE*)> pipe(popen("clang -print-target-triple", "r"), pclose);

    if (!pipe) {
        return "unknown-unknown-unknown-unknown";
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    // Remove trailing newline if present
    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }

    return result.empty() ? "unknown-unknown-unknown-unknown" : result;
}

// TODO: Should it stop? Probably not. Should we catch a wrong target before it goes to LLVM? Actually too much effort, plus might not be worth.
/* void stop_on_unsupported_target(std::string target)
{
    uint8_t i = 0;
    for (i = supported_targets.size(); i > 0;)
    {
        if (strcmp(target.data(), supported_targets[i - 1].data()) == 0)
        {
            break;
        }
        --i;
    }
    if (i == 0)
    {
        compilation_error(UnsupportedTarget);
        exit(UnsupportedTarget);
    }
} */