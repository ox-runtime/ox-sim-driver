#pragma once

#include <whereami.h>

#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace ox_sim {
namespace utils {
inline fs::path GetExecutablePath() {
    const int len = wai_getExecutablePath(nullptr, 0, nullptr);
    if (len <= 0) {
        return {};
    }

    std::vector<char> buffer(static_cast<size_t>(len) + 1, '\0');
    wai_getExecutablePath(buffer.data(), len, nullptr);
    buffer[static_cast<size_t>(len)] = '\0';
    return fs::path(buffer.data());
}

inline fs::path GetRuntimeJsonPath() {
    const auto executable_path = GetExecutablePath();
    return executable_path.parent_path() / "ox_runtime.json";
}

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
        fs::create_directories(openxr_dir);
        fs::remove(link_path);
        fs::create_symlink(runtime_json.string(), link_path);
        status_message = "Registered as active OpenXR runtime";
    } catch (const fs::filesystem_error& e) {
        status_message = std::string("Failed to set runtime: ") + e.what();
    }

#elif defined(__APPLE__)
    std::string openxr_dir = "/usr/local/share/openxr/1";
    std::string link_path = openxr_dir + "/active_runtime.json";

    try {
        fs::create_directories(openxr_dir);
        fs::remove(link_path);
        fs::create_symlink(runtime_json.string(), link_path);
        status_message = "Registered as active OpenXR runtime";
    } catch (const fs::filesystem_error& e) {
        status_message = std::string("Failed to set runtime: ") + e.what();
    }
#endif
}

}  // namespace utils
}  // namespace ox_sim