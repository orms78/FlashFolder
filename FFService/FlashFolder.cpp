/* This file is part of FlashFolder. 
 * Copyright (C) 2007 zett42 ( zett42 at users.sourceforge.net ) 
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
/** \file  Main file to compile the "FlashFolder.exe" service.\n
 *    The service is used to install / uninstall the global FlashFolder hook function
 *    contained in "fflib.dll". 
**/
#include "stdafx.h"
#include "resource.h"
#include "../_version.h"
#include "../fflib/fflib_exports.h"

HINSTANCE g_hInstance = NULL;

TCHAR INTERNAL_APPNAME[ 256 ] = _T("FlashFolder"); 

//---------------------------------------------------------------------------
// global state

SERVICE_STATUS          g_myServiceStatus = { 0 }; 
SERVICE_STATUS_HANDLE   g_myServiceStatusHandle = NULL; 

HANDLE g_hEventTerminate = NULL;

//---------------------------------------------------------------------------

void SetMyServiceStatus( DWORD status, DWORD err = 0 )
{
	if( status != 0xFFFFFFFF )
	{
		g_myServiceStatus.dwWin32ExitCode = err; 
		g_myServiceStatus.dwCurrentState  = status; 
		g_myServiceStatus.dwCheckPoint    = 0; 
		g_myServiceStatus.dwWaitHint      = 0; 
	}
	if( ! ::SetServiceStatus( g_myServiceStatusHandle, &g_myServiceStatus ) )
	{ 
		DWORD err = ::GetLastError(); 
		DebugOut( _T("[FF] SetServiceStatus() error (%d)\n"), err ); 
	} 
}

//---------------------------------------------------------------------------

DWORD MyServiceUninitialization( bool isShutDown )
{
	if( ! UninstallHook() )
	{
		DWORD err = ::GetLastError();
		DebugOut( _T("[FF] UninstallHook() failed (%d)\n"), err );
		if( ! isShutDown )
			return err;
	}

	::PulseEvent( g_hEventTerminate );

	return NO_ERROR;
}

//---------------------------------------------------------------------------
// handler that receives events from the service manager

VOID WINAPI MyServiceCtrlHandler( DWORD opcode ) 
{ 
	switch( opcode ) 
	{ 
		case SERVICE_CONTROL_STOP: 
		{
			DebugOut( _T("[FF] SERVICE_CONTROL_STOP received\n") );

			SetMyServiceStatus( SERVICE_STOP_PENDING );

			if( MyServiceUninitialization( false ) != NO_ERROR )
			{
				SetMyServiceStatus( SERVICE_RUNNING );
				return;
			}
			SetMyServiceStatus( SERVICE_STOPPED );
			return; 
		}

		case SERVICE_CONTROL_SHUTDOWN:
		{
			// TODO: on error write to system log
			MyServiceUninitialization( true );
			break;
		}

		case SERVICE_CONTROL_INTERROGATE: 
		{
			DebugOut( _T("[FF] SERVICE_CONTROL_INTERROGATE received\n") );

			// Send current status to service manager. 
			SetMyServiceStatus( 0xFFFFFFFF );
			return; 
		}

		default: 
			DebugOut( _T("[FF] Unrecognized opcode received (%d)\n"), opcode ); 
	} 
}

//---------------------------------------------------------------------------

DWORD MyServiceInitialization( DWORD argc, LPTSTR *argv ) 
{ 
	if( ! InstallHook() )
	{
		DWORD err = ::GetLastError();
		DebugOut( _T("[FF] InstallHook() failed (%d)\n"), err );
		return err;
	}	
	return NO_ERROR; 
}

//---------------------------------------------------------------------------

void WINAPI MyServiceStart( DWORD argc, LPTSTR *argv ) 
{ 
    DebugOut( _T("[FF] MyServiceStart()\n") ); 

    DWORD status = 0; 
 
    g_myServiceStatus.dwServiceType        = SERVICE_WIN32; 
    g_myServiceStatus.dwCurrentState       = SERVICE_START_PENDING; 
    g_myServiceStatus.dwControlsAccepted   = SERVICE_ACCEPT_STOP; //| SERVICE_ACCEPT_PAUSE_CONTINUE; 
    g_myServiceStatus.dwWin32ExitCode      = 0; 
    g_myServiceStatus.dwServiceSpecificExitCode = 0; 
    g_myServiceStatus.dwCheckPoint         = 0; 
    g_myServiceStatus.dwWaitHint           = 0; 
 
	g_myServiceStatusHandle = ::RegisterServiceCtrlHandler( 
        INTERNAL_APPNAME, MyServiceCtrlHandler ); 
 
    if( g_myServiceStatusHandle == (SERVICE_STATUS_HANDLE)0 ) 
    { 
		DebugOut( _T("[FF] RegisterServiceCtrlHandler() failed (%d)\n"), ::GetLastError() ); 
        return; 
    } 
 
	// Create event for termination 
	g_hEventTerminate = ::CreateEvent( NULL, FALSE, FALSE, NULL );

	// initialize service
	status = MyServiceInitialization( argc, argv );  
	if( status != NO_ERROR ) 
    { 
		SetMyServiceStatus( SERVICE_STOPPED, status );
        return; 
    } 
 
    // Initialization complete - report running status. 
	SetMyServiceStatus( SERVICE_RUNNING, status );

	DebugOut( _T("[FF] Service running, waiting for termination event\n") );

	// SetWindowsHookEx() seems to attach to the caller thread. So keep this thread running.
	::WaitForSingleObject( g_hEventTerminate, INFINITE );

	::CloseHandle( g_hEventTerminate );

    return; 
} 

//---------------------------------------------------------------------------

int WINAPI _tWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow )         
{ 
	DebugOut( _T("[FF] Process started\n") );

	SERVICE_TABLE_ENTRY dispatchTable[] = 
	{ 
		{ INTERNAL_APPNAME, MyServiceStart }, 
		{ NULL, NULL } 
	}; 

	int res = 0;
	if( ! ::StartServiceCtrlDispatcher( dispatchTable ) ) 
	{ 
		DWORD err = ::GetLastError();
		DebugOut( _T("[FF] StartServiceCtrlDispatcher() error (%d)\n"), err );

		if( err == ERROR_SERVICE_ALREADY_RUNNING )	
			DebugOut( _T("[FF] (Service already running)\n") );
		else
			res = 1;
	} 

	DebugOut( _T("[FF] Process terminates\n") );

	return res;
} 
