#include <portability.h>

/////////////////////////////////////////////////////////////////
// NamedPipe class
//
// Author: Bob Marinier
// Date  : 5/2007
//
// Based on Socket class.
//
// Represents a named pipe.
//
// Instances of this class are not created directly.
//
// A server creates a listener named pipe (a derived class)
// which is used to listen for incoming connections on a particular pipe.
//
// A client then connects to that listener pipe (it needs to know the name)
// through the "client named pipe" class.
//
// The client continues to use the client named pipe object it created.
// The server is passed a new named pipe when it checks for incoming connections
// on the listener pipe.
// 
/////////////////////////////////////////////////////////////////

#ifdef ENABLE_NAMED_PIPES

#include <stdio.h>
#include "sml_Utils.h"
#include "sock_NamedPipe.h"

#include <assert.h>

#ifdef PIPE_NON_BLOCKING
#include "sml_Utils.h"	// For sml::Sleep
#endif

using namespace sock ;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

NamedPipe::NamedPipe()
{
	m_hPipe = INVALID_HANDLE_VALUE ;
	m_bTraceCommunications = false ;
}

NamedPipe::NamedPipe(HANDLE hPipe)
{
	m_hPipe = hPipe ;
	m_bTraceCommunications = false ;
}

NamedPipe::~NamedPipe()
{
	Close();
}

#ifdef PIPE_NON_BLOCKING
static bool IsErrorWouldBlock()
{
	int error = PIPE_ERROR_NUMBER ;

	return (error == PIPE_NO_DATA) ;
}
#endif

/////////////////////////////////////////////////////////////////////
// Function name  : NamedPipe::SendBuffer
// 
// Return type    : bool 	
// Argument       : char* pSendBuffer	
// Argument       : uint32_t bufferSize	
// 
// Description	  : Send a buffer of data to a pipe.
//					This may require repeated calls to the low level "send" call.
//
/////////////////////////////////////////////////////////////////////
bool NamedPipe::SendBuffer(char const* pSendBuffer, uint32_t bufferSize)
{
	CTDEBUG_ENTER_METHOD("NamedPipe::SendBuffer");

	CHECK_RET_FALSE(pSendBuffer && bufferSize > 0) ;

	HANDLE hPipe = m_hPipe ;

	if (!hPipe)
	{
		sml::PrintDebug("Error: Can't send because this pipe is closed") ;
		return false; 
	}

	DWORD thisSend = 0;
	uint32_t bytesSent = 0 ;
	int    success = 0 ;

	// May need repeated calls to send all of the data.
	while (bytesSent < bufferSize)
	{
		do
		{
			success = WriteFile( 
				hPipe,        // handle to pipe 
				pSendBuffer,      // buffer to write from 
				bufferSize, // number of bytes to write 
				&thisSend,   // number of bytes written 
				NULL);        // not overlapped I/O 

			// Check if there was an error
			if (!success)
			{
#ifdef PIPE_NON_BLOCKING
				// On a non-blocking pipe, the pipe can return "pipe closing" -- in which case
				// we need to wait for it to clear.  A blocking pipe would not return in this case
				// so this would always be an error.
				if (IsErrorWouldBlock())
				{
					sml::PrintDebug("Waiting for pipe to unblock") ;
					sml::Sleep(0, 0) ;
				}
				else
#endif
				{
					//sml::PrintDebug("Error: Error sending message") ;
					sml::ReportSystemErrorMessage() ;
					return false ;
				}
			}
		} while (!success) ;

		if (m_bTraceCommunications)
			sml::PrintDebugFormat("Sent %d bytes",thisSend) ;

		bytesSent   += thisSend ;
		pSendBuffer += thisSend ;
	}

	return true ;
}

/////////////////////////////////////////////////////////////////////
// Function name  : NamedPipe::IsReadDataAvailable
// 
// Argument		  : int secondsWait -- Seconds part of how long to wait for data in secs (0 is default)
// Argument		  : int millisecondssecondsWait -- Milliseconds part of how long to wait for data in usecs (0 is default, must be < 1000)
// Return type    : bool 	
// 
// Description	  : Returns true if data is waiting to be read on this pipe.
//					Also returns true if the pipe is closed.
//					In that case the next read will return 0 bytes w/o an error
//					indicating that the pipe is closed.
//
/////////////////////////////////////////////////////////////////////
bool NamedPipe::IsReadDataAvailable(int /*secondsWait*/, int /*millisecondsWait*/)
{
	CTDEBUG_ENTER_METHOD("NamedPipe::IsReadDataAvailable");

	HANDLE hPipe = m_hPipe ;

	if (hPipe == INVALID_HANDLE_VALUE)
	{
		sml::PrintDebug("Error: Can't check for read data because this pipe is closed") ;
		return false;
	}

	// Check if anything is waiting to be read
	DWORD bytesAvail = 0;
	int res = 0;

	res = PeekNamedPipe(hPipe, NULL, NULL, NULL, &bytesAvail, NULL);

	// Did an error occur?
	if (res == 0)
	{
		// if the pipe hasn't been connected yet, then just return
		if(GetLastError() == ERROR_BAD_PIPE) {
			return false;
		}

		if(GetLastError() == ERROR_BROKEN_PIPE) {
			//sml::PrintDebug("Remote pipe has closed gracefully") ;
			Close() ;
			return false;
		}

		//sml::PrintDebug("Error: Error checking if data is available to be read") ;
		sml::ReportSystemErrorMessage() ;

		// We treat these errors as all being fatal, which they all appear to be.
		// If we later decide we can survive certain ones, we should test for them here
		// and not always close the socket.
		//sml::PrintDebug("Closing our side of the pipe because of error") ;
		Close() ;

		return false ;
	}

	return bytesAvail>0 ;
}

