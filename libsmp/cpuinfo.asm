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
; @param result: can be NULL
; @return: 1 on success = CPUID is supported), 0 when CPUID is not supported
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
	test edi, edi
	jz skip_save
		mov [edi   ], eax
		mov [edi+ 4], ebx
		mov [edi+ 8], ecx
		mov [edi+12], edx
	skip_save:
	
	pop edi
	pop ebx
	
	mov eax, 1
	pop ebp
	retn

;
; int __cdecl long cpuid_tsc_freq()
;
; @return: frequency in kHz (mainly because 32bit overflow at > ~4.3 GHz CPU)
;
public _cpuid_tsc_freq
_cpuid_tsc_freq:
	push ebx
	push edi
	push esi
	mov eax, 0x15
	cpuid
	test eax, eax
	jz tsc_fail
	test ecx, ecx
	jz read_16h ; TSCFreq = (ECX*(EBX/EAX))/1000 = ((ECX / 1000)*EBX)/EAX
;	jmp read_16h
		mov esi, eax
		mov edi, ebx

		mov eax, ecx
		xor edx, edx
		mov ebx, 1000
		div ebx
		xor edx, edx
		mul edi
		jo tsc_fail

		div esi
		jmp tsc_done
	read_16h:
		; TSCFreq = CPUID_16h_EAX * 1000
		mov eax, 0x16
		cpuid
		test eax, eax
		jz tsc_fail
		mov ecx, 1000
		mul ecx
		jo tsc_fail
		jmp tsc_done

	tsc_fail:
		xor eax, eax

	tsc_done:
	pop esi
	pop edi
	pop ebx
	retn


