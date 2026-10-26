/ ISpecifyPropertyPagesImpl.cpp
//
// Contains the implementation for ISpecifyPropertyPages
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

#include "MADFilter.h"
#include "MADFilter_CLSIDs.h"



HRESULT CMADFilter::GetPages( CAUUID *pPages )
{
 //MessageBox( NULL, "CMADFilter::GetPages( CAUUID *pPages )" ,"Called", 0 ) ;
 
 if ( NULL == pPages )
  return E_INVALIDARG ;
 
 pPages->cElems = 1 ; 
 pPages->pElems = (GUID*)CoTaskMemAlloc( sizeof(GUID) ) ;
 
 if ( !pPages->pElems )
 {
  pPages->cElems = 0 ;
  return E_OUTOFMEMORY ;
 }
 
 memcpy( pPages->pElems, &CLSID_MADFilterProperties, sizeof(GUID) ) ;
 
 return S_OK ;
}