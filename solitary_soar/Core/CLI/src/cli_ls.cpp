/////////////////////////////////////////////////////////////////
// ls command file.
//
// Author: Jonathan Voigt, voigtjr@gmail.com
// Date  : 2004
//
/////////////////////////////////////////////////////////////////

#include <portability.h>

#include "sml_Utils.h"
#include "cli_CommandLineInterface.h"
#include "cli_Commands.h"
#include "sml_Names.h"

#include <errno.h>

using namespace cli;
using namespace sml;

bool CommandLineInterface::DoLS() {

#ifdef WIN32

    // Windows-specific directory listing
    WIN32_FIND_DATA FindFileData;
    HANDLE hFind;

    // Open a file find using the universal dos wildcard *.*
    hFind = FindFirstFile("*.*", &FindFileData);
    if (hFind == INVALID_HANDLE_VALUE) {
        
        // Not a single file, or file system error, we'll just assume no files
        return true;    
    }

    // At least one file found, concatinate additional ones with newlines
    do {
        if (m_RawOutput)
            m_Result << '\n';
        PrintFilename(FindFileData.cFileName, FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ? true : false);

    } while (FindNextFile(hFind, &FindFileData));

    // Close the file find
    FindClose(hFind);
    return true;

#else // WIN32
    DIR* directoryPointer;
    struct dirent* entry;

    // Get the current working directory and store in dir
    std::string dir;
    if (!GetCurrentWorkingDirectory(dir)) return false;

    // Open the directory for reading
    if ((directoryPointer = opendir(dir.c_str())) == 0) return SetError("Error opening directory.");

    // Read the files
    errno = 0;
    while ((entry = readdir(directoryPointer)) != 0) {
        m_Result << '\n';
#ifndef DT_DIR
#define DT_DIR 4
#endif
        PrintFilename(entry->d_name, entry->d_type == DT_DIR);
    }

    // Check for error
    if (errno != 0) return SetError("Error reading directory.");

    // Ignoring close error
    closedir(directoryPointer);

    return true;
#endif // WIN32
}

void CommandLineInterface::PrintFilename(const std::string& name, bool isDirectory) {
    if (m_RawOutput) {
        if (isDirectory) {
            m_Result << '[';
        }
        m_Result << name;
        if (isDirectory) {
            m_Result << ']';
        }
    } else {
        if (isDirectory) {
            AppendArgTag(sml_Names::kParamDirectory, sml_Names::kTypeString, name);
        } else {
            AppendArgTag(sml_Names::kParamFilename, sml_Names::kTypeString, name);
        }
    }
}
