#include "stdafx.h"
#include "ModsLocation.h"
#include "FileHandles.h"
#include "Paths.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>

#include "Logger.h"

// How deep to walk below a mod directory looking for $PYTHIA$ markers.
static const int PYTHIA_SCAN_DEPTH = 2;

typedef std::unordered_set<tstring> dlist;

bool hasEnding(tstring const &fullString, tstring const &ending) {
    if (fullString.length() >= ending.length()) {
        return (0 == fullString.compare(fullString.length() - ending.length(), ending.length(), ending));
    }
    else {
        return false;
    }
}

dlist getDirectories(tstring const & fileExtension=LITERAL(""))
{
    WStringVector files;
    dlist directories;
    int retval = getOpenFiles(files);
    if (!retval)
        return directories;

    for (auto &file : files)
    {
        if (hasEnding(file, fileExtension))
        {
            directories.insert(getPathDirectory(file));
        }
    }

    return directories;
}

static bool validPythiaModuleName(std::string name)
{
    if (name.length() < 1)
    {
        return false;
    }

    for (char &c : name)
    {
        if (!isalnum(c) && c != '_')
        {
            return false;
        }
    }
    return true;
}

static std::string getPythiaModuleName(std::ifstream &stream)
{
    std::string pythiaModuleName;
    stream >> pythiaModuleName;
    return pythiaModuleName;
}

/**
   Check if the given directory contains python code usable by Pythia.
 */
static void tryAddingPythiaModule(modules_t &modules, const tstring path)
{
    auto pythiaFile = std::filesystem::path(path) / LITERAL("$PYTHIA$");

    std::ifstream pythiaFileHandle;
    pythiaFileHandle.open(pythiaFile, std::ios::binary);
    if (!pythiaFileHandle.good())
    {
        // File probably doesn't exist
        return;
    }

    std::string pythiaModuleName = getPythiaModuleName(pythiaFileHandle);
    if (!validPythiaModuleName(pythiaModuleName))
    {
        return;
    }

    // Add the current directory to the mods list
    modules[pythiaModuleName] = path;
}

/**
   Directories that never hold Pythia modules. Skipped while scanning to keep
   things fast and avoid descending into large asset trees.
 */
static bool isNoiseDirectory(const tstring &name)
{
    static const std::unordered_set<tstring> noise = {
        LITERAL("addons"), LITERAL("keys"), LITERAL("dta"), LITERAL("bin"),
        LITERAL("missions"), LITERAL("mpmissions"), LITERAL("__pycache__"), LITERAL(".git"),
    };

    tstring lower = name;
    for (auto &c : lower)
    {
        if (c < 128)
        {
            c = static_cast<tstring::value_type>(std::tolower(static_cast<int>(c)));
        }
    }

    return noise.find(lower) != noise.end();
}

/**
   Walk `root` up to `depth` levels deep, registering every directory that holds
   a $PYTHIA$ marker. The root itself is always checked.
 */
static void scanRecursively(modules_t &modules, const std::filesystem::path &root, int depth)
{
    tryAddingPythiaModule(modules, root);

    if (depth <= 0)
    {
        return;
    }

    std::error_code ec;
    auto directory_iter = std::filesystem::directory_iterator(root, ec);
    if (ec)
    {
        return;
    }

    for (const auto &entry : directory_iter)
    {
        if (!std::filesystem::is_directory(entry, ec) || ec)
        {
            continue;
        }
        if (isNoiseDirectory(entry.path().filename().native()))
        {
            continue;
        }
        scanRecursively(modules, entry.path(), depth - 1);
    }
}

/**
   Register modules listed in the PYTHIA_PATH environment variable. Each entry
   (separated by ';' on Windows, ':' elsewhere) is treated as a directory that
   directly contains a $PYTHIA$ marker. These take precedence over discovered
   modules because they are added last.
 */
static void addModulesFromEnv(modules_t &modules)
{
#ifdef _WIN32
    const wchar_t *raw = _wgetenv(L"PYTHIA_PATH");
    const wchar_t sep = L';';
#else
    const char *raw = getenv("PYTHIA_PATH");
    const char sep = ':';
#endif
    if (!raw)
    {
        return;
    }

    tstring value(raw);
    size_t start = 0;
    while (start <= value.size())
    {
        size_t end = value.find(sep, start);
        if (end == tstring::npos)
        {
            end = value.size();
        }

        tstring dir = value.substr(start, end - start);
        if (!dir.empty())
        {
            tryAddingPythiaModule(modules, dir);
        }
        start = end + 1;
    }
}

/**
   Get loaded python mods.
   Scan all the open file handles for .pbo files and open the parent directory.
   Those are hopefully mod dirs. Each is walked recursively (bounded depth)
   looking for $PYTHIA$ markers. Finally, PYTHIA_PATH entries are added on top.

   Return the list of module names along with paths to those modules.
   (string -> wstring)
 */
modules_t getPythiaModulesSources()
{
    modules_t modules;

    dlist directoriesList = getDirectories(LITERAL(".pbo"));
    for (auto &directory : directoriesList)
    {
        std::filesystem::path parent = getPathDirectory(directory);
        scanRecursively(modules, parent, PYTHIA_SCAN_DEPTH);
    }

    addModulesFromEnv(modules);

    return modules;
}
