/ Fast memcopy functions
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


#include "aag_memcpy.h"

#ifndef DEBUG
#ifndef DONT_USE_ASM

#ifdef _AAG_MEMCOPY_VERBOSE
    #include <stdio.h>
#endif


// Externals
extern void* memcpy_MMX_64b( void *, const void *, unsigned int ) ;          // MMX 64 byte blocks
extern void* memcpy_MMX_Norbert_64b( void *, const void *, unsigned int ) ;  // MMX 64 byte blocks

extern void* memcpy_MMX  ( void *, const void *, unsigned int ) ;            // MMX generic
extern void* memcpy_3DNow( void *, const void *, unsigned int ) ;            // 3DNow! generic
extern void* memcpy_SSE  ( void *, const void *, unsigned int ) ;            // MMX with SSE prefetch generic
extern void* memcpy_amd  ( void *, const void *, unsigned int ) ;            // Enhanced 3DNow! generic


// Internals
static unsigned int SupportsMMX() ;
static unsigned int Supports3DNow() ;
static unsigned int SupportsEnhanced3DNow() ;
static unsigned int SupportsSSE() ;
static unsigned int SupportsSSE2() ;


// Main pointer
aag_memcpy_type aag_memcpy = memcpy ;

void aag_memcopy_enable()
{
 if ( SupportsEnhanced3DNow() )
 {
  aag_memcpy = memcpy_amd ;
 }
 else
 if ( Supports3DNow() )
 {
  aag_memcpy = memcpy_3DNow ;
 }
 else
 if ( SupportsSSE() )
 {
  aag_memcpy = memcpy_SSE ;
 }
 else
 if ( SupportsMMX() )
 {
  aag_memcpy = memcpy_MMX ;
 }
 else
 {
  aag_memcpy = memcpy ;
 }
}


#ifdef _AAG_MEMCOPY_VERBOSE
void aag_memcopy_enable_verbose()
{
 if ( SupportsEnhanced3DNow() )
 {
  printf("Using Enhanced 3DNow! optimisations.\n") ;
  aag_memcpy = memcpy_amd ;
 }
 else
 if ( Supports3DNow() )
 {
  printf("Using 3DNow! optimisations.\n") ;
  aag_memcpy = memcpy_3DNow ;
 }
 else
 if ( SupportsSSE() )
 {
  printf("Using MMX with SSE prefetch optimisations.\n") ;
  aag_memcpy = memcpy_SSE ;
 }
 else
 if ( SupportsMMX() )
 {
  printf("Using MMX optimisations.\n") ;
  aag_memcpy = memcpy_MMX ;
 }
 else
 {
  aag_memcpy = memcpy ;
 }
}
#endif


void aag_memcopy_disable()
{
 aag_memcpy = memcpy ;
}



unsigned int Supports3DNow()
{
 unsigned int iResult      = 0,
              iMaxExtended = 0 ;
 
 // Werkt niet omdat de extended-level return-waarden op Intel CPU's officieel ongedefinieerd zijn
 /*
 iMaxExtended = GetMaxExtended() ;
 //printf("Max Extended Level  :\t%xh\n", iMaxExtended ) ;
 if ( 0==iMaxExtended )
  return 0 ;
 */
  
 //NB: Alle Intel IA-32 CPU's van de PIII en hoger zullen 0 in bit 31 retourneren als
 //EAX > MAX_CPUID_VAL. Dit lijkt ook het geval te zijn met alle huidige  PII's.
 //Zo deze code zal velig zijn op ten minste AMD, Centaur, Cyrix and Intel.
 _asm
 {
  mov  eax, 0x80000001 ; set EAX
  cpuid                ; get CPUID
  TEST edx, 0x80000000 ; check bit 31
  JZ no_3dnow          ; jump if bit=0
  mov iResult, 1       ; set flag
no_3dnow:
 }
 
 return iResult ;
}


unsigned int SupportsEnhanced3DNow()
{
 unsigned int iResult      = 0,
              iMaxExtended = 0 ;
 
 // Werkt niet omdat de extended-level return-waarden op Intel CPU's officieel ongedefinieerd zijn
 /*
 iMaxExtended = GetMaxExtended() ;
 //printf("Max Extended Level  :\t%xh\n", iMaxExtended ) ;
 if ( 0==iMaxExtended )
  return 0 ;
 */
  
 _asm
 {
  mov  eax, 0x80000001 ; zet EAX
  cpuid                ; verkrijg CPUID
  TEST edx, 0x40000000 ; controleer bit 30
  JZ no_enh3dnow       ; jump als bit=0
  mov iResult, 1       ; zet vlag
no_enh3dnow:
 }
 
 return iResult ;
}


unsigned int SupportsMMX()
{
 unsigned int iResult = 0 ;

  _asm
  {
   mov  eax, 1          ; zet EAX
   cpuid                ; verkrijg CPUID
   TEST edx, 0x00800000 ; controleer bit 23
   JZ no_mmx            ; jump als bit=0
   mov iResult, 1       ; zet vlag
no_mmx:
  }
 
 return iResult ;
}


unsigned int SupportsSSE()
{
 unsigned int iResult = 0 ;

  _asm
  {
   mov  eax, 1          ; zet EAX
   cpuid                ; verkrijg CPUID
   TEST edx, 0x02000000 ; controleer bit 25
   JZ no_sse            ; jump als bit=0
   mov iResult, 1       ; zet vlag
no_sse:
  }
 
 return iResult ;
}


unsigned int SupportsSSE2()
{
 unsigned int iResult = 0 ;

  _asm
  {
   mov  eax, 1          ; zet EAX
   cpuid                ; verkrijg CPUID
   TEST edx, 004000000h ; controleer bit 26
   JZ no_sse2           ; jump als bit=0
   mov iResult, 1       ; zet vlag
no_sse2:
  }
 
 return iResult ;
}

#endif //DONT_USE_ASM
#endif //DEBUG