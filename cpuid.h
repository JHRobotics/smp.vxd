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

#define CPUID_ECX_SSE3          0
#define CPUID_ECX_PCLMULQDQ     1
#define CPUID_ECX_DTES64        2
#define CPUID_ECX_MONITOR       3
#define CPUID_ECX_DS_CPL        4
#define CPUID_ECX_VMX           5
#define CPUID_ECX_SMX           6
#define CPUID_ECX_EIST          7
#define CPUID_ECX_TM2           8
#define CPUID_ECX_SSSE3         9
#define CPUID_ECX_L1_CONTEXT_ID 10
#define CPUID_ECX_DEBUG_INTERFACE 11
#define CPUID_ECX_FMA           12
#define CPUID_ECX_CMPXCHG16B    13
#define CPUID_ECX_XTPR_UPDATE_CONTROL 14
#define CPUID_ECX_PERF_CAPABILITIES 15
#define CPUID_ECX_PCID          17
#define CPUID_ECX_DCA           18
#define CPUID_ECX_SSE4_1        19
#define CPUID_ECX_SSE4_2        20
#define CPUID_ECX_X2APIC        21
#define CPUID_ECX_MOVBE         22
#define CPUID_ECX_POPCNT        23
#define CPUID_ECX_TSC_DEADLINE  24
#define CPUID_ECX_AESNI         25
#define CPUID_ECX_XSAVE         26
#define CPUID_ECX_OSXSAVE       27
#define CPUID_ECX_AVX           28
#define CPUID_ECX_F16C          29
#define CPUID_ECX_RDRAND        30

#define CPUID_EDX_FPU           1
#define CPUID_EDX_VME           2
#define CPUID_EDX_DE            3
#define CPUID_EDX_PSE           4
#define CPUID_EDX_TSC           5
#define CPUID_EDX_MSR           6
#define CPUID_EDX_PAE           7
#define CPUID_EDX_MCE           8
#define CPUID_EDX_CMPXCHG8B     9
#define CPUID_EDX_APIC          10
#define CPUID_EDX_SEP           11
#define CPUID_EDX_MTRR          12
#define CPUID_EDX_PGE           13
#define CPUID_EDX_MCA           14
#define CPUID_EDX_CMOV          15
#define CPUID_EDX_PAT           16
#define CPUID_EDX_PSE_36        17
#define CPUID_EDX_PSN           18
#define CPUID_EDX_CLFLUSH       19
#define CPUID_EDX_DS            21
#define CPUID_EDX_ACPI          22
#define CPUID_EDX_MMX           23
#define CPUID_EDX_FXSR          24
#define CPUID_EDX_SSE           25
#define CPUID_EDX_SSE2          26
#define CPUID_EDX_SELF_SNOOP    27
#define CPUID_EDX_HTT           28
#define CPUID_EDX_TM            29
#define CPUID_EDX_PBE           31

#endif /* __CPUID_H__INCLUDED__ */
