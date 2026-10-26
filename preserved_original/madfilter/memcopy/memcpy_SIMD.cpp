/ Created by Armin Gerritsen:
//
// Different memcopy methods, accelerated using SIMD
//
// Armin.Gerritsen@@hetnet.nl - 13-09-2001
//

#include "aag_memcpy.h"

#ifndef DONT_USE_ASM

// (AAG:) Taken from a usenet posting. (Author unknown)
void* memcpy_MMX_64b( void *dest, const void *src, unsigned int nbytes )
{
 _asm
 { 
  mov esi, src 
  mov edi, dest 
  mov ecx, nbytes 
  shr ecx, 6         // 64 bytes per iteration 
  
loop1: 
  movq mm1,  0[ESI]  // Read in source data movq mm2,  8[ESI] 	
  movq mm3, 16[ESI]
  movq mm4, 24[ESI]
  movq mm5, 32[ESI]
  movq mm6, 40[ESI]
  movq mm7, 48[ESI]
  movq mm0, 56[ESI] 
  movq  0[EDI], mm1  // Write to destination 
  movq  8[EDI], mm2 
  movq 16[EDI], mm3 
  movq 24[EDI], mm4 
  movq 32[EDI], mm5 
  movq 40[EDI], mm6 
  movq 48[EDI], mm7 
  movq 56[EDI], mm0 
  
  add esi, 64 
  add edi, 64 
  dec ecx 
  jnz loop1 
  
  emms 
 }
 
 return dest ;
}