/////////////////////////////////////////////////////////////////////
// Function name  : ReceiveBuffer
// 
// Return type    : bool 	
// Argument       : char* pRecvBuffer	
// Argument       : uint32_t bufferSize	
// 
// Description	  : Receive a buffer of data.
//
/////////////////////////////////////////////////////////////////////
bool NamedPipe::ReceiveBuffer(char* pRecvBuffer, uint32_t bufferSize)
{
	CTDEBUG_ENTER_METHOD("Socket::ReceiveBuffer");

	CHECK_RET_FALSE(pRecvBuffer && bufferSize > 0) ;

	HANDLE hPipe = m_hPipe ;

	if (hPipe==INVALID_HANDLE_VALUE)
	{
		sml::PrintDebug("Error: Can't read because this pipe is closed") ;
		return false;
	}

	uint32_t bytesRead = 0 ;
	DWORD thisRead  = 0 ;
	int success = 0;

	// Check our incoming data is valid
	if (!pRecvBuffer || hPipe==INVALID_HANDLE_VALUE)
	{
		assert(pRecvBuffer && hPipe==INVALID_HANDLE_VALUE) ;
		return false ;
	}

	// May need to make repeated calls to read all of the data
	while (bytesRead < bufferSize)
	{
		long tries = 0 ;

		do
		{
			tries++ ;

			success = ReadFile( 
				hPipe,        // handle to pipe 
				pRecvBuffer,    // buffer to receive data 
				bufferSize, // size of buffer 
				&thisRead, // number of bytes read 
				NULL);        // not overlapped I/O 

			// Check if there was an error
			if (!success)
			{
#ifdef PIPE_NON_BLOCKING
				// On a non-blocking pipe, the socket may return "pipe closing" -- in which case
				// we need to wait for it to clear.  A blocking pipe would not return in this case
				// so this would always be an error.
				if (IsErrorWouldBlock())
				{
					//sml::PrintDebug("Waiting for pipe to unblock") ;
					sml::Sleep(0, 0) ;	// BADBAD: Should have a proper way to pass control back to the caller while we're blocked.
				}
				else
#endif
				{

					// We treat these errors as all being fatal, which they all appear to be.
					// If we later decide we can survive certain ones, we should test for them here
					// and not always close the socket.
					//sml::PrintDebug("Closing our side of the pipe because of error") ;
					//Close() ;

					//return false ;

               sml::Sleep(0,1);
				}
			}

			// Check for 0 bytes read--which is the behavior if the remote pipe is
			// closed gracefully.
			if (thisRead == 0)
			{
				if(m_bTraceCommunications) sml::PrintDebug("Remote pipe has closed gracefully") ;

				// Now close down our socket
				//sml::PrintDebug("Closing our side of the pipe") ;

				Close() ;

				return false ;	// No message received.
			}

		} while (!success && tries < 3) ;

      if(!success)
      {
         if(m_bTraceCommunications)
         {
            sml::PrintDebug("Error: Error receiving message (named pipe)") ;
		      sml::ReportSystemErrorMessage() ;
         }
         return false;
      }

		//if(tries>1)	sml::PrintDebugFormat("Number tries %d",tries) ;

		if (m_bTraceCommunications)
			sml::PrintDebugFormat("Received %d bytes",thisRead) ;

		bytesRead   += thisRead ;
		pRecvBuffer += thisRead ;
	}

	return true ;
}

/////////////////////////////////////////////////////////////////////
// Function name  : NamedPipe::CloseInternal
// 
// Return type    : void 	
// 
// Description	  : Close down the pipe.
//
/////////////////////////////////////////////////////////////////////
void NamedPipe::CloseInternal()
{
	if (m_hPipe != INVALID_HANDLE_VALUE)
	{
		FlushFileBuffers(m_hPipe); 
		DisconnectNamedPipe(m_hPipe); 
		CloseHandle(m_hPipe); 

		m_hPipe = INVALID_HANDLE_VALUE ;
	}
}

#endif // ENABLE_NAMED_PIPES
