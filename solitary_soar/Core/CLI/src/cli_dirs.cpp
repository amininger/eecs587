/////////////////////////////////////////////////////////////////
// dirs command file.
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

using namespace cli;
using namespace sml;

bool CommandLineInterface::DoDirs() {
    
    StringStack tempStack;

    std::string cwd;
    if (!GetCurrentWorkingDirectory(cwd)) return false;

    // cwd is top of stack
    if (m_RawOutput) {
        m_Result << cwd;
    } else {
        AppendArgTagFast(sml_Names::kParamDirectory, sml_Names::kTypeString, cwd);
    }

    // print rest of stack making a new one
    while (m_DirectoryStack.size()) {
        if (m_RawOutput) {
            m_Result << ' ' << m_DirectoryStack.top();
        } else {
            AppendArgTagFast(sml_Names::kParamDirectory, sml_Names::kTypeString, m_DirectoryStack.top());
        }

        tempStack.push(m_DirectoryStack.top());
        m_DirectoryStack.pop();
    }

    // put the old stack back together
    while (tempStack.size()) {
        m_DirectoryStack.push(tempStack.top());
        tempStack.pop();
    }
    return true;
}

