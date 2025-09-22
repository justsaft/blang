
#include <memory>
#include <stdexcept>

#include "backend.hpp"
#include "common.hpp"

extern "C" {
#include "../3rd-party/nob.h"
}

// std::array<std::string, 4> known_targets = {
// 	"x86_64-pc-linux-gnu",
// 	"x86_64-pc-windows-gnu",
// 	"aarch64-pc-linux-gnu",
// 	"aarch64-pc-windows-gnu",
// };

extern Compilation c;
std::string get_target_triple_clang(void);
std::string get_target_triple_llc(void);

const char* backend2str(Backend b)
{
    switch (b) {
        case Backend_CLANG: return "Clang";
        case Backend_LLC: return "LLC";
        default: NOB_UNREACHABLE("Unknown backend");
    }
}

bool is_backend_installed(Backend b)
{
#if defined(_WIN32)
    FILE* pipe;

    switch (b) {
        case Backend_CLANG: pipe = _popen("clang --version 2>nul", "r");
        case Backend_LLC: pipe = _popen("llc --version 2>nul", "r");
        default: NOB_UNREACHABLE("Unknown backend");
    }

    if (!pipe) {
        return false;
    }
    char buffer[128];
    memset(buffer, 0, sizeof(buffer));
    bool found = false;
    if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        found = true; // If we get any output, clang exists
    }
    _pclose(pipe);
    return found;
#else
    FILE* pipe;

    switch (b) {
        case Backend_CLANG: pipe = popen("clang --version 1>/dev/null 2>/dev/null", "r"); break;
        case Backend_LLC: pipe = popen("llc --version 1>/dev/null 2>/dev/null", "r"); break;
        default: NOB_UNREACHABLE("Unknown backend");
    }

    if (!pipe) {
        return false;
    }
    int status = pclose(pipe);
    return status == 0;
#endif
}

bool is_backend_installed(void)
{
    return is_backend_installed(c.GetBackend());
}

std::string get_target_triple(const Backend b)
{
    switch (b) {
        case Backend_CLANG: return get_target_triple_clang(); break;
        case Backend_LLC: return get_target_triple_llc(); break;
        default: NOB_UNREACHABLE("Unknown backend");
    }
}

std::string get_target_triple(void)
{
    return get_target_triple(c.GetBackend());
}

// bool is_any_backend_installed(void)
// {
//     for (int i = 0; i < TotalAmountOfBackends; ++i)
//         return is_backend_installed((Backend)i);
// }

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
    char buffer[128] = { };
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

    std::array<char, 128> buffer { };
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

std::string get_target_triple_llc(void)
{
    std::array<char, 128> buffer { };
    std::string result;
    std::string cmd = "llc --version 2>&1";
    std::unique_ptr<FILE, int (*)(FILE*)> pipe(popen("clang -print-target-triple", "r"), pclose);

    if (!pipe) {
        Compilation_error(ErrorWriteOutput);
        exit(ErrorWriteOutput);
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    // Find the "Default target:" line.
    const std::string key = "Default target:";
    size_t pos = result.find(key);
    if (pos == std::string::npos) throw std::runtime_error("Default target line not found in llc output");

    // Extract the rest of the line after the key and trim whitespace/newline.
    pos += key.size();
    size_t end = result.find_first_of("\r\n", pos);
    std::string triple = (end == std::string::npos) ? result.substr(pos) : result.substr(pos, end - pos);

    // Trim leading/trailing whitespace.
    auto ltrim = [ ] (std::string& s)
    {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [ ] (unsigned char ch)
        {
            return !std::isspace(ch);
        }));
    };
    auto rtrim = [ ] (std::string& s)
    {
        s.erase(std::find_if(s.rbegin(), s.rend(), [ ] (unsigned char ch)
        {
            return !std::isspace(ch);
        }).base(), s.end());
    };
    ltrim(triple);
    rtrim(triple);
    return triple;
}


// TODO: (no) linking with libc
//       -nostdlib: Disables linking with both libc and the standard startup files.
//       -nodefaultlibs : Disables linking with libc but still includes the startup files.
//       -llibstd++
//       ld -s hello.o crt2.o -o hello.exe libstdc++.a libgcc.a libmingw32.a libmingwex.a libmsvcrt.a libkernel32.a