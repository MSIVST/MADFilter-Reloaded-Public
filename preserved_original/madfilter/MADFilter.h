/ MADFilter.h
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


#ifndef _MADFILTER_H
#define _MADFILTER_H


#include "MADFilterInterface.h"


class CMADFilter : public CTransformFilter, ISpecifyPropertyPages, IMADFilterInterface
{
public:
    static CUnknown * WINAPI CreateInstance( LPUNKNOWN lpunk, HRESULT *phr ) ;
    
    // Overriden to say what interfaces we support where.
    STDMETHODIMP NonDelegatingQueryInterface( REFIID riid, void **ppv ) ;
    
    // Overridden to handle filter stop and pausing.
    STDMETHODIMP Pause() ;
    STDMETHODIMP Stop() ;
    
    // Transform override function - This is where the action is.
    HRESULT Transform( IMediaSample *pIn, IMediaSample *pOut ) ;
    
    // Input pin overrride functions (Pure virtual method implementation)
    HRESULT CheckInputType( const CMediaType *pmtIn ) ;
    HRESULT CheckTransform( const CMediaType *mptIn, const CMediaType *pmtOut ) ;
    
    // Output pin override functions (Pure virtual method implementation)
    HRESULT GetMediaType( int iPos, CMediaType *pmt ) ;
    HRESULT DecideBufferSize( IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pProperties ) ;
    
    // Implementation for  ISpecifyPropertyPages
    DECLARE_IUNKNOWN ;
    STDMETHODIMP GetPages( CAUUID *pPages ) ;
    
    // Implementation for  ISpecifyPropertyPages
    //DECLARE_IUNKNOWN ;
    STDMETHODIMP GetMADVersion( int *pVersion ) ;
    
private:
    // It is only allowed to to create these objects with CreateInstance.
    CMADFilter( LPUNKNOWN lpunk, HRESULT *phr) ;
    
    // Actual MAD wrapper
    BOOL CMADFilter::DecodeBuffer( unsigned char *inbuf,   size_t inbufsize,
                                   unsigned char **outbuf, size_t *outbufsize ) ;
    
private:
    CCritSec m_cStateLock ;
};

#endif //_MADFILTER_H