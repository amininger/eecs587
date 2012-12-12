/////////////////////////////////////////////////////////////////
// pushd command file.
//
// Author: Jonathan Voigt, voigtjr@gmail.com
// Date  : 2004
//
/////////////////////////////////////////////////////////////////

#include <portability.h>

#include "sml_Utils.h"
#include "cli_CommandLineInterface.h"

#include "cli_Commands.h"

using namespace cli;

bool CommandLineInterface::DoPushD(const std::string& directory) {
    
    // Sanity check thanks to rchong
    if (directory.length() == 0) return true;

    // Target directory required, checked in DoCD call.

    // Save the current (soon to be old) directory
    std::string oldDirectory;
    if (!GetCurrentWorkingDirectory(oldDirectory)) return false;// Error message handled in function

    // Change to the new directory.
    if (!DoCD(&directory)) return false;// Error message handled in function

    // Directory change successful, store old directory and move on
    m_DirectoryStack.push(oldDirectory);
    return true;
}

