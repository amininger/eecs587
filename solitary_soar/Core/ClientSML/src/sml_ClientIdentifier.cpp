#include <portability.h>

/////////////////////////////////////////////////////////////////
// Identifier class
//
// Author: Douglas Pearson, www.threepenny.net
// Date  : Sept 2004
//
// Working memory element that has an ID value.
//
/////////////////////////////////////////////////////////////////

#include "sml_ClientIdentifier.h"
#include "sml_ClientStringElement.h"
#include "sml_Connection.h"
#include "sml_ClientAgent.h"
#include "sml_StringOps.h"
#include <assert.h>

#include "sml_ClientDirect.h"
#include "sml_EmbeddedConnection.h"	// For direct methods

using namespace sml ;

IdentifierSymbol::IdentifierSymbol(Identifier* pIdentifier)
{
	m_UsedBy.push_back(pIdentifier) ;
}

IdentifierSymbol::~IdentifierSymbol()
{
	// Nobody should be using this symbol when we're deleted.
	assert (GetNumberUsing() == 0) ;

	DeleteAllChildren() ;
}

void IdentifierSymbol::SetIdentifierSymbol(char const* pID)   
{ 
	// this really should be in the ctor
	assert(pID);
	// this is valid on input-link
	//assert(m_Symbol.empty()); 
	m_Symbol = pID ; 
	//std::cout << "new symbol " << pID << std::endl;
}

void IdentifierSymbol::DeleteAllChildren()
{
	//std::cout << "IdentifierSymbol::DeleteAllChildren" << std::endl;

	// We own all of these children, so delete them when we are deleted.
	for (Identifier::ChildrenIter iter = m_Children.begin() ; iter != m_Children.end() ; iter++)
	{
		WMElement* pWME = *iter ;
		//std::string wmeString;
		//pWME->DebugString(wmeString);
		//std::cout << "deleting: " << wmeString << std::endl;
		delete pWME ;
	}
	m_Children.clear() ;
}

std::list<WMElement*>::iterator IdentifierSymbol::FindChildByTimeTag(long long timeTag)
{
	for (std::list<WMElement*>::iterator i = m_Children.begin(); i != m_Children.end() ; ++i)
	{
		if ((**i).GetTimeTag() == timeTag) {
			return i;
		}
	}
	return m_Children.end();
}

void IdentifierSymbol::AddChild(WMElement* pWME)
{
	// Record that we're changing the list of children in case the
	// client would like to know that this identifier was changed in some fashion.
	SetAreChildrenModified(true) ;

	Identifier::ChildrenIter iter = FindChildByTimeTag(pWME->GetTimeTag());
	if ( iter == m_Children.end() )
	{
		//std::cout << "AddChild: " << pWME->GetIdentifierName() << ", " << pWME->GetAttribute() << ", " << pWME->GetValueAsString() << " (" << pWME->GetTimeTag() << ")" << " (" << pWME << ")" << std::endl;
		m_Children.push_back(pWME) ;
	} 
	else
	{
		//std::cout << "Did not AddChild: " << pWME->GetIdentifierName() << ", " << pWME->GetAttribute() << ", " << pWME->GetValueAsString() << " (" << pWME->GetTimeTag() << ")" << " (" << pWME << ")" << std::endl;
	}
}

WMElement* IdentifierSymbol::GetChildByTimeTag(long long timeTag)
{
	Identifier::ChildrenIter iter = FindChildByTimeTag(timeTag);
	if ( iter != m_Children.end() )
	{
		return *iter;
	}
	return 0;
}

void IdentifierSymbol::TransferChildren(IdentifierSymbol* pDestination)
{
	for (Identifier::ChildrenIter iter = m_Children.begin() ; iter != m_Children.end() ; iter++)
	{
		WMElement* pWME = *iter ;
		pDestination->AddChild(pWME);
		pWME->SetSymbol(pDestination);
	}
	m_Children.clear();
}

