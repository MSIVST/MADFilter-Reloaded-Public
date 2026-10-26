/ MADFilterProperties.cpp
//

/* 
 * MADFilter - DirectShow filter for the MAD library
 * http://sourceforge.net/projects/maddxshow
 * 
 * Created by Armin Gerritsen - Copyright (c) April 2002
 * Contact: Armin.Gerritsen@@hetnet.nl
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA
 * http://www.gnu.org/copyleft/gpl.html
 *
 */


#include <streams.h>

#include "./res/resource.h"
#include "./res/version.h"
#include "./mad/mad.h"
#include "MADFilterProperties.h"


/*
 * (AAG:) This class doesn't do anything yet besides showing an 'About',
 *        but I created it to be able to show how a property page should
 *        be made and to make future work easier.
 */


//
// CreateInstance
//
CUnknown * WINAPI CMADFilterProperties::CreateInstance( LPUNKNOWN lpunk, HRESULT *phr )
{
 CUnknown *punk = new CMADFilterProperties( lpunk, phr ) ;
 if ( NULL == punk )
 {
  *phr = E_OUTOFMEMORY ;
 }
 
 return punk ;
}



//
//	Constructor
//
CMADFilterProperties::CMADFilterProperties( LPUNKNOWN lpunk, HRESULT *phr )
 : CBasePropertyPage( NAME("MAD Transform Filter Property Page"), lpunk,
                      IDD_PROPPAGE, IDS_FILTERNAME )
{
 m_pMADFilterInterface = NULL ;
}


//
//	Deconstructor
//
CMADFilterProperties::~CMADFilterProperties()
{
}


//
// Version Numbers, Date, Time
//
HRESULT CMADFilterProperties::OnActivate()
{		
	SetDlgItemText(m_hwnd, IDC_STATIC_MADFILTER, 
		"Version " MADFILTER_VERSION " - ("__DATE__", "__TIME__")");
	SetDlgItemText(m_hwnd, IDC_STATIC_LIBMAD, 
		"Version " MAD_VERSION " - (Sep  1 2003)");
	return S_OK;
}


//
// Called when a message is sent to the property page dialog box.
//
BOOL CMADFilterProperties::OnReceiveMessage( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
 // Does nothing yet, but I created it to make future work easier.
 //
/*
 switch ( uMsg )
 {
  default:
  {
  }
 }
*/
 return CBasePropertyPage::OnReceiveMessage( hwnd, uMsg, wParam, lParam ) ;
}


//
// Called when the property page connects to a filter.
//
HRESULT CMADFilterProperties::OnConnect( IUnknown *pUnknown )
{
 // Does nothing yet, but I created it to make future work easier.
 //
 ASSERT( NULL == m_pMADFilterInterface ) ;
 
 // Get a interface reference to our filter.
 HRESULT hr = pUnknown->QueryInterface( IID_IMADFilterInterface, (void**)&m_pMADFilterInterface ) ;
 if ( FAILED(hr) )
 {
  return E_NOINTERFACE ;
 }
 
 ASSERT( m_pMADFilterInterface ) ;
 
 return CBasePropertyPage::OnConnect( pUnknown ) ;
}


//
// Called when the property page is disconnected from the owning filter.
//
HRESULT CMADFilterProperties::OnDisconnect()
{
 // Does nothing yet, but I created it to make future work easier.
 //
 if ( NULL == m_pMADFilterInterface )
 {
  return E_UNEXPECTED ;
 }
 
 // Release the interface
 m_pMADFilterInterface->Release() ;
 m_pMADFilterInterface = NULL ;
 
 return CBasePropertyPage::OnDisconnect() ;
}


//
// Keep the changes we made.
//
HRESULT CMADFilterProperties::OnApplyChanges()
{
 // Does nothing yet, but I created it to make future work easier.
 //
 return CBasePropertyPage::OnApplyChanges() ;
}