// (AAG:) Taken from a usenet posting. (By Norbert Juffa)
// This version uses a smarter aproach to mix loads and stores better.
// Is about 3% faster than 'memcpy_MMX_64b' in most cases.
void* memcpy_MMX_Norbert_64b( void *dest, const void *src, unsigned int nbytes )
{
 _asm
 { 
  mov   eax, src
  mov   edx, dest
  mov   ecx, nbytes
  shr   ecx, 6       ;; 64 bytes per iteration 
  
  align 16           ;; VC allows align 16 only ;-(
xfer:
  movq  mm0, [eax]
  add   edx, 64
  
  movq  mm1, [eax+8]
  add   eax, 64
  
  movq  mm2, [eax-48]
  movq  [edx-64], mm0
  
  movq  mm3, [eax-40]
  movq  [edx-56], mm1
  
  movq  mm4, [eax-32]
  movq  [edx-48], mm2
  
  movq  mm5, [eax-24]
  movq  [edx-40], mm3
  
  movq  mm6, [eax-16]
  movq  [edx-32], mm4
  
  movq  mm7, [eax-8]
  movq  [edx-24], mm5
  
  movq  [edx-16], mm6
  dec   ecx
  
  movq  [edx-8], mm7
  jnz   xfer
  
  emms 
 }
 
 return dest ;
}



//
// (AAG:) Based on AMD's 3DNow! example for DWORDS.
// I changed the start- and end-code so it would copy also bytes,
// and not just amounts of DWORDS. Also removed 3DNow! only
// statements like prefetch.
// It can therefore be used as a true memcpy replacement.
// (Note it uses the same internal load/store mix as Norberts MMX 
// version.)
//
void* memcpy_MMX( void *dest, const void *src, unsigned int nbytes )
{
 __asm
 {
  MOV ESI, src         ;; pointer to src, DWORD aligned
  MOV EDI, dest        ;; pointer to dst, DWORD aligned
  MOV ECX, nbytes      ;; number of bytes to copy
  MOV EDX, ECX         ;; backup number of bytes
  
  CMP ECX, 4           ;; less than one DWORD to copy ?
  JB $copybyte2_cc     ;; yes, must be no DWORDs to copy, maybe bytes
  
  SHR ECX, 2           ;; number of DWORDs to copy
  
  TEST EDI,7           ;; dst QWORD aligned?
  JZ $dstqaligned2_cc  ;; yes
  
  MOVD MM0,[ ESI ]     ;; read one DWORD from src
  MOVD [EDI ] ,MM0     ;; store one DWORD to dst
  ADD ESI,4            ;; src++
  ADD EDI,4            ;; dst++
  DEC ECX              ;; number of DWORDs to copy
  
$dstqaligned2_cc:
  MOV EBX,ECX          ;; number of DWORDs to copy
  SHR ECX,4            ;; number of cache lines to copy
  JZ $copyqwords2_cc   ;; no whole cache lines to copy, maybe QWORDs
  
  ALIGN 16             ;; align loop for optimal performance
  
$cloop2_cc:
  MOVQ MM0,[ ESI ]     ;; load first QWORD in cache line from src
  ADD EDI,64           ;; src++
  MOVQ MM1,[ ESI+8 ]   ;; load second QWORD in cache line from src
  ADD ESI,64           ;; dst++
  MOVQ MM2,[ ESI-48 ]  ;; load third QWORD in cache line from src
  MOVQ [EDI-64 ] ,MM0  ;; store first DWORD in cache line to dst
  MOVQ MM0,[ ESI-40 ]  ;; load fourth QWORD in cache line from src
  MOVQ [EDI-56 ] ,MM1  ;; store second DWORD in cache line to dst
  MOVQ MM1,[ ESI-32 ]  ;; load fifth QWORD in cache line from src
  MOVQ [EDI-48 ] ,MM2  ;; store third DWORD in cache line to dst
  MOVQ MM2,[ ESI-24 ]  ;; load sixth QWORD in cache line from src
  MOVQ [EDI-40 ] ,MM0  ;; store fourth DWORD in cache line to dst
  MOVQ MM0,[ ESI-16 ]  ;; load seventh QWORD in cache line from src
  MOVQ [EDI-32 ] ,MM1  ;; store fifth DWORD in cache line to dst
  MOVQ MM1,[ ESI-8 ]   ;; load eight QWORD in cache line from src
  MOVQ [EDI-24 ] ,MM2  ;; store sixth DWORD in cache line to dst
  MOVQ [EDI-16 ] ,MM0  ;; store seventh DWORD in cache line to dst
  DEC ECX              ;; count--
  MOVQ [EDI-8 ] ,MM1   ;; store eighth DWORD in cache line to dst
  JNZ $cloop2_cc       ;; until no more cache lines to copy
  
$copyqwords2_cc:
  MOV ECX,EBX          ;; number of DWORDs to copy
  AND EBX,0xE          ;; number of QWORDS left to copy *2
  SHR EBX, 1           ;; number of QWORDS left to copy
  JZ $copydword2_cc    ;; no QWORDs left, maybe DWORD left
  
  ALIGN 16             ;; align loop for optimal performance
  
$qloop2_cc:
  MOVQ MM0, [ESI]      ;; read QWORD from src
  MOVQ [EDI], MM0      ;; store QWORD to dst
  ADD ESI,8            ;; src++
  ADD EDI,8            ;; dst++
  SUB EBX,1            ;; count--
  JNZ $qloop2_cc       ;; until no more QWORDs left to copy
  
$copydword2_cc:
  TEST ECX, 1          ;; DWORD left to copy ?
  JZ $copybyte2_cc     ;; nope, but maybe bytes left
  MOVD MM0, [ESI]      ;; read last DWORD from src
  MOVD [EDI], MM0      ;; store last DWORD to dst
  ADD  ESI, 4          ;; 4 bytes read
  ADD  EDI, 4          ;; 4 bytes written
  
$copybyte2_cc:
  AND EDX, 0x3         ;; number of bytes left to copy
  JZ $copydone2_cc     ;; no bytes left, we're done
  
$bloop2_cc:
  MOV AH,BYTE PTR [ESI]
  MOV BYTE PTR [EDI],AH
  ADD ESI,1            ;; src++
  ADD EDI,1            ;; dst++
  SUB EDX,1            ;; count--
  JNZ $bloop2_cc       ;; until no more bytes left to copy
  
$copydone2_cc:
  EMMS                 ;; clear MMX state
 }

 return dest ;
}



//
// (AAG:) Based on AMD's 3DNow! example for DWORDS.
// I changed the start- and end-code so it would copy also bytes,
// and not just amounts of DWORDS.
// It can therefore be used as a true memcpy replacement.
// (Note it uses the same internal load/store mix as Norberts MMX 
// version.)
//
void* memcpy_3DNow( void *dest, const void *src, unsigned int nbytes )
{
 __asm
 {
  MOV ESI, src         ;; pointer to src, DWORD aligned
  MOV EDI, dest        ;; pointer to dst, DWORD aligned
  MOV ECX, nbytes      ;; number of bytes to copy
  MOV EDX, ECX         ;; backup number of bytes
  
  PREFETCH [ESI]       ;; prefetch first src cache line
  CMP ECX, 4           ;; less than one DWORD to copy ?
  JB $copybyte2_cc     ;; yes, must be no DWORDs to copy, maybe bytes
  
  SHR ECX, 2           ;; number of DWORDs to copy
  
  TEST EDI,7           ;; dst QWORD aligned?
  JZ $dstqaligned2_cc  ;; yes
  
  MOVD MM0,[ ESI ]     ;; read one DWORD from src
  MOVD [EDI ] ,MM0     ;; store one DWORD to dst
  ADD ESI,4            ;; src++
  ADD EDI,4            ;; dst++
  DEC ECX              ;; number of DWORDs to copy
  
$dstqaligned2_cc:
  MOV EBX,ECX          ;; number of DWORDs to copy
  SHR ECX,4            ;; number of cache lines to copy
  JZ $copyqwords2_cc   ;; no whole cache lines to copy, maybe QWORDs
  
  prefetch [ESI+64]    ;; prefetch src cache line ahead
  
  ALIGN 16             ;; align loop for optimal performance
  
$cloop2_cc:
  prefetchw [EDI+64]   ;; prefetch dst cache line ahead
  MOVQ MM0,[ ESI ]     ;; load first QWORD in cache line from src
  ADD EDI,64           ;; src++
  MOVQ MM1,[ ESI+8 ]   ;; load second QWORD in cache line from src
  ADD ESI,64           ;; dst++
  MOVQ MM2,[ ESI-48 ]  ;; load third QWORD in cache line from src
  MOVQ [EDI-64 ] ,MM0  ;; store first DWORD in cache line to dst
  MOVQ MM0,[ ESI-40 ]  ;; load fourth QWORD in cache line from src
  MOVQ [EDI-56 ] ,MM1  ;; store second DWORD in cache line to dst
  MOVQ MM1,[ ESI-32 ]  ;; load fifth QWORD in cache line from src
  MOVQ [EDI-48 ] ,MM2  ;; store third DWORD in cache line to dst
  MOVQ MM2,[ ESI-24 ]  ;; load sixth QWORD in cache line from src
  MOVQ [EDI-40 ] ,MM0  ;; store fourth DWORD in cache line to dst
  MOVQ MM0,[ ESI-16 ]  ;; load seventh QWORD in cache line from src
  MOVQ [EDI-32 ] ,MM1  ;; store fifth DWORD in cache line to dst
  MOVQ MM1,[ ESI-8 ]   ;; load eight QWORD in cache line from src
  MOVQ [EDI-24 ] ,MM2  ;; store sixth DWORD in cache line to dst
  MOVQ [EDI-16 ] ,MM0  ;; store seventh DWORD in cache line to dst
  DEC ECX              ;; count--
  MOVQ [EDI-8 ] ,MM1   ;; store eighth DWORD in cache line to dst
  JNZ $cloop2_cc       ;; until no more cache lines to copy
  
$copyqwords2_cc:
  MOV ECX,EBX          ;; number of DWORDs to copy
  AND EBX,0xE          ;; number of QWORDS left to copy *2
  SHR EBX, 1           ;; number of QWORDS left to copy
  JZ $copydword2_cc    ;; no QWORDs left, maybe DWORD left
  
  ALIGN 16             ;; align loop for optimal performance
  
$qloop2_cc:
  MOVQ MM0, [ESI]      ;; read QWORD from src
  MOVQ [EDI], MM0      ;; store QWORD to dst
  ADD ESI,8            ;; src++
  ADD EDI,8            ;; dst++
  SUB EBX,1            ;; count--
  JNZ $qloop2_cc       ;; until no more QWORDs left to copy
  
$copydword2_cc:
  TEST ECX, 1          ;; DWORD left to copy ?
  JZ $copybyte2_cc     ;; nope, but maybe bytes left
  MOVD MM0, [ESI]      ;; read last DWORD from src
  MOVD [EDI], MM0      ;; store last DWORD to dst
  ADD  ESI, 4          ;; 4 bytes read
  ADD  EDI, 4          ;; 4 bytes written
  
$copybyte2_cc:
  AND EDX, 0x3         ;; number of bytes left to copy
  JZ $copydone2_cc     ;; no bytes left, we're done
  
$bloop2_cc:
  MOV AH, BYTE PTR [ESI]
  MOV BYTE PTR [EDI], AH
  ADD ESI,1            ;; src++
  ADD EDI,1            ;; dst++
  SUB EDX,1            ;; count--
  JNZ $bloop2_cc       ;; until no more bytes left to copy
  
$copydone2_cc:
  FEMMS                ;; clear MMX state
 }
 
 return dest ;
}



//
// (AAG:) Based on AMD's 3DNow! example for DWORDS.
// I changed the start- and end-code so it would copy also bytes,
// and not just amounts of DWORDS.
// 
// It can therefore be used as a true memcpy replacement.
// (Note it uses the same internal load/store mix as Norberts MMX 
// version.)
//
// Note: It is not a true SSE memcpy, but merely a MMX with SSE 
//       prefetch memcpy.
//
void* memcpy_SSE( void *dest, const void *src, unsigned int nbytes )
{
 __asm
 {
  MOV ESI, src         ;; pointer to src, DWORD aligned
  MOV EDI, dest        ;; pointer to dst, DWORD aligned
  MOV ECX, nbytes      ;; number of bytes to copy
  MOV EDX, ECX         ;; backup number of bytes
  
  PREFETCHT0 [ESI]     ;; prefetch first src cache line
  CMP ECX, 4           ;; less than one DWORD to copy ?
  JB $copybyte2_cc     ;; yes, must be no DWORDs to copy, maybe bytes
  
  SHR ECX, 2           ;; number of DWORDs to copy
  
  TEST EDI,7           ;; dst QWORD aligned?
  JZ $dstqaligned2_cc  ;; yes
  
  MOVD MM0,[ ESI ]     ;; read one DWORD from src
  MOVD [EDI ] ,MM0     ;; store one DWORD to dst
  ADD ESI,4            ;; src++
  ADD EDI,4            ;; dst++
  DEC ECX              ;; number of DWORDs to copy
  
$dstqaligned2_cc:
  MOV EBX,ECX          ;; number of DWORDs to copy
  SHR ECX,4            ;; number of cache lines to copy
  JZ $copyqwords2_cc   ;; no whole cache lines to copy, maybe QWORDs
  
  prefetcht0 [ESI+64]  ;; prefetch src cache line ahead
  
  ALIGN 16             ;; align loop for optimal performance
  
$cloop2_cc:
  prefetcht0 [EDI+64]  ;; prefetch dst cache line ahead
  MOVQ MM0,[ ESI ]     ;; load first QWORD in cache line from src
  ADD EDI,64           ;; src++
  MOVQ MM1,[ ESI+8 ]   ;; load second QWORD in cache line from src
  ADD ESI,64           ;; dst++
  MOVQ MM2,[ ESI-48 ]  ;; load third QWORD in cache line from src
  MOVQ [EDI-64 ] ,MM0  ;; store first DWORD in cache line to dst
  MOVQ MM0,[ ESI-40 ]  ;; load fourth QWORD in cache line from src
  MOVQ [EDI-56 ] ,MM1  ;; store second DWORD in cache line to dst
  MOVQ MM1,[ ESI-32 ]  ;; load fifth QWORD in cache line from src
  MOVQ [EDI-48 ] ,MM2  ;; store third DWORD in cache line to dst
  MOVQ MM2,[ ESI-24 ]  ;; load sixth QWORD in cache line from src
  MOVQ [EDI-40 ] ,MM0  ;; store fourth DWORD in cache line to dst
  MOVQ MM0,[ ESI-16 ]  ;; load seventh QWORD in cache line from src
  MOVQ [EDI-32 ] ,MM1  ;; store fifth DWORD in cache line to dst
  MOVQ MM1,[ ESI-8 ]   ;; load eight QWORD in cache line from src
  MOVQ [EDI-24 ] ,MM2  ;; store sixth DWORD in cache line to dst
  MOVQ [EDI-16 ] ,MM0  ;; store seventh DWORD in cache line to dst
  DEC ECX              ;; count--
  MOVQ [EDI-8 ] ,MM1   ;; store eighth DWORD in cache line to dst
  JNZ $cloop2_cc       ;; until no more cache lines to copy
  
$copyqwords2_cc:
  MOV ECX,EBX          ;; number of DWORDs to copy
  AND EBX,0xE          ;; number of QWORDS left to copy *2
  SHR EBX, 1           ;; number of QWORDS left to copy
  JZ $copydword2_cc    ;; no QWORDs left, maybe DWORD left
  
  ALIGN 16             ;; align loop for optimal performance
  
$qloop2_cc:
  MOVQ MM0, [ESI]      ;; read QWORD from src
  MOVQ [EDI], MM0      ;; store QWORD to dst
  ADD ESI,8            ;; src++
  ADD EDI,8            ;; dst++
  SUB EBX,1            ;; count--
  JNZ $qloop2_cc       ;; until no more QWORDs left to copy
  
$copydword2_cc:
  TEST ECX, 1          ;; DWORD left to copy ?
  JZ $copybyte2_cc     ;; nope, but maybe bytes left
  MOVD MM0, [ESI]      ;; read last DWORD from src
  MOVD [EDI], MM0      ;; store last DWORD to dst
  ADD  ESI, 4          ;; 4 bytes read
  ADD  EDI, 4          ;; 4 bytes written
  
$copybyte2_cc:
  AND EDX, 0x3         ;; number of bytes left to copy
  JZ $copydone2_cc     ;; no bytes left, we're done
  
$bloop2_cc:
  MOV AH, BYTE PTR [ESI]
  MOV BYTE PTR [EDI], AH
  ADD ESI,1            ;; src++
  ADD EDI,1            ;; dst++
  SUB EDX,1            ;; count--
  JNZ $bloop2_cc       ;; until no more bytes left to copy
  
$copydone2_cc:
  EMMS                 ;; clear MMX state
 }
 
 return dest ;
}

#endif //DONT_USE_ASM@


1.1
log
@first import