void IdentifierSymbol::RemoveChild(WMElement* pWME)
{
	// Record that we're changing the list of children in case the
	// client would like to know that this identifier was changed in some fashion.
	SetAreChildrenModified(true) ;

	Identifier::ChildrenIter iter = FindChildByTimeTag(pWME->GetTimeTag());
	if ( iter != m_Children.end() )
	{
		//std::cout << "RemoveChild: " << pWME->GetIdentifierName() << ", " << pWME->GetAttribute() << ", " << pWME->GetValueAsString() << " (" << pWME->GetTimeTag() << ")" << " (" << pWME << ")" << std::endl;
		m_Children.erase( iter ) ;
	} 
	else
	{
		//std::cout << "Did not RemoveChild: " << pWME->GetIdentifierName() << ", " << pWME->GetAttribute() << ", " << pWME->GetValueAsString() << " (" << pWME->GetTimeTag() << ")" << " (" << pWME << ")" << std::endl;
	}
}

void IdentifierSymbol::NoLongerUsedBy(Identifier* pIdentifier)  
{ 
	m_UsedBy.remove(pIdentifier) ; 
}

void IdentifierSymbol::UsedBy(Identifier* pIdentifier)		  
{
	m_UsedBy.push_back(pIdentifier) ; 
}

void IdentifierSymbol::DebugString(std::string& result)
{
	std::stringstream ss;
	ss << "[" << m_Symbol;
	if (!m_Children.empty())
	{
		ss << ": children:";
		for (Identifier::ChildrenIter iter = m_Children.begin(); iter != m_Children.end(); ++iter)
		{
			ss << " " << (*iter)->GetTimeTag();
		}
	}

	if (!m_UsedBy.empty())
	{
		ss << " used by:";
		for (std::list<Identifier*>::iterator iter = m_UsedBy.begin(); iter != m_UsedBy.end(); ++iter)
		{
			ss << " " << (*iter)->GetTimeTag();
		}
	}
	ss << "]";
	result.assign(ss.str());
}

// This version is only needed at the top of the tree (e.g. the input link)
Identifier::Identifier(Agent* pAgent, char const* pAttributeName, char const* pIdentifier, long long timeTag) 
: WMElement(pAgent, NULL, pIdentifier, pAttributeName, timeTag)
{
	//m_pSymbol = GetAgent()->GetWM()->FindIdentifierSymbol(pIdentifier);
	//if (m_pSymbol)
	//{
	//	m_pSymbol->UsedBy(this);
	//}
	//else
	//{
		m_pSymbol = new IdentifierSymbol(this) ;
		m_pSymbol->SetIdentifierSymbol(pIdentifier) ;
		RecordSymbolInMap();
	//}
	//std::cout << "created (" << this->GetIdentifierName() << " ^" << this->GetAttribute() 
	//	<< " " << this->GetValueAsString() << ": " << this->GetTimeTag() << ")" << std::endl;
}

// The normal case (where there is a parent id)
Identifier::Identifier(Agent* pAgent, Identifier* pParent, char const* pID, char const* pAttributeName, char const* pIdentifier, long long timeTag) 
: WMElement(pAgent, pParent->GetSymbol(), pID, pAttributeName, timeTag)
{
	//m_pSymbol = GetAgent()->GetWM()->FindIdentifierSymbol(pIdentifier);
	//if (m_pSymbol)
	//{
	//	m_pSymbol->UsedBy(this);
	//}
	//else
	//{
		m_pSymbol = new IdentifierSymbol(this) ;
		m_pSymbol->SetIdentifierSymbol(pIdentifier) ;
		RecordSymbolInMap();
	//}
	//std::cout << "created (" << this->GetIdentifierName() << " ^" << this->GetAttribute() 
	//	<< " " << this->GetValueAsString() << ": " << this->GetTimeTag() << ")" << std::endl;
}

Identifier::Identifier(Agent* pAgent, IdentifierSymbol* pParentSymbol, char const* pID, char const* pAttributeName, char const* pIdentifier, long long timeTag) 
: WMElement(pAgent, pParentSymbol, pID, pAttributeName, timeTag)
{
	//m_pSymbol = GetAgent()->GetWM()->FindIdentifierSymbol(pIdentifier);
	//if (m_pSymbol)
	//{
	//	m_pSymbol->UsedBy(this);
	//}
	//else
	//{
		m_pSymbol = new IdentifierSymbol(this) ;
		m_pSymbol->SetIdentifierSymbol(pIdentifier) ;
		RecordSymbolInMap();
	//}
	//std::cout << "created (" << this->GetIdentifierName() << " ^" << this->GetAttribute() 
	//	<< " " << this->GetValueAsString() << ": " << this->GetTimeTag() << ")" << std::endl;
}

