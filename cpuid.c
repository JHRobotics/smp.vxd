/******************************************************************************
 * Copyright (c) 2026 Jaroslav Hensl <emulator@emulace.cz>                    *
 *                                                                            *
 * Permission is hereby granted, free of charge, to any person obtaining a    *
 * copy of this software and associated documentation files (the "Software"), *
 * to deal in the Software without restriction, including without limitation  *
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,   *
 * and/or sell copies of the Software, and to permit persons to whom the      *
 * Software is furnished to do so, subject to the following conditions:       *
 *                                                                            *
 * The above copyright notice and this permission notice shall be included in *
 * all copies or substantial portions of the Software.                        *
 *                                                                            *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR *
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   *
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    *
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER *
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    *
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        *
 * DEALINGS IN THE SOFTWARE.                                                  *
 ******************************************************************************/
#include "vxd.h"

int cpuid(int page, cpuid_result_t *result)
{
	int rc = 0;
	_asm
	{
    pushfd
    pushfd
    xor dword ptr [esp],0x00200000
    popfd
    pushfd
    pop eax
    xor eax,[esp]
    popfd
    and eax,0x00200000
    jz nocpuid
		mov [rc], 1
		
		push ebx
		push edi
		mov eax, [page]
		xor ecx, ecx
		cpuid
		mov edi, [result]
		mov [edi   ], eax
		mov [edi+ 4], ebx
		mov [edi+ 8], ecx
		mov [edi+12], edx
		pop edi
		pop ebx
		
		nocpuid:
	};
	
	return rc;
}

/**
 *
 * @return: frequency in kHz (mainly because 32bit overflow at > ~4.3 GHz CPU)
 *
 **/
unsigned long cpuid_tsc_freq()
{
	unsigned long freq = 0;
	_asm
	{
		push ebx
		push edi
		push esi
		mov eax, 0x15
		cpuid
		test eax, eax
		jz tsc_done
		test ecx, ecx
		jz read_16h ; TSCFreq = (ECX*(EBX/EAX))/1000 = ((ECX / 1000)*EBX)/EAX
			mov esi, eax
			mov edi, ebx

			mov eax, ecx
			xor edx, edx
			mov ebx, 1000
			div ebx
			xor edx, edx
			mul edi
			jo tsc_done

			div esi
			mov [freq], eax
			jmp tsc_done
		read_16h:
			; TSCFreq = CPUID_16h_EAX * 1000
			mov eax, 0x16
			cpuid
			test eax, eax
			jz tsc_done
			mov ecx, 1000
			mul ecx
			jo tsc_done
			mov [freq], eax

		tsc_done:
		pop esi
		pop edi
		pop ebx
	};
	
	return freq;
}
