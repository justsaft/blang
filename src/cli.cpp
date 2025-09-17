
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


std::array<std::string, 4> known_targets = {
	"x86_64-pc-linux-gnu",
	"x86_64-pc-windows-gnu",
	"aarch64-pc-linux-gnu",
	"aarch64-pc-windows-gnu",
};


static void usage(const char* program_name)
{
	const char* substr_program_name = strrchr(program_name, '/') + 1;

	fprintf(stderr, "Usage: %s <Options | Input Files> ...\n", substr_program_name == NULL ? program_name : substr_program_name);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "  -h, --help           Show this help message\n");
	fprintf(stderr, "  -v, --version        Show version information\n");
	fprintf(stderr, "  -o, --output         Specify output file\n");
	fprintf(stderr, "  -t, --target         Specify the target triple\n");
	fprintf(stderr, "  -c, --only-compile   Specify the target triple\n");
	fprintf(stderr, "  --print-target       Prints the target triple and exits.\n");
	fprintf(stderr, "  --emit-ir            Emit only the intermediate representation\n");
	fprintf(stderr, "  --keep-ir            Keep the generated intermediate representation\n");
	fprintf(stderr, "  --bext               TODO\n");
	fprintf(stderr, "  --historical         TODO\n");
	fprintf(stderr, "  --modern             Enable modern language features\n");
	fprintf(stderr, "  --largeint           TODO\n");
}

static void version(void)
{
	fprintf(stderr, "Version: 0.0.3\n");
	fprintf(stderr, "Build date: %s\n", __DATE__);
	fprintf(stderr, "Build time: %s\n", __TIME__);
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

bool is_clang_installed(void)
{
#if defined(_WIN32)
	FILE* pipe = _popen("clang --version 2>nul", "r");
	if (!pipe) {
		return false;
	}
	char buffer[128];
	bool found = false;
	if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
		found = true; // If we get any output, clang exists
	}
	_pclose(pipe);
	return found;
#else
	FILE* pipe = popen("clang --version 1>/dev/null 2>/dev/null", "r");
	if (!pipe) {
		return false;
	}
	int status = pclose(pipe);
	return status == 0;
#endif
}

std::string get_target_triple_clang(void)
{
#ifdef _WIN32

	HANDLE hReadPipe, hWritePipe;
	SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };

	if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
		nob_log(NOB_ERROR, "Failed to create pipe.");
		Compilation_error(ErrorWriteOutput);
		exit(ErrorWriteOutput);
	}

	STARTUPINFOA si = { };
	si.cb = sizeof(STARTUPINFOA);
	si.hStdOutput = hWritePipe;
	si.hStdError = hWritePipe;
	si.dwFlags |= STARTF_USESTDHANDLES;

	PROCESS_INFORMATION pi = { };
	const char* cmd = "clang -print-target-triple";

	if (!CreateProcessA(NULL, const_cast<char*>(cmd), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
		nob_log(NOB_INFO, "You may not have `clang` installed.");
		CloseHandle(hReadPipe);
		CloseHandle(hWritePipe);
		Compilation_error(ErrorWriteOutput);
		exit(ErrorWriteOutput);
	}

	CloseHandle(hWritePipe); // Close write end so we can read

	std::string result;
	char buffer[128] = { 0 };
	DWORD bytesRead;

	while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
		buffer[bytesRead] = '\0'; //unsafe
		result += buffer;
	}

	CloseHandle(hReadPipe);
	WaitForSingleObject(pi.hProcess, INFINITE);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	// Remove trailing newline
	if (!result.empty() && result.back() == '\n') {
		result.pop_back();
	}

	if (result.find("unknown") != std::string::npos) {
		nob_log(NOB_ERROR, "Target %s is not a valid target", result.c_str());
		Compilation_error(UnsupportedTarget);
		exit(UnsupportedTarget);
	}

#else

	std::array<char, 128> buffer;
	std::string result;
	std::unique_ptr<FILE, int (*)(FILE*)> pipe(popen("clang -print-target-triple", "r"), pclose);

	if (!pipe) {
		nob_log(NOB_INFO, "You may not have `clang` installed.");
		Compilation_error(ErrorWriteOutput);
		exit(ErrorWriteOutput);
	}

	while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
		result += buffer.data();
	}

	// Remove trailing newline if present
	if ((!result.empty()) && (result.back() == '\n')) {
		result.pop_back();
	}

	if (strstr(result.data(), "unknown") != NULL) {
		nob_log(NOB_ERROR, "Target %s is not a valid target", result.c_str());
		Compilation_error(UnsupportedTarget);
		exit(UnsupportedTarget);
	}

#endif

	return result;
}

// TODO: Should it stop? Probably not. Should we catch a wrong target before it goes to LLVM? Actually too much effort, plus might not be worth.
/* void stop_on_unsupported_target(std::string& target)
{
	// if (target.empty())
	//     nob_log(NOB_INFO, "You may not have `clang` installed", target.c_str());

	if (strstr(target.data(), "unknown") != NULL) {
		nob_log(NOB_ERROR, "Target %s is not a valid target", target.c_str());
		Compilation_error(UnsupportedTarget);
		exit(UnsupportedTarget);
	}
} */