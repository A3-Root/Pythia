#include "stdafx.h"
#include <string>
#include <vector>
#include <cstdlib>
#include <filesystem>
#include "Logger.h"

std::string GetCurrentWorkingDir()
{
    std::error_code ec;
    const auto current_path = std::filesystem::current_path(ec);
    if (ec)
    {
        LOG_ERROR("Error getting current working directory!");
        return "";
    }

    return current_path.string();
}

#ifdef _WIN32
#include <direct.h>

std::wstring getPathDirectory(const std::wstring& path)
{
    // Returns the whole string if no backslash is present
    return path.substr(0, path.find_last_of(L"/\\"));
}

std::string getPathDirectory(const std::string& path)
{
    // Returns the whole string if no backslash is present
    return path.substr(0, path.find_last_of("/\\"));
}

std::wstring getProgramPath()
{
    wchar_t buff[MAX_PATH] = { 0 };
    if (GetModuleFileNameW(NULL, buff, FILENAME_MAX) == 0)
    {
        LOG_ERROR("Error getting executable path!");
        return L"";
    }
    return buff;
}

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

std::wstring getDllPath()
{
    // https://stackoverflow.com/questions/6924195/get-dll-path-at-runtime
    wchar_t DllPath[MAX_PATH] = { 0 };
    if (GetModuleFileNameW((HINSTANCE)&__ImageBase, DllPath, _countof(DllPath)) == 0)
    {
        LOG_ERROR("Error getting Pythia DLL path");
        return L"";
    }
    return DllPath;
}

// Read a string value from a registry key. An empty valueName reads the key's
// default value. Honors the 32/64-bit registry view matching this build so a
// 32-bit Pythia finds a 32-bit Python and vice versa.
static std::wstring readRegistryString(HKEY root, const std::wstring& subkey, const std::wstring& valueName)
{
    HKEY hKey;
    REGSAM access = KEY_READ | (sizeof(void*) == 8 ? KEY_WOW64_64KEY : KEY_WOW64_32KEY);
    if (RegOpenKeyExW(root, subkey.c_str(), 0, access, &hKey) != ERROR_SUCCESS)
    {
        return L"";
    }

    wchar_t buffer[MAX_PATH] = { 0 };
    DWORD size = sizeof(buffer);
    DWORD type = 0;
    LSTATUS status = RegQueryValueExW(
        hKey,
        valueName.empty() ? nullptr : valueName.c_str(),
        nullptr, &type, reinterpret_cast<LPBYTE>(buffer), &size);
    RegCloseKey(hKey);

    if (status != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ))
    {
        return L"";
    }

    return buffer;
}

static std::wstring ifExists(const std::wstring& path)
{
    std::error_code ec;
    return (!path.empty() && std::filesystem::exists(path, ec)) ? path : L"";
}

// Locate the system python.exe for the Python version Pythia was built against.
// Probes, in order: PYTHIA_PYTHON_HOME, PYTHONHOME, the Windows registry
// (per-user then machine-wide), and finally each directory on PATH.
tstring discoverPythonExecutable()
{
    const std::wstring versionKey =
        L"SOFTWARE\\Python\\PythonCore\\" + Logger::s2w(PYTHON_VERSION_DOTTED) + L"\\InstallPath";
    std::wstring found;

    if (const wchar_t* home = _wgetenv(L"PYTHIA_PYTHON_HOME"))
    {
        found = ifExists(std::wstring(home) + L"\\python.exe");
        if (!found.empty()) return found;
    }

    if (const wchar_t* home = _wgetenv(L"PYTHONHOME"))
    {
        found = ifExists(std::wstring(home) + L"\\python.exe");
        if (!found.empty()) return found;
    }

    for (HKEY root : { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE })
    {
        // ExecutablePath points straight at python.exe; fall back to InstallPath dir.
        found = ifExists(readRegistryString(root, versionKey, L"ExecutablePath"));
        if (!found.empty()) return found;

        std::wstring dir = readRegistryString(root, versionKey, L"");
        if (!dir.empty())
        {
            found = ifExists(dir + L"\\python.exe");
            if (!found.empty()) return found;
        }
    }

    if (const wchar_t* pathEnv = _wgetenv(L"PATH"))
    {
        std::wstring paths(pathEnv);
        size_t start = 0;
        while (start <= paths.size())
        {
            size_t end = paths.find(L';', start);
            if (end == std::wstring::npos) end = paths.size();

            std::wstring dir = paths.substr(start, end - start);
            if (!dir.empty())
            {
                found = ifExists(dir + L"\\python.exe");
                if (!found.empty()) return found;
            }
            start = end + 1;
        }
    }

    return L"";
}

