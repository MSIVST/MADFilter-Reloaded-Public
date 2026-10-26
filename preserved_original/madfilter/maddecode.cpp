/ maddecode.cpp - 2002 - Armin Gerritsen - Armin.Gerritsen@@hetnet.nl
// 
// Based partly on  madlld.c by Bertrand Petit (c) 2001, 2002
// His copyrights on madlld.c:
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:								
//									
// 1. Redistributions of source code must retain the above copyright	
//    notice, this list of conditions and the following disclaimer.	
//									
// 2. Redistributions in binary form must reproduce the above		
//    copyright notice, this list of conditions and the following	
//    disclaimer in the documentation and/or other materials provided	
//    with the distribution.						
// 									
// 3. Neither the name of the author nor the names of its contributors	
//    may be used to endorse or promote products derived from this	
//    software without specific prior written permission.		
// 									
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS''	
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED	
// TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A	
// PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR	
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,		
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT	
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF	
// USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND	
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,	
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT	
// OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF	
// SUCH DAMAGE.
//


#include "./MAD/mad.h"
#include "maddecode.h"


// Internals
void           MAD_PrintFrameInfo( char **, struct mad_header * ) ;
const char*    MAD_ErrorString( const struct mad_stream * ) ;
unsigned short MAD_FixedToUshort( mad_fixed_t ) ;
signed int     MAD_SimpleScale( mad_fixed_t ) ;
signed long    MAD_Linear_Dither( unsigned int, mad_fixed_t, mad_fixed_t *,
                                  unsigned long *, mad_fixed_t * ) ;



// Globals
static unsigned int       g_MADInit = 0 ;
static struct mad_stream  g_Stream ;
static struct mad_frame	  g_Frame ;
static struct mad_synth	  g_Synth ;
static        mad_timer_t g_Timer ;


const char *MAD_ErrorString( const struct mad_stream *Stream )
{
	switch ( Stream->error )
	{
		/* Generic unrecoverable errors. */
		case MAD_ERROR_BUFLEN:
			return("input buffer too small (or EOF)");
		case MAD_ERROR_BUFPTR:
			return("invalid (null) buffer pointer");
		case MAD_ERROR_NOMEM:
			return("not enough memory");

		/* Frame header related unrecoverable errors. */
		case MAD_ERROR_LOSTSYNC:
			return("lost synchronization");
		case MAD_ERROR_BADLAYER:
			return("reserved header layer value");
		case MAD_ERROR_BADBITRATE:
			return("forbidden bitrate value");
		case MAD_ERROR_BADSAMPLERATE:
			return("reserved sample frequency value");
		case MAD_ERROR_BADEMPHASIS:
			return("reserved emphasis value");

		/* Recoverable errors */
		case MAD_ERROR_BADCRC:
			return("CRC check failed");
		case MAD_ERROR_BADBITALLOC:
			return("forbidden bit allocation value");
		case MAD_ERROR_BADSCALEFACTOR:
			return("bad scalefactor index");
		case MAD_ERROR_BADFRAMELEN:
			return("bad frame length");
		case MAD_ERROR_BADBIGVALUES:
			return("bad big_values count");
		case MAD_ERROR_BADBLOCKTYPE:
			return("reserved block_type");
		case MAD_ERROR_BADSCFSI:
			return("bad scalefactor selection info");
		case MAD_ERROR_BADDATAPTR:
			return("bad main_data_begin pointer");
		case MAD_ERROR_BADPART3LEN:
			return("bad audio data length");
		case MAD_ERROR_BADHUFFTABLE:
			return("bad Huffman table select");
		case MAD_ERROR_BADHUFFDATA:
			return("Huffman data overrun");
		case MAD_ERROR_BADSTEREO:
			return("incompatible block_type for JS");

		/* Unknown error. This swich may be out of sync with libmad's
		   defined error codes.
		*/
		default:
			return("Unknown error code");
	}
}


