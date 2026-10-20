//
// 傊側偪傚偙僂僀儞僪僂僋儔僗
//
#include "DebugOut.h"
#include "Wnd.h"

// Instance
CWndList	WndList;

list<CWnd*>	CWndList::m_WndPtrList;

// 揇廘偄乧
CWndList::CWndList()
{
//	m_WndPtrList.clear();
}

CWndList::~CWndList()
{
//	if( !m_WndPtrList.empty() )
//		m_WndPtrList.clear();
}

void	CWndList::Add( CWnd* pWnd )
{
	m_WndPtrList.push_back( pWnd );
}

void	CWndList::Del( CWnd* pWnd )
{
	for( list<CWnd*>::iterator it=m_WndPtrList.begin(); it!=m_WndPtrList.end(); ) {
		if( *it == pWnd ) {
			m_WndPtrList.erase(it);
			break;
		} else {
			++it;
		}
	}
}

BOOL	CWndList::IsDialogMessage( LPMSG msg )
{
	if( m_WndPtrList.empty() )
		return	FALSE;

	list<CWnd*>::iterator it=m_WndPtrList.begin();
	while( it != m_WndPtrList.end() ) {
		if( ::IsDialogMessage( (*it)->m_hWnd, msg ) )
			return	TRUE;
		++it;
	}

	return	FALSE;
}

CWnd::CWnd()
{
	m_hWnd = NULL;
	m_hMenu = NULL;
}

CWnd::~CWnd()
{
}

void	CWnd::SetThis()
{
	// Dispatch弌棃傞傛偆偵CWnd*傪杽傔崬傓
	if( m_hWnd ) {
		::SetWindowLongPtr( m_hWnd, GWLP_USERDATA, (LONG_PTR)this );
	}
}

LRESULT	CALLBACK CWnd::g_WndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
	// 僂僀儞僪僂偑奐偔慜偵偙偭偦傝張棟偡傞
	if( msg == WM_CREATE ) {
		LPCREATESTRUCT	lpcs = (LPCREATESTRUCT)lParam;
		CWnd* pWnd = (CWnd*)::GetWindowLongPtr( hWnd, GWLP_USERDATA );
		if( !pWnd ) {
			// CWnd* this傪杽傔崬傓
			//::SetWindowLong( hWnd, GWLP_USERDATA, (LONG)lpcs->lpCreateParams );
			::SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)lpcs->lpCreateParams);
			// 帺暘偺僂僀儞僪僂僴儞僪儖
			pWnd = (CWnd*)lpcs->lpCreateParams;
			pWnd->m_hWnd = hWnd;
		}
	}
	// CWnd* this傪杽傔崬傫偱偁傞
	CWnd* pWnd = (CWnd*)::GetWindowLongPtr( hWnd, GWLP_USERDATA );

	if( pWnd ) {
		return	pWnd->DispatchWnd( hWnd, msg, wParam, lParam );
	} else {
		return	::DefWindowProc(  hWnd, msg, wParam, lParam );
	}
}

INT_PTR	CALLBACK CWnd::g_DlgProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
	// 僨傿僗僷僢僠慜偵偙偭偦傝張棟偡傞
	if( msg == WM_INITDIALOG ) {
		// Dispatch弌棃傞傛偆偵CWnd*傪杽傔崬傓(儌乕僟儖偼昁偢DialogBoxParam偱婲摦偡傞帠)
		// CWnd* this傪杽傔崬傫偱偁傞偑丆儌乕僟儖偱偼擖偭偰偄側偄
		CWnd* pWnd = (CWnd*)::GetWindowLongPtr( hWnd, GWLP_USERDATA );

		if( !pWnd ) {
			//::SetWindowLong( hWnd, GWLP_USERDATA, (LONG)lParam );
			::SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)lParam);
			pWnd = (CWnd*)lParam;
		}
		// 帺暘偺僂僀儞僪僂僴儞僪儖
		pWnd->m_hWnd = hWnd;

		// 僟僀傾儘僌傪拞墰偵堏摦偡傞:)
		HWND hWndParent = ::GetParent( hWnd );
		if( hWndParent ) {
			RECT	rcParent, rc;
			::GetWindowRect( hWndParent, &rcParent );
			::GetWindowRect( hWnd, &rc );
			INT x = rcParent.left+(rcParent.right-rcParent.left)/2-(rc.right-rc.left)/2;
			INT y = rcParent.top +(rcParent.bottom-rcParent.top)/2-(rc.bottom-rc.top)/2;
//			DEBUGOUT( "X=%d  Y=%d\n", x, y );
			::SetWindowPos( hWnd, NULL, x, y, -1, -1, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE );
		}
	}

	// CWnd* this傪杽傔崬傫偱偁傞
	//CWnd* pWnd = (CWnd*)::GetWindowLongPtr( hWnd, GWLP_USERDATA );
	CWnd* pWnd = (CWnd*)::GetWindowLongPtr(hWnd, GWLP_USERDATA);
	if( pWnd ) {
		return	pWnd->DispatchDlg( hWnd, msg, wParam, lParam );
	} else {
		return	FALSE;
	}
}

