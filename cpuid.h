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
#ifndef __CPUID_H__INCLUDED__
#define __CPUID_H__INCLUDED__

typedef union cpuid_result
{
	struct
	{
		uint32_t regEAX;
		uint32_t regEBX;
		uint32_t regECX;
		uint32_t regEDX;
	} regs;
	uint32_t raw[4];
} cpuid_result_t;

void cpuid(int page, cpuid_result_t *result);

/* leaf 0x01 */

#define CPUID_ECX_SSE3          (1 << 0)
#define CPUID_ECX_PCLMULQDQ     (1 << 1)
#define CPUID_ECX_DTES64        (1 << 2)
#define CPUID_ECX_MONITOR       (1 << 3)
#define CPUID_ECX_DS_CPL        (1 << 4)
#define CPUID_ECX_VMX           (1 << 5)
#define CPUID_ECX_SMX           (1 << 6)
#define CPUID_ECX_EIST          (1 << 7)
#define CPUID_ECX_TM2           (1 << 8)
#define CPUID_ECX_SSSE3         (1 << 9)
#define CPUID_ECX_L1_CONTEXT_ID (1 << 10)
#define CPUID_ECX_DEBUG_INTERFACE (1 << 11)
#define CPUID_ECX_FMA           (1 << 12)
#define CPUID_ECX_CMPXCHG16B    (1 << 13)
#define CPUID_ECX_XTPR_UPDATE_CONTROL (1 << 14)
#define CPUID_ECX_PERF_CAPABILITIES (1 << 15)
#define CPUID_ECX_PCID          (1 << 17)
#define CPUID_ECX_DCA           (1 << 18)
#define CPUID_ECX_SSE4_1        (1 << 19)
#define CPUID_ECX_SSE4_2        (1 << 20)
#define CPUID_ECX_X2APIC        (1 << 21)
#define CPUID_ECX_MOVBE         (1 << 22)
#define CPUID_ECX_POPCNT        (1 << 23)
#define CPUID_ECX_TSC_DEADLINE  (1 << 24)
#define CPUID_ECX_AESNI         (1 << 25)
#define CPUID_ECX_XSAVE         (1 << 26)
#define CPUID_ECX_OSXSAVE       (1 << 27)
#define CPUID_ECX_AVX           (1 << 28)
#define CPUID_ECX_F16C          (1 << 29)
#define CPUID_ECX_RDRAND        (1 << 30)

#define CPUID_EDX_FPU           (1 << 1)
#define CPUID_EDX_VME           (1 << 2)
#define CPUID_EDX_DE            (1 << 3)
#define CPUID_EDX_PSE           (1 << 4)
#define CPUID_EDX_TSC           (1 << 5)
#define CPUID_EDX_MSR           (1 << 6)
#define CPUID_EDX_PAE           (1 << 7)
#define CPUID_EDX_MCE           (1 << 8)
#define CPUID_EDX_CMPXCHG8B     (1 << 9)
#define CPUID_EDX_APIC          (1 << 10)
#define CPUID_EDX_SEP           (1 << 11)
#define CPUID_EDX_MTRR          (1 << 12)
#define CPUID_EDX_PGE           (1 << 13)
#define CPUID_EDX_MCA           (1 << 14)
#define CPUID_EDX_CMOV          (1 << 15)
#define CPUID_EDX_PAT           (1 << 16)
#define CPUID_EDX_PSE_36        (1 << 17)
#define CPUID_EDX_PSN           (1 << 18)
#define CPUID_EDX_CLFLUSH       (1 << 19)
#define CPUID_EDX_DS            (1 << 21)
#define CPUID_EDX_ACPI          (1 << 22)
#define CPUID_EDX_MMX           (1 << 23)
#define CPUID_EDX_FXSR          (1 << 24)
#define CPUID_EDX_SSE           (1 << 25)
#define CPUID_EDX_SSE2          (1 << 26)
#define CPUID_EDX_SELF_SNOOP    (1 << 27)
#define CPUID_EDX_HTT           (1 << 28)
#define CPUID_EDX_TM            (1 << 29)
#define CPUID_EDX_PBE           (1 << 31)

#endif /* __CPUID_H__INCLUDED__ */