// Converts a sample from mad's fixed point number format to an
// unsigned short (16 bits).
//
// Note: Use 'MAD_SimpleScale' instead. This filter is purly for
//       testing: its sound-quality is bad.
//
unsigned short MAD_FixedToUshort( mad_fixed_t Fixed )
{
 /* A fixed point number is formed of the following bit pattern:
  *
  * SWWWFFFFFFFFFFFFFFFFFFFFFFFFFFFF
  * MSB                          LSB
  * S ==> Sign (0 is positive, 1 is negative)
  * W ==> Whole part bits
  * F ==> Fractional part bits
  *
  * This pattern contains MAD_F_FRACBITS fractional bits, one
  * should alway use this macro when working on the bits of a fixed
  * point number. It is not guaranteed to be constant over the
  * different platforms supported by libmad.
  *
  * The unsigned short value is formed by the least significant
  * whole part bit, followed by the 15 most significant fractional
  * part bits. Warning: this is a quick and dirty way to compute
  * the 16-bit number, madplay includes much better algorithms.
  */
 Fixed = Fixed >> (MAD_F_FRACBITS-15 ) ;
 return ( (unsigned short)Fixed ) ;
}



// The following utility routine performs simple rounding, clipping, and
// scaling of MAD's high-resolution samples down to 16 bits. It does not
// perform any dithering or noise shaping, which would be recommended to
// obtain any exceptional audio quality. It is therefore not recommended to
// use this routine if real high-quality output is desired.
//
signed int MAD_SimpleScale( mad_fixed_t sample )
{
 // Round
 sample += ( 1L << (MAD_F_FRACBITS - 16) ) ;
 
 // Clip
 if ( sample >= MAD_F_ONE )
  sample = MAD_F_ONE - 1 ;
 else
 if ( sample < -MAD_F_ONE )
  sample = -MAD_F_ONE ;
 
 // Quantise
 return sample >> (MAD_F_FRACBITS + 1 - 16) ;
}


// Print human-readable informations about an audio MPEG frame
void MAD_PrintFrameInfo( char **buf, struct mad_header *Header )
{
	const char *Layer, *Mode, *Emphasis ;
        
        if ( NULL!=buf && *buf==NULL )
        {
         (*buf) = new char[256] ;
        }
        
	/* Convert the layer number to it's printed representation. */
	switch( Header->layer )
	{
		case MAD_LAYER_I:
			Layer="I";
			break;
		case MAD_LAYER_II:
			Layer="II";
			break;
		case MAD_LAYER_III:
			Layer="III";
			break;
		default:
			Layer="(unexpected layer value)";
			break;
	}

	/* Convert the audio mode to it's printed representation. */
	switch ( Header->mode )
	{
		case MAD_MODE_SINGLE_CHANNEL:
			Mode="single channel";
			break;
		case MAD_MODE_DUAL_CHANNEL:
			Mode="dual channel";
			break;
		case MAD_MODE_JOINT_STEREO:
			Mode="joint (MS/intensity) stereo";
			break;
		case MAD_MODE_STEREO:
			Mode="normal LR stereo";
			break;
		default:
			Mode="(unexpected mode value)";
			break;
	}

	/* Convert the emphasis to it's printed representation. */
	switch ( Header->emphasis )
	{
		case MAD_EMPHASIS_NONE:
			Emphasis="no";
			break;
		case MAD_EMPHASIS_50_15_US:
			Emphasis="50/15 us";
			break;
		case MAD_EMPHASIS_CCITT_J_17:
			Emphasis="CCITT J.17";
			break;
		default:
			Emphasis="(unexpected emphasis value)";
			break;
	}
        
	sprintf( *buf, "%lu kb/s Audio MPEG Layer %s stream %s CRC, "
		           "%s with %s emphasis at %d Hz sample rate.",
		           Header->bitrate, Layer,
		           Header->flags & MAD_FLAG_PROTECTION ? "with" : "without",
		           Mode, Emphasis, Header->samplerate ) ;    
}



