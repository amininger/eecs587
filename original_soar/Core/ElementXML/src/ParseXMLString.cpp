#include <portability.h>

/////////////////////////////////////////////////////////////////
// ParseXMLString class
//
// Author: Douglas Pearson, www.threepenny.net
// Date  : August 2004
//
// This class is used to parse an XML document from a file/string and
// create an ElementXML object that represents it.
//
// This class reads from a string.
//
/////////////////////////////////////////////////////////////////

#include "ParseXMLString.h"
#include "ElementXMLImpl.h"

using namespace soarxml;

ParseXMLString::ParseXMLString(char const* pInputLine, size_t startPos)
{
	m_pInputLine = pInputLine ;
	m_Pos = startPos ;
	m_StartTokenPos = m_Pos ;
	m_LineLength = strlen(m_pInputLine) ;

	InitializeLexer() ;
}

ParseXMLString::~ParseXMLString(void)
{
}

void ParseXMLString::ReadLine()
{
	if (!m_pInputLine)
	{
		RecordError("Invalid input string") ;
		return ;
	}

	// We only have one input string, so just check for EOF in
	// calls to read more lines.
	if (m_Pos >= m_LineLength)
	{
		m_IsEOF = true ;
		return ;
	}
}