// Directory containing python.exe, used so the OS can resolve pythonXY.dll.
std::wstring getPythonDirectory()
{
    auto exe = discoverPythonExecutable();
    return exe.empty() ? L"" : getPathDirectory(exe);
}

#else

#include <unistd.h>
#include <dlfcn.h>

std::string getPathDirectory(const std::string& path)
{
    // Returns the whole string if no backslash is present
    return path.substr(0, path.find_last_of("/"));
}

std::string getProgramPath()
{
    std::error_code ec;
    auto selfPath = std::filesystem::path("/proc/self/exe");
    auto isSymlink = std::filesystem::is_symlink(selfPath, ec);
    if (!isSymlink)
    {
        if (ec)
        {
            LOG_ERROR(std::string("Could not check symlink ") + selfPath.c_str() + ": " + ec.message());
        }
        else
        {
            LOG_ERROR("Error getting executable path! (/proc/self/exe is not a symlink)");
        }

        return "";
    }

    auto self = std::filesystem::read_symlink(selfPath, ec);
    if (ec)
    {
        LOG_ERROR("Error getting executable path! (" + ec.message() + ")");
        return "";
    }

    return self;
}

std::string getSoPath()
{
    // Link with -ldl
    Dl_info dlInfo;
    bool success = dladdr((void*)getSoPath, &dlInfo);

    if (!success)
    {
        LOG_ERROR("Error getting Pythia .so path (dladdr failed)");
        return "";
    }

    if (dlInfo.dli_sname != NULL && dlInfo.dli_saddr != NULL)
        return dlInfo.dli_fname;
    else
    {
        LOG_ERROR("Error getting Pythia .so path (dlInfo.dli_sname == NULL or dlInfo.dli_saddr == NULL)");
        return "";
    }
}

// Locate the system python3 interpreter for the version Pythia was built
// against. Probes PYTHIA_PYTHON_HOME, then each directory on PATH, preferring
// the exact minor (python3.X) over the generic python3/python names.
tstring discoverPythonExecutable()
{
    const std::string minor = PYTHON_VERSION_DOTTED; // e.g. "3.12"
    const std::vector<std::string> names = { "python" + minor, "python3", "python" };
    std::error_code ec;

    if (const char* home = getenv("PYTHIA_PYTHON_HOME"))
    {
        for (const auto& rel : { std::string("/bin/python") + minor, std::string("/bin/python3") })
        {
            std::string candidate = std::string(home) + rel;
            if (std::filesystem::exists(candidate, ec)) return candidate;
        }
    }

    if (const char* pathEnv = getenv("PATH"))
    {
        std::string paths(pathEnv);
        size_t start = 0;
        while (start <= paths.size())
        {
            size_t end = paths.find(':', start);
            if (end == std::string::npos) end = paths.size();

            std::string dir = paths.substr(start, end - start);
            if (!dir.empty())
            {
                for (const auto& name : names)
                {
                    std::string candidate = dir + "/" + name;
                    if (std::filesystem::exists(candidate, ec)) return candidate;
                }
            }
            start = end + 1;
        }
    }

    return "";
}
#endif

tstring getProgramDirectory()
{
    return getPathDirectory(getProgramPath());
}
