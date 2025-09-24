
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


static void usage(const char* program_name)
{
	const char* substr_program_name = strrchr(program_name, '/') + 1;

	fprintf(stderr, "Usage: %s <Options | Input Files> ...\n",
			substr_program_name == NULL ? program_name : substr_program_name);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "  -h, --help           Show this help message\n");
	fprintf(stderr, "  -v, --version        Show version information\n");
	fprintf(stderr, "  -o, --output         Specify output file\n");
	fprintf(stderr, "  -t, --target         Specify the target triple\n");
	fprintf(stderr, "  --print-target       Prints the target triple and exits.\n");
	fprintf(stderr, "\nCompilation:\n");
	fprintf(stderr, "  -c, --compile   		Only compile (do not produce an excutable)\n");
	fprintf(stderr, "  --emit-ir            Emit only the IR\n");
	fprintf(stderr, "  --keep-ir            Keep the generated IR (default)\n");
	fprintf(stderr, "\nLanguage:\n");
	fprintf(stderr, "  --bext               Use the same compile standard as B-Ext\n");
	fprintf(stderr, "                       (implies --modern --largeint)\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "  --historical         Use the original 16-Bit compile standard as B (default)\n");
	fprintf(stderr, "  --modern             Enable modern language features\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "  --largeint           Use 64-Bit integers (word size)\n");
}

static void version(void)
{
	fprintf(stderr, "Version: 0.0.4\n");
	fprintf(stderr, "Build date: %s\n", __DATE__);
	fprintf(stderr, "Build time: %s\n", __TIME__);
#if defined(DEBUG)
	fprintf(stderr, "THIS BUILD WAS BUILT WITH -DDEBUG\n");
	fprintf(stderr, "            Debug build.\n");
#endif
}

void parse_cli_arguments(int argc, char** argv, std::string& target_override, std::string& output_override, B_Files& input_files, Compilation& comp)
{
	bool print_target_flag = false;

	for (uint8_t arg = 1; arg < argc; ++arg) {
		if ((strcmp(argv[arg], "-v") == 0) ||
			(strcmp(argv[arg], "--version") == 0)) { // -v flag

			version();
			exit(Success);

		} else if ((strcmp(argv[arg], "-h") == 0) ||
				   (strcmp(argv[arg], "--help") == 0)) { // -h flag

			usage(argv[0]);
			exit(Success);

		} else if ((strcmp(argv[arg], "-o") == 0) ||
				   (strcmp(argv[arg], "--output") == 0)) { // -o flag

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

		} else if ((strcmp(argv[arg], "-t") == 0) ||
				   (strcmp(argv[arg], "--target") == 0)) { // -t flag

			static bool duplicate_t = false;
			if (duplicate_t) {
				nob_log(NOB_ERROR, "There cannot be two targets specified.");
				Compilation_error(UnexpectedArguments);
			}
			++arg;
			if (argc <= arg) { // TODO: This check is weird
				nob_log(NOB_ERROR, "No target triple provided after `--target` or `-t`.");
				Compilation_error(UnexpectedArguments);
			}
			duplicate_t = true;
			target_override = argv[arg];
			if (strncmp(target_override.data(), "/dev/", 5) == 0) {
				comp.SetIROutput(DontCompile); // TODO: Why is this here again?
			}

		} else if ((strcmp(argv[arg], "-c") == 0) ||
				   (strcmp(argv[arg], "--only-compile") == 0)) {

			comp.wants_executable = false;

		} else if (strcmp(argv[arg], "--print-target") == 0) {
			print_target_flag = true;

		} else if (strcmp(argv[arg], "--emit-ir") == 0) {
			comp.SetIROutput(DontCompile);

		} else if (strcmp(argv[arg], "--keep-ir") == 0) {
			comp.SetIROutput(KeepIrAfterCompile);

		} else if (strcmp(argv[arg], "--bext") == 0) {
			comp.SetLangMode(Modernized);
			comp.SetWordSize(SixtyfourBit);

		} else if (strcmp(argv[arg], "--modern") == 0) {
			comp.SetLangMode(Modernized);

		} else if (strcmp(argv[arg], "--historical") == 0) {
			comp.SetLangMode(Historical);

		} else if (strcmp(argv[arg], "--largeint") == 0) {
			comp.SetWordSize(SixtyfourBit);

		} else { // input files
			const char* path = argv[arg];
			if (nob_file_exists(path)) {
				input_files.push_back(B_File { path });
			} else { // Not appending to list of files to compile:
				nob_log(NOB_WARNING, "File doesn't exist: `%s`", path);
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