// Creating one identifier to have the same value as another
Identifier::Identifier(Agent* pAgent, Identifier* pParent, char const* pID, char const* pAttributeName, Identifier* pLinkedIdentifier, long long timeTag) 
: WMElement(pAgent, pParent->GetSymbol(), pID, pAttributeName, timeTag)
{
	m_pSymbol = pLinkedIdentifier->m_pSymbol ;
	m_pSymbol->UsedBy(this) ;
	//RecordSymbolInMap();	// redundant, if it is on another symbol, it is already in the map
	//std::cout << "created (" << this->GetIdentifierName() << " ^" << this->GetAttribute() 
	//	<< " " << this->GetValueAsString() << ": " << this->GetTimeTag() << ")" << std::endl;
}

Identifier::Identifier(Agent* pAgent, IdentifierSymbol* pParentSymbol, char const* pID, char const* pAttributeName, IdentifierSymbol* pLinkedIdentifierSymbol, long long timeTag) 
: WMElement(pAgent, pParentSymbol, pID, pAttributeName, timeTag)
{
	m_pSymbol = pLinkedIdentifierSymbol;
	m_pSymbol->UsedBy(this) ;
	RecordSymbolInMap(); // potentially redundant
	//std::cout << "created (" << this->GetIdentifierName() << " ^" << this->GetAttribute() 
	//	<< " " << this->GetValueAsString() << ": " << this->GetTimeTag() << ")" << std::endl;
}

void Identifier::RecordSymbolInMap()
{
	GetAgent()->GetWM()->RecordSymbolInMap( m_pSymbol );
}

void Identifier::UpdateSymbol(IdentifierSymbol* pSymbol)
{
	//std::string symString;
	//m_pSymbol->DebugString(symString);
	//std::cout << "UpdateSymbol for wme " << GetTimeTag() << " " << symString;

	m_pSymbol->TransferChildren(pSymbol);

	ReleaseSymbol();

	m_pSymbol = pSymbol;
	m_pSymbol->UsedBy(this) ;
	RecordSymbolInMap();
}

Identifier::~Identifier(void)
{
	//std::string symString;
	//m_pSymbol->DebugString(symString);
	//std::cout << "~Identifier " << GetTimeTag() << " " << symString;

	ReleaseSymbol();
}

void Identifier::ReleaseSymbol()
{
	// Indicate this identifier is no longer using the identifier symbol
	m_pSymbol->NoLongerUsedBy(this) ;

	// Decide if we need to delete the identifier symbol (or is someone else still using it)
	if (m_pSymbol->GetNumberUsing() == 0)
	{
		GetAgent()->GetWM()->RemoveSymbolFromMap( m_pSymbol );
		delete m_pSymbol ;
	}

	m_pSymbol = NULL ;
}

void Identifier::SetSymbol(IdentifierSymbol* p_ID)
{
	WMElement::SetSymbol(p_ID) ;
}

/*************************************************************
* @brief Returns the n-th WME that has the given attribute
*		 and this identifier as its parent (or NULL).
*
* @param pAttribute		The name of the attribute to match
* @param index			0 based index of values for this attribute
*					   (> 0 only needed for multi-valued attributes)
*************************************************************/
WMElement* Identifier::FindByAttribute(char const* pAttribute, int index) const
{
	for (ChildrenConstIter iter = m_pSymbol->m_Children.begin() ; iter != m_pSymbol->m_Children.end() ; iter++)
	{
		WMElement* pWME = *iter ;

		if (IsStringEqualIgnoreCase(pWME->GetAttribute(), pAttribute))
		{
			if (index == 0)
				return pWME ;
			index-- ;
		}
	}

	return NULL ;
}

/*************************************************************
* @brief Adds "^status complete" as a child of this identifier.
*************************************************************/
void Identifier::AddStatusComplete()
{
	GetAgent()->CreateStringWME(this, "status", "complete") ;
}

/*************************************************************
* @brief Adds "^status error" as a child of this identifier.
*************************************************************/
void Identifier::AddStatusError()
{
	GetAgent()->CreateStringWME(this, "status", "error") ;
}

