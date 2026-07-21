; ******************************************************************************
; * Copyright (c) 2026 Jaroslav Hensl <emulator@emulace.cz>                    *
; *                                                                            *
; * Permission is hereby granted, free of charge, to any person obtaining a    *
; * copy of this software and associated documentation files (the "Software"), *
; * to deal in the Software without restriction, including without limitation  *
; * the rights to use, copy, modify, merge, publish, distribute, sublicense,   *
; * and/or sell copies of the Software, and to permit persons to whom the      *
; * Software is furnished to do so, subject to the following conditions:       *
; *                                                                            *
; * The above copyright notice and this permission notice shall be included in *
; * all copies or substantial portions of the Software.                        *
; *                                                                            *
; * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR *
; * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   *
; * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    *
; * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER *
; * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    *
; * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        *
; * DEALINGS IN THE SOFTWARE.                                                  *
; ******************************************************************************
;
; FASM source
;
format MS COFF

section '.text' code readable executable

;
; int __cdecl cpuid(int page, cpuid_result_t *result)
;
public _cpuid
	param_page equ ebp+8
	param_result equ ebp+12
_cpuid:
	push ebp
	mov  ebp, esp
	
	; check if CPUID is supported
  pushfd
  pushfd
  xor dword [esp], 0x00200000
  popfd
  pushfd
  pop eax
  xor eax,[esp]
  popfd
  and eax, 0x00200000
  jnz have_cpuid
  	xor eax, eax
  	pop ebp
  	retn
  
  ; yes, copy result to [result]
  have_cpuid:
	push ebx
	push edi
	mov eax, [param_page]
	xor ecx, ecx
	cpuid
	mov edi, [param_result]
	mov [edi   ], eax
	mov [edi+ 4], ebx
	mov [edi+ 8], ecx
	mov [edi+12], edx
	pop edi
	pop ebx
	
	mov eax, 1
	pop ebp
	retn
