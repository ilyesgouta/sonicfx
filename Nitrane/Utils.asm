;
; SSE2 optimized float/int convertion routines for SonicFX
; Copyright (C) 2009-2010, Ilyes Gouta (ilyes.gouta@gmail.com)
;
; SonicFX is free software: you can redistribute it and/or modify
; it under the terms of the GNU Lesser General Public License as published by
; the Free Software Foundation, either version 3 of the License, or
; (at your option) any later version.
;
; SonicFX is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU Lesser General Public License for more details.
;
; You should have received a copy of the GNU Lesser General Public License
; along with SonicFX.  If not, see <http://www.gnu.org/licenses/>.
;

cpu p4
bits 32

section .text align=16

; void float2short(float* in, short* out, int size)
; out must be aligned on a 16 bytes boundary, size shoule be a multiple of 8.
global _float2short

_float2short:
	mov ebx, [esp + 4]        ; float* in
	mov eax, [esp + 8]        ; short* out
	mov ecx, [esp + 12]       ; int size

.loop:
	cvtps2dq xmm1, [ebx]      ; convert 4 floats to 4 doublewords.
	cvtps2dq xmm0, [ebx + 16] ; convert 4 floats to 4 doublewords.

	packssdw xmm1, xmm0       ; convert the 8 doublewords from xmm0 and xmm1 to 8 saturated words.
	add ebx, 32

	movdqa [eax], xmm1
	add eax, 16

	sub ecx, 8
	jnz .loop

	ret

; void short2float(short* in, float* out, int size)
; in/out must be aligned on a 16 bytes boundary, size shoule be a multiple of 8.
global _short2float

_short2float:
	mov ebx, [esp + 4]      ; short* in
	mov eax, [esp + 8]      ; float* out
	mov ecx, [esp + 12]     ; int size

.loop:
	movdqa xmm0, [ebx]      ; fetch 8 words from memory.
	pxor xmm1, xmm1         ; zero'ed.

	movdqa xmm3, xmm0
	add ebx, 16

	punpcklwd xmm1, xmm0
	psrad xmm1, 16          ; reconstruct the signed doubleword representation.

	cvtdq2ps xmm2, xmm1     ; convert 4 doublewords to 4 floats.
	movaps [eax], xmm2

	pxor xmm1, xmm1         ; zero'ed.
	add eax, 32

	punpckhwd xmm1, xmm3
	psrad xmm1, 16          ; reconstruct the signed doubleword representation.

	cvtdq2ps xmm2, xmm1     ; convert 4 doublewords to 4 floats.
	movaps [eax - 16], xmm2

	sub ecx, 8
	jnz .loop

	ret
