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
 */
#include "stdafx.h"

#include "fflib.h"
#include "cmnOpenWithDlgHook.h"

//-----------------------------------------------------------------------------------------

namespace {
	const TCHAR FLASHFOLDER_HOOK_PROPERTY[] = _T("FlashFolderHook.k5evf782uxt56a8zp29");
	const UINT IDC_SIZEGRIP = 0x5555;
};

// for easy access from the subclassed WindowProc
CmnOpenWithDlgHook* g_pHook = NULL;

//-----------------------------------------------------------------------------------------

bool CmnOpenWithDlgHook::Init( HWND hwndFileDlg, FileDlgHookCallback_base* pCallbacks )
{
	if( m_hwndDlg ) return false;  // only init once!

	::OutputDebugString( _T("[fflib] CmnOpenWithDlgHook::Init()\n") );

	g_pHook = this;

	m_hwndDlg = hwndFileDlg;

	// Subclass the window proc of the file dialog.
	m_oldWndProc = reinterpret_cast<WNDPROC>( 
		::SetWindowLongPtr( hwndFileDlg, GWL_WNDPROC, 
		                    reinterpret_cast<LONG_PTR>( HookWindowProc) ) );
	_ASSERTE( m_oldWndProc );

	// Associate the this-pointer with the hooked window so we can access
	// "this" instance in the static HookWindowProc without the need of global variables.
	::SetProp( hwndFileDlg, FLASHFOLDER_HOOK_PROPERTY, 
	           reinterpret_cast<HANDLE>( this ) );

	//--- read settings from INI file ---
	m_minFileDialogWidth = g_profile.GetInt( _T("CommonOpenWithDlg"), _T("MinWidth"), 650 );
	m_minFileDialogHeight = g_profile.GetInt( _T("CommonOpenWithDlg"), _T("MinHeight"), 500 );
	m_centerFileDialog = g_profile.GetInt( _T("CommonOpenWithDlg"), _T("Center"), 1 ) != 0;
    
	// modify window style to make it resizable
	::SetWindowLong( m_hwndDlg, GWL_STYLE, 
		::GetWindowLong( m_hwndDlg, GWL_STYLE ) | WS_SIZEBOX | WS_SYSMENU | WS_CLIPCHILDREN );
	HMENU hSysMenu = ::GetSystemMenu( m_hwndDlg, FALSE );
	::AppendMenu( hSysMenu, MF_STRING, SC_SIZE, _T("Change size") );

	RECT rcClient;
	::GetClientRect( m_hwndDlg, &rcClient );                                

	// add "size grip" control
	m_hwndSizeGrip = ::CreateWindow( _T("ScrollBar"), _T(""), 
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | SBS_SIZEGRIP | SBS_SIZEBOXBOTTOMRIGHTALIGN, 0, 0, 
			rcClient.right, rcClient.bottom, m_hwndDlg, reinterpret_cast<HMENU>( IDC_SIZEGRIP ), NULL, NULL );

	return true;
}

//-----------------------------------------------------------------------------------------

// unused methods

bool CmnOpenWithDlgHook::SetFolder( LPCTSTR path )
{
	return true;
}

bool CmnOpenWithDlgHook::GetFolder( LPTSTR folderPath )
{
	return true;
}

bool CmnOpenWithDlgHook::SetFilter( LPCTSTR filter )
{
	return true;
}

//-----------------------------------------------------------------------------------------

