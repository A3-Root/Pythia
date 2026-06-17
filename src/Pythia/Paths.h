#pragma once
#include "stdafx.h"
#include <string>

std::string GetCurrentWorkingDir();
std::wstring getPathDirectory(const std::wstring& path);
std::string getPathDirectory(const std::string& path);
tstring getProgramPath();
tstring getProgramDirectory();
tstring getDllPath();

// Locate the system Python interpreter Pythia was built against (full path to
// python.exe / python3). Returns an empty string if none is found.
tstring discoverPythonExecutable();

#ifdef _WIN32
// Directory holding python.exe, passed to SetDllDirectory so pythonXY.dll resolves.
std::wstring getPythonDirectory();
#endif
