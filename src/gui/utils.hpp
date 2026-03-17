#pragma once
#include <filesystem>
#include <string>

#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__)
#include <limits.h>
#include <unistd.h>
#elif defined(__APPLE__)
#include <limits.h>
#include <mach-o/dyld.h>
#endif

namespace ox_sim {
namespace utils {
inline std::filesystem::path GetExecutableDir() {
#ifdef _WIN32
    char buffer[MAX_PATH];
    GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    return std::filesystem::path(buffer).parent_path();

#elif defined(__linux__)
    char buffer[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    buffer[len] = '\0';
    return std::filesystem::path(buffer).parent_path();

#elif defined(__APPLE__)
    char buffer[PATH_MAX];
    uint32_t size = sizeof(buffer);
    _NSGetExecutablePath(buffer, &size);
    return std::filesystem::path(buffer).parent_path();
#endif

    return std::filesystem::path();
}

inline std::filesystem::path GetRuntimeJsonPath() { return GetExecutableDir() / "ox_runtime.json"; }

static void SetAsOpenXRRuntime(std::string& status_message) {
    auto runtime_json = GetRuntimeJsonPath();

#ifdef _WIN32
    std::string message =
        "To register as the active OpenXR runtime, administrator permissions are required.\n\n"
        "You will be prompted by Windows to allow this.\n\n"
        "Alternatively, you can set this manually without admin rights by creating the following "
        "environment variable:\n\n"
        "    XR_RUNTIME_JSON=" +
        runtime_json.string() +
        "\n\n"
        "Press OK to proceed with the admin permission prompt, or Cancel to skip.";

    int result = MessageBoxA(nullptr, message.c_str(), "OpenXR Runtime Registration", MB_OKCANCEL | MB_ICONINFORMATION);

    if (result != IDOK) {
        status_message = "Cancelled by user";
        return;
    }

    // Write a small helper .reg-style command via elevated powershell
    std::string ps_command =
        "Set-ItemProperty -Path 'HKLM:\\SOFTWARE\\Khronos\\OpenXR\\1' "
        "-Name 'ActiveRuntime' -Value '" +
        runtime_json.string() + "'";

    // Escape for ShellExecute
    std::string args = "-NoProfile -NonInteractive -Command \"" + ps_command + "\"";

    SHELLEXECUTEINFOA sei = {};
    sei.cbSize = sizeof(sei);
    sei.lpVerb = "runas";
    sei.lpFile = "powershell.exe";
    sei.lpParameters = args.c_str();
    sei.nShow = SW_HIDE;
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;

    if (ShellExecuteExA(&sei)) {
        WaitForSingleObject(sei.hProcess, INFINITE);
        DWORD exit_code = 1;
        GetExitCodeProcess(sei.hProcess, &exit_code);
        CloseHandle(sei.hProcess);
        status_message = (exit_code == 0) ? "Registered as active OpenXR runtime" : "Failed to set runtime";
    } else {
        // User cancelled UAC or other error
        DWORD err = GetLastError();
        status_message = (err == ERROR_CANCELLED) ? "Cancelled by user" : "Failed to set runtime";
    }

#elif defined(__linux__)
    const char* xdg = getenv("XDG_CONFIG_HOME");
    std::string config_dir = xdg ? std::string(xdg) : std::string(getenv("HOME")) + "/.config";
    std::string openxr_dir = config_dir + "/openxr/1";
    std::string link_path = openxr_dir + "/active_runtime.json";

    try {
        std::filesystem::create_directories(openxr_dir);
        std::filesystem::remove(link_path);
        std::filesystem::create_symlink(runtime_json.string(), link_path);
        status_message = "Registered as active OpenXR runtime";
    } catch (const std::filesystem::filesystem_error& e) {
        status_message = std::string("Failed to set runtime: ") + e.what();
    }

#elif defined(__APPLE__)
    const char* home = getenv("HOME");
    std::string openxr_dir = std::string(home) + "/Library/Application Support/OpenXR/1";
    std::string link_path = openxr_dir + "/active_runtime.json";

    try {
        std::filesystem::create_directories(openxr_dir);
        std::filesystem::remove(link_path);
        std::filesystem::create_symlink(runtime_json.string(), link_path);
        status_message = "Registered as active OpenXR runtime";
    } catch (const std::filesystem::filesystem_error& e) {
        status_message = std::string("Failed to set runtime: ") + e.what();
    }
#endif
}

template <typename T>
constexpr T pi = T(3.14159265358979323846L);

template <typename T>
constexpr T DegToRad(T deg) {
    return deg * pi<T> / T(180);
}

template <typename T>
constexpr T RadToDeg(T rad) {
    return rad * T(180) / pi<T>;
}

}  // namespace utils
}  // namespace ox_sim