// (AAG:) This function will try to decode all frames possible, which exist
// in the input-buffer. Return codes:
// - 0 = everything OK (and all recoverable errors have been recovered)
// - 1 = everything OK, but more input-data is needed to decode rest of the buffer
// - 2 = everything OK, but the output was buffer was too small.
// - 3 = unrecoveranble decoding error, remaining part in inputbuffer is not usefull
//
// Note: 'OutputBufSize' must be an integer multiple of 4.
int MAD_MPEGAudioDecoder( unsigned char *InputBuf,  size_t InputBufSize,
                          unsigned char *OutputBuf, size_t OutputBufSizeIn,
                          unsigned int  *BytesLeft, unsigned int *BytesUnused  )
{
    unsigned int  OutputBufSize = OutputBufSizeIn ;
    unsigned char *OutputPtr    = NULL ;
	int			  Status        = 0 ;
    
	// First the structures used by libmad must be initialised.
    if ( 0==g_MADInit ) 
	{
     MAD_Init( 0 ) ;
    }
    
    OutputPtr    = OutputBuf ;
    *BytesLeft   = InputBufSize ;
    *BytesUnused = OutputBufSize ;
    
	// Pipe the buffer content to libmad's stream decoder facility.
	//
	mad_stream_buffer( &g_Stream, InputBuf, InputBufSize ) ;
	g_Stream.error = (enum mad_error)0 ;
    
	/* Decode the next MPEG frame. The streams is read from the
	 * buffer, its constituents are break down and stored the the
	 * Frame structure, ready for examination/alteration or PCM
	 * synthesis. Decoding options are carried in the Frame
	 * structure from the Stream structure.
	 * 
	 * Error handling: mad_frame_decode() returns a non zero value
	 * when an error occurs. The error condition can be checked in
	 * the error member of the Stream structure. A mad error is
	 * recoverable or fatal, the error status is checked with the
	 * MAD_RECOVERABLE macro.
	 * 
	 * When a fatal error is encountered all decoding
	 * activities shall be stopped, except when a MAD_ERROR_BUFLEN
	 * is signaled. This condition means that the
	 * mad_frame_decode() function needs more input to achieve
	 * it's work. One shall refill the buffer and repeat the
	 * mad_frame_decode() call. Some bytes may be left unused at
	 * the end of the buffer if those bytes forms an incomplete
	 * frame. Before refilling, the remainign bytes must be moved
	 * to the begining of the buffer and used for input for the
	 * next mad_frame_decode() invocation.
	 *
	 * Recoverable errors are caused by malformed bit-streams, in
	 * this case one can call again mad_frame_decode() in order to
	 * skip the faulty part and re-sync to the next frame.
	 */
    
    loop: ;
	if ( mad_frame_decode( &g_Frame, &g_Stream ) )
    {
	 if ( MAD_RECOVERABLE( g_Stream.error ) )
	 {
	  //printf( "Decoding Error: Recoverable frame level error.\nError = (%s)\n",
      //        MAD_ErrorString(&g_Stream) ) ;
      
      // Try to skip the faulty part and re-sync to the next frame.
      //printf( "Will try to recover ... " ) ;
      Status = mad_frame_decode( &g_Frame, &g_Stream ) ;
      if ( Status )
      {
	   //printf( "Failed!\nNew error = (%s)\n", MAD_ErrorString(&g_Stream) ) ;
       Status = 3 ;
      }
      else
      {
       //printf( "Succeded!\n" ) ;
      }
	 }
	 else
     {
	  if ( g_Stream.error==MAD_ERROR_BUFLEN )
	  {
	   //printf( "Decoding Error: Need more data\n" ) ;
       Status = 1 ;
      }
	  else
	  {
	   //printf( "Decoding Error: Unrecoverable frame level error (%s).\n",
	   //        MAD_ErrorString(&g_Stream) ) ;
	   Status = 3 ;
	  }
     }
    } //end-if-decode
    else
    {
     //printf( "Decoding of a frame was succesfull.\n" ) ;
    }
    
/*
	// The first frame is representative of the entire stream.
    {
     char *buf = NULL ;
	 MAD_PrintFrameInfo( &buf, &g_Frame.header ) ;
     printf( "Frame information: %s\n", buf ) ;
     delete buf ;
    }
*/
    
    if ( Status )   //= if error
    {
     return Status ;
    }
    
	/* Accounting. The computed frame duration is in the frame
	 * header structure. It is expressed as a fixed point number
	 * whole data type is mad_timer_t. It is different from the
	 * samples fixed point format and unlike it, it can't directly
	 * be added or substracted. The timer module provides several
	 * functions to operate on such numbers. Be careful there, as
	 * some functions of mad's timer module receive some of their
	 * mad_timer_t arguments by value!
	 */
	mad_timer_add( &g_Timer, g_Frame.header.duration ) ;
	
	// Once decoded the frame is synthesized to PCM samples. No errors
	// are reported by mad_synth_frame();
	//
	mad_synth_frame( &g_Synth, &g_Frame ) ;
    
    // Synthesized samples must be converted from mad's fixed
	// point number to the consumer format. Here we use unsigned
	// 16 bit big endian integers on two channels.
	//
    size_t WrittenBytes = 0 ;
	for ( int i=0 ; i<g_Synth.pcm.length ; i++ )
	{
     unsigned short Sample = 0 ;
     
	 // Left channel
	 //Sample = MAD_FixedToUshort( g_Synth.pcm.samples[0][i] ) ;
	 Sample = MAD_SimpleScale( g_Synth.pcm.samples[0][i] ) ;
     *(OutputPtr++) = (Sample >> 0) & 0xff ;
     *(OutputPtr++) = (Sample >> 8) & 0xff ;
     
	 // Right channel. If the decoded stream is monophonic then
	 // the right output channel is the same as the left one.
	 if( MAD_NCHANNELS(&g_Frame.header)==2 )
     {
	  //Sample = MAD_FixedToUshort( g_Synth.pcm.samples[1][i] ) ;
      Sample = MAD_SimpleScale( g_Synth.pcm.samples[1][i] ) ;
     }
     *(OutputPtr++) = (Sample >> 0) & 0xff ;
     *(OutputPtr++) = (Sample >> 8) & 0xff ;
     
     WrittenBytes += 4 ;
     
	 // Error if the buffer is full.
	 if ( WrittenBytes == OutputBufSize )
	 {
      //printf( "Decoding Warning: Output buffer too small to store the decoded frame.\n" ) ;
      Status = 2 ;
      break ;
	 }
	}
    
    // Process results
    if ( 0==Status )
    {
     // Set the number of unused bytes in the output buffer
     OutputBufSize  = OutputBufSize - WrittenBytes ;
     (*BytesUnused) = OutputBufSize ;
     
     // Set the number of bytes left in the input buffer
     (*BytesLeft) = g_Stream.bufend - g_Stream.next_frame ;
     
     //printf( "Try another frame ...\n" ) ;
     goto loop ;    // Try another frame
    }
    
    if ( 2==Status ) // Decoding was OK, but buffer was not large enough
    {
     /* Don't since it is only part of the the encoded buffer:
      *
      * // Set the number of unused bytes in the output buffer
      * (*BytesUnused) = OutputBufSize - WrittenBytes ;
      */
     
     // Set the number of bytes left in the input buffer
     (*BytesLeft) = g_Stream.bufend - g_Stream.next_frame ;
    }
    
/*
    // Display some info
    if ( !Status )
	{
	 char Buffer[80];
	 mad_timer_string( g_Timer, Buffer, "%lu:%02lu.%03u",
                       MAD_UNITS_MINUTES, MAD_UNITS_MILLISECONDS, 0) ;
	 printf( stdout,"Decoding Raport: %lu frames decoded (%s).\n", 1, Buffer ) ;
	}
*/
    
	// That's the end of the world (in the H. G. Wells way).
    return Status ;
}


void MAD_Init( unsigned int DeInitFlag )
{
 if ( DeInitFlag && 1==g_MADInit)
 {
  mad_synth_finish(  &g_Synth  ) ;
  mad_frame_finish(  &g_Frame  ) ;
  mad_stream_finish( &g_Stream ) ;
  
  g_MADInit = 0 ;
  return ;
 }
 
 if ( !DeInitFlag && 0==g_MADInit )
 {
  mad_stream_init( &g_Stream ) ;
  mad_frame_init(  &g_Frame  ) ;
  mad_synth_init(  &g_Synth  ) ;
  mad_timer_reset( &g_Timer  ) ;
  
  g_MADInit = 1 ;
  return ;
 }
}