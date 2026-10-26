******************************************************************************

 Copyright (c) 2000 Advanced Micro Devices, Inc.

 LIMITATION OF LIABILITY:  THE MATERIALS ARE PROVIDED *AS IS* WITHOUT ANY
 EXPRESS OR IMPLIED WARRANTY OF ANY KIND INCLUDING WARRANTIES OF MERCHANTABILITY,
 NONINFRINGEMENT OF THIRD-PARTY INTELLECTUAL PROPERTY, OR FITNESS FOR ANY
 PARTICULAR PURPOSE.  IN NO EVENT SHALL AMD OR ITS SUPPLIERS BE LIABLE FOR ANY
 DAMAGES WHATSOEVER (INCLUDING, WITHOUT LIMITATION, DAMAGES FOR LOSS OF PROFITS,
 BUSINESS INTERRUPTION, LOSS OF INFORMATION) ARISING OUT OF THE USE OF OR
 INABILITY TO USE THE MATERIALS, EVEN IF AMD HAS BEEN ADVISED OF THE POSSIBILITY
 OF SUCH DAMAGES.  BECAUSE SOME JURISDICTIONS PROHIBIT THE EXCLUSION OR LIMITATION
 OF LIABILITY FOR CONSEQUENTIAL OR INCIDENTAL DAMAGES, THE ABOVE LIMITATION MAY
 NOT APPLY TO YOU.

 AMD does not assume any responsibility for any errors which may appear in the
 Materials nor any responsibility to support or update the Materials.  AMD retains
 the right to make changes to its test specifications at any time, without notice.

 NO SUPPORT OBLIGATION: AMD is not obligated to furnish, support, or make any
 further information, software, technical information, know-how, or show-how
 available to you.

 So that all may benefit from your experience, please report  any  problems
 or  suggestions about this software to 3dsdk.support@@amd.com

 AMD Developer Technologies, M/S 585
 Advanced Micro Devices, Inc.
 5900 E. Ben White Blvd.
 Austin, TX 78741
 3dsdk.support@@amd.com
******************************************************************************/

/*********************************************************
MEMCPY_AMD.CPP
*********************************************************/

// Very optimized memcpy() routine for AMD Athlon and Duron family
// Uses new movnt "non-temporal move" instructions for large blocks
// Be sure you're running on Athlon/Duron or later CPU before calling!
//
// Inline assembly syntax for use with Visual C++

void* memcpy_amd( void *dest, const void *src, size_t n )
{
  __asm {

	mov ecx,[n]		; number of bytes to copy
	mov esi,[src]		; source
	mov edi,[dest]		; destination
	mov ebx,ecx		; keep copy of count
	mov eax,edi		; for returning destination pointer

	cld
	cmp ecx,64
	jb $memcpy_2_1		; if less than 64 bytes, skip mmx copy

//$check_align_problem:		; don't align between 32k-64k because
	cmp ecx,32*1024		;  it appears to be slower
	jbe $align_problem
	cmp ecx,64*1024
	jbe $no_align_problem
$align_problem:
	mov ecx,8
	sub ecx,edi		; align destination to qword
	and ecx,111b
	sub ebx,ecx		; update copy count
	neg ecx
	add ecx,offset $no_align_problem
	jmp ecx			; jump to array of movsb's

align 4
	movsb
	movsb
	movsb
	movsb
	movsb
	movsb
	movsb
	movsb

$no_align_problem:		; already dword aligned
	mov ecx,ebx
	shr ecx,6		; break down into 64-byte block count
	jz $memcpy_2_2

	cmp ecx,64*1024/64	; 64k+? if so, do movntq copy
	jae $memcpy_nt_1

align 16
$memcpy_1:			; do 64-byte block copies first

	prefetch [esi + (200*64/34+192)]

	movq mm0,[esi+0]
	movq mm1,[esi+8]
	movq [edi+0],mm0
	movq [edi+8],mm1
	movq mm2,[esi+16]
	movq mm3,[esi+24]
	movq [edi+16],mm2
	movq [edi+24],mm3
	movq mm0,[esi+32]
	movq mm1,[esi+40]
	movq [edi+32],mm0
	movq [edi+40],mm1
	movq mm2,[esi+48]
	movq mm3,[esi+56]
	movq [edi+48],mm2
	movq [edi+56],mm3

	add esi,64
	add edi,64
	dec ecx
	jnz $memcpy_1		; last 64-byte block?

$memcpy_2_2:
	mov ecx,ebx
$memcpy_2_1:
	shr ecx,2		; dword count
	and ecx,1111b
//$memcpy_2:
	neg ecx
	add ecx,offset $done
	jmp ecx			; jump to array of movsd's

align 16
$memcpy_nt_1:			; 64-byte blocks, uncached copy

	prefetch [esi + (200*64/34+192)]

	movq mm0,[esi+0]
	add edi,64
	movq mm1,[esi+8]
	add esi,64
	movq mm2,[esi-48]
	movntq [edi-64], mm0
	movq mm0,[esi-40]
	movntq [edi-56], mm1
	movq mm1,[esi-32]
	movntq [edi-48], mm2
	movq mm2,[esi-24]
	movntq [edi-40], mm0
	movq mm0,[esi-16]
	movntq [edi-32], mm1
	movq mm1,[esi-8]
	movntq [edi-24], mm2
	movntq [edi-16], mm0
	dec ecx
	movntq [edi-8], mm1
	jnz $memcpy_nt_1	; last 64-byte block?

	mov ecx,ebx
	shr ecx,2		; dword count
	and ecx,1111b
	neg ecx
	add ecx,offset $done
	jmp ecx			; jump to array of movsd's

align 4
	movsd
	movsd			; perform last 1-15 dword copies
	movsd
	movsd
	movsd
	movsd
	movsd
	movsd
	movsd
	movsd			; perform last 1-7 dword copies
	movsd
	movsd
	movsd
	movsd
	movsd
	movsd

$done:				; dword aligned from before movsd's
	mov ecx,ebx
	and ecx,11b		; byte count
	jz $final
	rep movsb		; do last 1..3 bytes

$final: femms			; clean up mmx

        }
 
 return dest ;  // (AAG:) Small mistake from AMD. :-)
}