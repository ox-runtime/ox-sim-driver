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
inline std::string ShellQuote(const std::string& value) {
    std::string result = "'";
    for (char ch : value) {
        if (ch == '\'') {
            result += "'\\''";
        } else {
            result += ch;
        }
    }
    result += "'";
    return result;
}

inline std::string EscapeAppleScriptString(const std::string& value) {
    std::string result;
    result.reserve(value.size());
    for (char ch : value) {
        switch (ch) {
            case '\\':
                result += "\\\\";
                break;
            case '"':
                result += "\\\"";
                break;
            case '\n':
                result += "\\n";
                break;
            default:
                result += ch;
                break;
        }
    }
    return result;
}

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

    std::string message =
        "To register as the active OpenXR runtime, administrator permissions are required.\n\n"
        "macOS will prompt you for permission to update the system runtime link at:\n\n"
        "    " +
        link_path +
        "\n\n"
        "Alternatively, you can set this manually for the current shell with:\n\n"
        "    XR_RUNTIME_JSON=" +
        runtime_json.string() +
        "\n\n"
        "Press OK to continue, or Cancel to skip.";

    std::string prompt_script = "display dialog \"" + EscapeAppleScriptString(message) +
                                "\" buttons {\"Cancel\", \"OK\"} default button \"OK\" with icon note";
    int prompt_result = std::system(("osascript -e " + ShellQuote(prompt_script)).c_str());
    if (prompt_result != 0) {
        status_message = "Cancelled by user";
        return;
    }

    std::string shell_command = "mkdir -p " + ShellQuote(openxr_dir) + "; rm -f " + ShellQuote(link_path) +
                                "; ln -sfn " + ShellQuote(runtime_json.string()) + " " + ShellQuote(link_path);
    std::string elevate_script =
        "do shell script \"" + EscapeAppleScriptString(shell_command) + "\" with administrator privileges";

    int result = std::system(("osascript -e " + ShellQuote(elevate_script)).c_str());
    status_message = (result == 0) ? "Registered as active OpenXR runtime" : "Failed to set runtime";
#endif
}

}  // namespace utils
}  // namespace ox_sim