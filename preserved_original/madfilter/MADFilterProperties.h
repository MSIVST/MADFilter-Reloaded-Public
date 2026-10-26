/ MADFilterProperties.h
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


#ifndef _MADFILTERPROPERTIES_H
#define _MADFILTERPROPERTIES_H


#include "MADFilter.h"


class CMADFilterProperties : public CBasePropertyPage
{
public:
    static CUnknown * WINAPI CreateInstance( LPUNKNOWN lpunk, HRESULT *phr ) ;
    
private:
    CMADFilterProperties( LPUNKNOWN lpunk, HRESULT *phr) ;
    ~CMADFilterProperties() ;
    
    // Oberridden base members
    HRESULT OnConnect( IUnknown *pUnknown ) ;
    HRESULT OnDisconnect() ;
    HRESULT OnApplyChanges() ;
    HRESULT OnActivate() ;
	BOOL    OnReceiveMessage( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam ) ;

    // Usefull helper
    inline const IMADFilterInterface* pMADFilter()
    {
     ASSERT( m_pMADFilterInterface ) ;
     return m_pMADFilterInterface ;
    };
    
private:
    IMADFilterInterface *m_pMADFilterInterface ;
};

#endif //_MADFILTERPROPERTIES_H