LRESULT CALLBACK CmnOpenWithDlgHook::HookWindowProc(
	HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch( uMsg )
	{
		case WM_PAINT:
		{
			//hooking WM_PAINT may seem curious here but we need a message that is send
			// AFTER the file dialogs controls are initialised
			if( ! g_pHook->m_initDone )
            {
				g_pHook->m_initDone = true;

				::GetWindowRect( hwnd, &g_pHook->m_rcInitial );

				//customize file dialog's initial size + position
				g_pHook->ResizeDialog();
            }
		}
		break;

		case WM_ACTIVATE:
			g_pHook->m_isWindowActive = LOWORD(wParam) != 0;
		break;

		case WM_SIZE:
		{
			WORD cx = static_cast<WORD>( lParam & 0xFFFF );
			WORD cy = static_cast<WORD>( ( lParam >> 16 ) & 0xFFFF );
			HWND hwndList = ::GetDlgItem( g_pHook->m_hwndDlg, 0x3605 );
			HWND hwndChk = ::GetDlgItem( g_pHook->m_hwndDlg, 0x3509 );
			HWND hwndOK = ::GetDlgItem( g_pHook->m_hwndDlg, IDOK );
			HWND hwndCancel = ::GetDlgItem( g_pHook->m_hwndDlg, IDCANCEL );
			HWND hwndOther = ::GetDlgItem( g_pHook->m_hwndDlg, 0x3507 );
			HWND hwndGrip = ::GetDlgItem( g_pHook->m_hwndDlg, IDC_SIZEGRIP );
		
			HDWP hdwp = ::BeginDeferWindowPos( 6 );
			
			hdwp = ::DeferWindowPos( hdwp, hwndList, NULL, 12, 99, cx - 24, cy - 170, SWP_NOZORDER );
			hdwp = ::DeferWindowPos( hdwp, hwndChk, NULL, 12, cy - 58, 0, 0, SWP_NOZORDER | SWP_NOSIZE );
			hdwp = ::DeferWindowPos( hdwp, hwndOK, NULL, 54, cy - 32, 0, 0, SWP_NOZORDER | SWP_NOSIZE );
			hdwp = ::DeferWindowPos( hdwp, hwndCancel, NULL, 135, cy - 32, 0, 0, SWP_NOZORDER | SWP_NOSIZE );
			hdwp = ::DeferWindowPos( hdwp, hwndOther, NULL, 216, cy - 32, 0, 0, SWP_NOZORDER | SWP_NOSIZE );
			RECT rcGrip; ::GetClientRect( hwndGrip, &rcGrip );
			hdwp = ::DeferWindowPos( hdwp, hwndGrip, NULL, cx - rcGrip.right, cy - rcGrip.bottom, 0, 0, SWP_NOZORDER | SWP_NOSIZE );

			::EndDeferWindowPos( hdwp );
		}
		break;

		case WM_GETMINMAXINFO:
		{
			MINMAXINFO* pmi = reinterpret_cast<MINMAXINFO*>( lParam );
			pmi->ptMinTrackSize.x = 
				g_pHook->m_rcInitial.right - g_pHook->m_rcInitial.left;
			pmi->ptMinTrackSize.y = 
				g_pHook->m_rcInitial.bottom - g_pHook->m_rcInitial.top;
		}
		break;

		case WM_NCDESTROY:
		{
			// Remove our window property from the file dialog.
			// We stored it there in CmnOpenWithDlgHook::Init().
			::RemoveProp( hwnd, FLASHFOLDER_HOOK_PROPERTY );     
		}
		break;
    }

    //call original message handler
    return CallWindowProc( g_pHook->m_oldWndProc, hwnd, uMsg, wParam, lParam);
}

//-----------------------------------------------------------------------------------------

void CmnOpenWithDlgHook::ResizeDialog()
{
	//---- customize file dialog size + position -----
	//when centering the file dialog, take care that it is centered on the correct monitor

	RECT rcMonitor = { 0 };
	if (m_centerFileDialog)
	{
		HWND wnd = GetParent(m_hwndDlg);
		if (wnd == NULL) wnd = m_hwndDlg;
		GetMaximizedRect( wnd, rcMonitor );
	}

	//get original file dialog size
	RECT rc;
	GetClientRect(m_hwndDlg, &rc);

	//get new dimensions only if bigger than original size
	int newWidth = rc.right < m_minFileDialogWidth ? m_minFileDialogWidth : rc.right;
	int newHeight = rc.bottom < m_minFileDialogHeight ? m_minFileDialogHeight : rc.bottom;

	//--- make the dialog bigger + center it ---
	if (m_centerFileDialog)
		SetWindowPos(m_hwndDlg, NULL, 
			rcMonitor.left + (rcMonitor.right - rcMonitor.left - newWidth) / 2, 
			rcMonitor.top  + (rcMonitor.bottom - rcMonitor.top - newHeight) / 2, 
			newWidth, newHeight, 
			SWP_NOZORDER | SWP_NOACTIVATE);
	else
		SetWindowPos(m_hwndDlg, NULL, 
			0, 0, newWidth, newHeight, 
			SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}
