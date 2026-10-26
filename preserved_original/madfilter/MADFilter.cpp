/ MADFilter.cpp
//
// The main implementation
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
#include <stdio.h>
#include <mmreg.h>

#include <initguid.h>
#include "MADFilter_CLSIDs.h"

#include "MADFilter.h"
#include "MADFilterProperties.h"

#include "aag_memcpy.h"
#include "maddecode.h"



#define AAG_MAXIMUM_COMPRESSIONFACTOR        24  // 64kbps MP3 = 8kBps : 8 * 24 = 192 kB/s > PCM_kB/s
#define AAG_DEFAULT_INPUTSIZE              8000  // We are screwed, if it is much larger.
//#define MAD_DIRECTSHOW_BUFFERSIZE    (32*1024) // 32kB
#define MAD_DIRECTSHOW_BUFFERSIZE      (AAG_MAXIMUM_COMPRESSIONFACTOR*AAG_DEFAULT_INPUTSIZE)

DEFINE_GUID(MEDIASUBTYPE_MP3,
			WAVE_FORMAT_MPEGLAYER3, 0x000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
/*
 Begin of CMADFilter implementation.
*/


//
// CreateInstance
//
CUnknown * WINAPI CMADFilter::CreateInstance( LPUNKNOWN lpunk, HRESULT *phr )
{		  
 CUnknown *punk = new CMADFilter( lpunk, phr ) ;
 if ( NULL == punk )
 {
  *phr = E_OUTOFMEMORY ;
 }
 
 return punk ;
}


//
// Constructor
//
CMADFilter::CMADFilter( LPUNKNOWN lpunk, HRESULT *phr )
  : CTransformFilter( NAME("MAD Transform Filter"), lpunk, CLSID_MADFilter )
{
 aag_memcopy_enable() ; // Enable SIMD usage
}



//
// NonDelegatingQueryInterface
//
// Override this to say what interfaces we support where
//
STDMETHODIMP CMADFilter::NonDelegatingQueryInterface( REFIID riid, void **ppv )
{
 CAutoLock cAutoLock( &m_cStateLock ) ;
 CheckPointer( ppv, E_POINTER ) ;
 //MessageBox( NULL, "CMADFilter::NonDelegatingQueryInterface( ... )" ,"Called", 0 ) ;
 
 if ( riid == IID_IMADFilterInterface )
 {
  return GetInterface( (IMADFilterInterface*)this, ppv ) ;
 }
 
 if ( riid == IID_ISpecifyPropertyPages )
 {
  return GetInterface( (ISpecifyPropertyPages*)this, ppv ) ;
 }
 
 return CTransformFilter::NonDelegatingQueryInterface( riid, ppv ) ;
}



//
// CheckMediaType
//
// Determines if we accept a specific media type as input.
//
HRESULT CMADFilter::CheckInputType( const CMediaType *pMediaType )
{
 CAutoLock cAutoLock( &m_cStateLock ) ;
 ASSERT( pMediaType ) ;
 //MessageBox( NULL, "CMADFilter::CheckInputType( ... )" ,"Called", 0 ) ;
 
 // Check type
 if ( ( *( pMediaType->Type() ) != MEDIATYPE_Audio  ) &&
      ( *( pMediaType->Type() ) != MEDIATYPE_Stream )  )
 {
  return E_INVALIDARG ;
 }
 
 // Check subtype
 if ( *pMediaType->Subtype() == MEDIASUBTYPE_MPEG1Audio ||
      *pMediaType->Subtype() == MEDIASUBTYPE_MPEG1AudioPayload ||
      *pMediaType->Subtype() == MEDIASUBTYPE_NULL ||
      *pMediaType->Subtype() == MEDIASUBTYPE_MP3)
 {
  return S_OK ;  // This format is acceptable.
 }
 
 return E_INVALIDARG ;
}



//
// GetMediaType
//
// Defines the type for the output-pin: PCM CD quality audio
//
HRESULT CMADFilter::GetMediaType( int iPos, CMediaType *pmt )
{
 CAutoLock cAutoLock( &m_cStateLock ) ;
 ASSERT( pmt ) ;
 //MessageBox( NULL, "CMADFilter::GetmediaType( ... )" , "Called", 0 ) ;
 
	if (!m_pInput->IsConnected())
	{
		return E_UNEXPECTED;
	}

 // Check if we get a request for a valid pin
 if ( iPos < 0 )
  return E_INVALIDARG;
    if ( iPos > 0 )
  return VFW_S_NO_MORE_ITEMS ;
 
 // Set type and subtype
 pmt->InitMediaType() ;
 pmt->SetType( & MEDIATYPE_Audio ) ;
 pmt->SetSubtype( & MEDIASUBTYPE_PCM ) ;
 pmt->SetTemporalCompression( FALSE ) ;
 
 // Initialise - prepare for PCM CD-quality
 unsigned int  block_al, bitdepth, samples_ps, nchannels ;
 unsigned long bytes_ps ;
 samples_ps = 44100 ;
 bytes_ps   = 176400 ;
 bitdepth   = 16 ;
 nchannels  = 2 ;
	
 CMediaType& InMt = m_pInput->CurrentMediaType();	
 if (*InMt.FormatType() == FORMAT_WaveFormatEx)
 {
   WAVEFORMATEX *wfex = (WAVEFORMATEX *)InMt.Format();
   samples_ps = wfex->nSamplesPerSec;		
 // Channelnumber and BitsPerSample can't be taken from WAVEFORMATEX because Madfilter always decodes to 2 channels and 16bit (maddecode.ccp)
 //if (wfex->wBitsPerSample != 0)
 //  bitdepth = wfex->wBitsPerSample;
 //if (wfex->nChannels != 0)
 //  nchannels = wfex->nChannels;		
 }
	
 bytes_ps = samples_ps * (bitdepth / 8) * nchannels;
 block_al   = nchannels * ( (bitdepth + 7) / 8 ) ;
 
 // Set WAVEFORMAT struct
 WAVEFORMATEX *pmf = (WAVEFORMATEX*)pmt->AllocFormatBuffer( sizeof(WAVEFORMATEX) ) ;
 pmf->cbSize            = 16 ;
 pmf->nAvgBytesPerSec   = bytes_ps ;
 pmf->nBlockAlign       = block_al ;
 pmf->nChannels         = nchannels ;
 pmf->nSamplesPerSec    = samples_ps ;
 pmf->wBitsPerSample    = bitdepth ;
 pmf->wFormatTag        = WAVE_FORMAT_PCM ;
 
 // Now set the prepared format-struct
 pmt->SetFormatType( & FORMAT_WaveFormatEx ) ;
 
 return S_OK ;
}



//
// Check if input and output media types are compatible.
//
HRESULT CMADFilter::CheckTransform( const CMediaType *pmtIn, const CMediaType *pmtOut )
{
 CAutoLock cAutoLock( &m_cStateLock ) ;
 ASSERT( pmtIn ) ;
 ASSERT( pmtOut ) ;
 //MessageBox( NULL, "CMADFilter::CheckTransform( ... )" , "Called", 0 ) ;
 
 // There is no relationship (yet) between in- and output.
 // We could however, for instance, think of things like
 // automagically change output sample-rate depending on
 // the MP3 input quality.
 return S_OK ;
}



//
// DecideBufferSize
//
// This will always be called after the format has been sucessfully
// negotiated. Then we can ask for buffers of the correct size to
// contain the samples.
//
HRESULT CMADFilter::DecideBufferSize( IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pProperties )
{
 CAutoLock cAutoLock( &m_cStateLock ) ;
 ASSERT( pAlloc ) ;
 ASSERT( pProperties ) ;
 //MessageBox( NULL, "CMADFilter::DecideBufferSize( ... )" ,"Called", 0 ) ;
 
    pProperties->cBuffers = 10 ;
 pProperties->cbBuffer = MAD_DIRECTSHOW_BUFFERSIZE ;
 pProperties->cbAlign  = 1 ;
 pProperties->cbPrefix = 0 ;
 
 // Ask the allocator to reserve us some sample memory.
 // NOTE: the function can succeed (that is return NOERROR)
 //       but still not have allocated the memory that we
 //       requested, so we must check we got whatever we wanted.
 
 ALLOCATOR_PROPERTIES Actual ;
 HRESULT hr = pAlloc->SetProperties( pProperties, &Actual ) ;
 if ( FAILED(hr) )
 {
  return hr ;
 }
 
 // So now check if this allocator is suitable
 if ( Actual.cbBuffer < pProperties->cbBuffer )
 {
  return E_FAIL ;
 }
 
 // Make sure that we have at least 1 buffer
 ASSERT( Actual.cBuffers > 0 ) ;
 
 return S_OK ;
}



//
// Note: In DirectShow 'Pause' is not what you might expect:
//       This is what can happen: Stop -> Pause -> Run
//                                Run  -> Pause (-> Stop)
//
STDMETHODIMP CMADFilter::Pause()
{
 CAutoLock cAutoLock( &m_cStateLock ) ;
 //MessageBox( NULL, "CMADFilter::Pause()" ,"Called", 0 ) ;
 
 return CTransformFilter::Pause() ;
}



//
// See comments with 'Pause'
//
HRESULT CMADFilter::Stop()
{
 CAutoLock cAutoLock( &m_cStateLock ) ;
 //MessageBox( NULL, "CMADFilter::Stop()" ,"Called", 0 ) ;
 
 return CTransformFilter::Stop() ;
}


//
// Transform
//
HRESULT CMADFilter::Transform( IMediaSample *pIn, IMediaSample *pOut )
{
 CAutoLock cAutoLock( &m_cStateLock ) ;
 //MessageBox( NULL, "CMADFilter::Transform( ... )" ,"Called", 0 ) ;
 
 // Get input information
 BYTE         *pInData  = NULL ;
 unsigned int uiInSize  = 0 ;
 pIn->GetPointer( &pInData ) ;
 uiInSize = pIn->GetSize() ;
 
 // Get output information
 BYTE         *pOutData = NULL ;
 unsigned int uiOutSize = 0 ;
 pOut->GetPointer( &pOutData ) ;
 uiOutSize = pOut->GetSize() ;
 
 //char bufje[256] ;
 //sprintf( bufje, "In=%d\nOut=%d", uiInSize, uiOutSize ) ;
 //MessageBox( NULL, bufje, "Buffer Sizes", 0 ) ;
 
 // Decode input-data
 unsigned char *buf   = NULL ;
 unsigned int bufsize = 0 ;
 if ( DecodeBuffer( pInData, uiInSize, &buf, &bufsize ) )
 {
  if ( uiOutSize < bufsize ) // Does the decoded data fit?
  {
   return E_UNEXPECTED ;
  }
  
  // Copy data from 'buf' to 'pOutData'.
  // Note: bufsize <= lDataLen
  aag_memcpy( pOutData, buf, bufsize ) ;
  pOut->SetActualDataLength( bufsize ) ;
  
  // Note: don't delete the 'buf' buffer!
  return S_OK ;
 }
 
 // Decode failed => Panic!?
 return E_UNEXPECTED ;
}




//
// This handles decoding and also handles 'remains': Not every buffer
// will contain a full number of frames and can therefore be decoded
// completely.
// 
BOOL CMADFilter::DecodeBuffer( unsigned char *inbuf,   size_t inbufsize,
                               unsigned char **outbuf, size_t *outbufsize )
{
 // Statics
 static unsigned char *decodedbuf  = new unsigned char[MAD_DIRECTSHOW_BUFFERSIZE] ;
 static unsigned char *encodedbuf  = new unsigned char[4*inbufsize] ; // (*)
 static unsigned char *leftoverbuf = new unsigned char[4*inbufsize] ; // (*)
 static unsigned int  BytesLeft    = 0 ;
 // (*): So, the first call decides the buffersize. Must be factor of 4 !!!
 
 if ( decodedbuf==NULL || encodedbuf==NULL || leftoverbuf==NULL )
 {
  //Fatal error: Could not create decoding-buffers.
  return FALSE ;
 }
 
 // Normal variables
 unsigned int  BytesUnused  = 0 ;
 unsigned int  InBufSize    = 0 ;
 int           iReturn      = 0 ;
 
 // Check for leftovers from previous run
 if ( BytesLeft )
 {
  // Get sizes
  InBufSize  = inbufsize + BytesLeft ;
  
  // Create source and copy the data to it
  aag_memcpy( encodedbuf, leftoverbuf, BytesLeft ) ;
  aag_memcpy( encodedbuf+BytesLeft, inbuf, inbufsize ) ;
 }
 else
 {
  InBufSize = inbufsize ;
  aag_memcpy( encodedbuf, inbuf, inbufsize ) ;
 }
 
 // Decode the buffer
 iReturn = MAD_MPEGAudioDecoder( encodedbuf, InBufSize,
                                 decodedbuf, MAD_DIRECTSHOW_BUFFERSIZE,
                                 &BytesLeft, &BytesUnused ) ;
 
 // Process results
 switch ( iReturn )
 {
  case 0 :  //All OK
           break ;
           
  case 1 : // Check for leftovers and save them
           //if ( BytesLeft )
           {
            aag_memcpy( leftoverbuf, encodedbuf+InBufSize-BytesLeft, BytesLeft ) ;
           }
           break ;
           
  case 2 : // Output buffer was too small
           //
           // First check if we decoded anything ...
           if ( ! (BytesUnused<MAD_DIRECTSHOW_BUFFERSIZE) ) // No we didn't ... ;-(
           {
            // Note: We cannot decode the input-buffer with a output-buffer
            //       of the given size.
            // For now, just skip data: (Yes, this means 'toDo' :-) )
            BytesLeft = 0 ;
            break ;
           }
           
           // Anything left?
           if ( BytesLeft )
           {
            aag_memcpy( leftoverbuf, encodedbuf+InBufSize-BytesLeft, BytesLeft ) ;
           }
           break ;
           
  case 3 : // Error: TODO?
           // For now, just skip bad data
           BytesLeft = 0 ;
           break ;
 }
 
 // Assign return value's
 *outbuf     = decodedbuf ;
 *outbufsize = (size_t)(MAD_DIRECTSHOW_BUFFERSIZE-BytesUnused) ;
 
 //Note: don't delete the buffers! We reuse them.
 return TRUE ;
}



/*
 Begin of basic COM stuf ...
*/


const AMOVIESETUP_MEDIATYPE sudIpPinTypes[] =
{
 {
  & MEDIATYPE_Audio,
  & MEDIASUBTYPE_MP3
 },
 {
  & MEDIATYPE_Audio,
  & MEDIASUBTYPE_MPEG1Audio
 },
 {
  & MEDIATYPE_Audio,
  & MEDIASUBTYPE_MPEG1AudioPayload
 },
 {
  & MEDIATYPE_Stream,
  & MEDIASUBTYPE_MPEG1Audio
 },
 {
  & MEDIATYPE_Stream,
  & MEDIASUBTYPE_MPEG1AudioPayload
 },
};


const AMOVIESETUP_MEDIATYPE sudOpPinTypes[] =
{
  & MEDIATYPE_Audio,            // MajorType
  & MEDIASUBTYPE_PCM            // MinorType
};


const AMOVIESETUP_PIN psudPins[] =
{
   {
    L"Input",                  // The Pins name
    FALSE,                     // Is rendered
    FALSE,                     // Is an output pin
    FALSE,                     // Allowed none
    FALSE,                     // Allowed many
    &CLSID_NULL,               // Connects to filter
    NULL,                      // Connects to pin
    4,                         // Number of types
    &sudIpPinTypes[0]		   // Pin details
   },
   {
    L"Output",                 // The Pins name
	FALSE,                     // Is rendered
	TRUE,                      // Is an output pin
	FALSE,                     // Allowed none
	FALSE,                     // Allowed many
	&CLSID_NULL,               // Connects to filter
	NULL,                      // Connects to pin
	1,                         // Number of types
	&sudOpPinTypes[0]          // Pin details
   },
};


const AMOVIESETUP_FILTER sudMADtransAx =
{
   	&CLSID_MADFilter,    		// Filter CLSID
	L"MAD Transform Filter", 	// String name
	// we add 0x2E90000 to have a higher MERIT than the default Windows filter
    MERIT_PREFERRED+0x2E90000,  // Filter merit
	2,                          // Number of pins
	psudPins                    // Pin details
};


// List of class IDs and creator functions for the class factory. This
// provides the link between the OLE entry point in the DLL and an object
// being created. The class factory will call the static CreateInstance
// function when it is asked to create a CLSID_XXX object
//
CFactoryTemplate g_Templates[] =
{
    { L"MADFilter"
	, &CLSID_MADFilter
	, CMADFilter::CreateInstance
	, NULL
	, &sudMADtransAx
    },
   	{ L"MADFilter PropertyPage"
    , &CLSID_MADFilterProperties
    , CMADFilterProperties::CreateInstance
	}

};
int g_cTemplates = sizeof(g_Templates) / sizeof(g_Templates[0]) ;


//
// DllRegisterServer
//
// Used to register and unregister the filter
//
STDAPI DllRegisterServer()
{
 return AMovieDllRegisterServer2( TRUE ) ;	
}


//
// DllUnregisterServer
//
STDAPI DllUnregisterServer()
{
 return AMovieDllRegisterServer2( FALSE ) ;
}


//
// DllMain
//
// Very important and often forgotten!
//
extern "C" BOOL WINAPI DllEntryPoint( HINSTANCE, ULONG, LPVOID ) ;
BOOL WINAPI DllMain( HANDLE hDllHandle, DWORD dwReason, LPVOID lpReserved )
{
 // If we don't do this, this DLL will register itself by the wrong name
 // since the global instance handle is not set.
 // (Generally it will point to regsvr32.)
 return DllEntryPoint( reinterpret_cast<HINSTANCE>(hDllHandle), 
                       dwReason, lpReserved ) ;
}