/*************************************************************
* @brief Adds "^error-code <code>" as a child of this identifier.
*************************************************************/
void Identifier::AddErrorCode(int errorCode)
{
	GetAgent()->CreateIntWME(this, "error-code", errorCode) ;
}

/*************************************************************
* @brief Searches for a child of this identifier that has the given
*		 time tag.
*		 (The search is recursive over all children).
*
* @param timeTag	The tag to look for (e.g. +12 for kernel side or -15 for client side)
*************************************************************/
WMElement* Identifier::FindFromTimeTag(long long timeTag) const
{
	// SLOWSLOW: We could use a hash table to speed this up and replace O(n) with O(1).
	// Right now that will only impact performance when elements are removed from the output link,
	// but if clients start to use this (or the output link is really large for an application)
	// the saving could be significant.

	if (this->GetTimeTag() == timeTag)
		return const_cast<WMElement*>(static_cast<const WMElement*>(this)) ;

	// Go through each child in turn and if it's an identifier search its children for a matching id.
	for (ChildrenConstIter iter = m_pSymbol->m_Children.begin() ; iter != m_pSymbol->m_Children.end() ; iter++)
	{
		WMElement* pWME = *iter ;

		if (pWME->GetTimeTag() == timeTag)
			return pWME ;

		// If this is an identifer, we search deeper for the match
		if (pWME->IsIdentifier())
		{
			Identifier* pId = static_cast<Identifier*>(pWME);
			WMElement* pResult = pId->FindFromTimeTag(timeTag) ;

			if (pResult)
				return pResult ;
		}
	}

	return NULL ;
}

/*************************************************************
* @brief Gets the n-th child.
*        Ownership of this WME is retained by the agent.
*
*		 This is an O(n) operation.  We could expose the iterator directly
*		 but we want to export this interface to Java/Tcl etc. and this is easier.
*************************************************************/
WMElement* Identifier::GetChild(int index)
{
	for (ChildrenIter iter = m_pSymbol->m_Children.begin() ; iter != m_pSymbol->m_Children.end() ; iter++)
	{
		if (index == 0)
			return *iter ;
		index-- ;
	}

	return NULL ;
}

// Returns the type of the value stored here (e.g. "string" or "int" etc.)
char const* Identifier::GetValueType() const
{
	return sml_Names::kTypeID ;
}

// Send over to the kernel again
void Identifier::Refresh()
{
	// Add this identifier into the tree (except for input link which is auto created)
	if (GetAgent()->GetInputLink() != this)
		WMElement::Refresh() ;

	// If this symbol appears as the value for several identifier objects we only
	// want to add the child WMEs once.
	if (m_pSymbol->IsFirstUser(this))
	{
		for (ChildrenIter iter = m_pSymbol->m_Children.begin() ; iter != m_pSymbol->m_Children.end() ; iter++)
		{
			WMElement* pWME = *iter ;
			pWME->Refresh() ;
		}
	}
}

#ifdef SML_DIRECT
void Identifier::DirectAdd(Direct_AgentSML_Handle pAgentSML, long long timeTag)
{
	EmbeddedConnection* pConnection = static_cast<EmbeddedConnection*>(GetAgent()->GetConnection());
	pConnection->DirectAddID( pAgentSML, m_ID->GetIdentifierSymbol(), GetAttribute(), GetValueAsString(), timeTag);
}
#endif

StringElement* Identifier::CreateStringWME(char const* pAttribute, char const* pValue)
{
	return this->m_Agent->GetWM()->CreateStringWME(this, pAttribute, pValue);
}

IntElement* Identifier::CreateIntWME(char const* pAttribute, long long value)
{
	return this->m_Agent->GetWM()->CreateIntWME(this, pAttribute, value);
}

FloatElement* Identifier::CreateFloatWME(char const* pAttribute, double value)
{
	return this->m_Agent->GetWM()->CreateFloatWME(this, pAttribute, value);
}

Identifier* Identifier::CreateIdWME(char const* pAttribute)
{
	return this->m_Agent->GetWM()->CreateIdWME(this, pAttribute);
}

Identifier* Identifier::CreateSharedIdWME(char const* pAttribute, Identifier* pSharedValue)
{
	return this->m_Agent->GetWM()->CreateSharedIdWME(this, pAttribute, pSharedValue);
}
