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
#ifndef __IPFP_H__INCLUDED__
#define __IPFP_H__INCLUDED__

#define FCHECK(_leaf, _reg, _f) if(_leaf.regs.reg ## _reg & _f) return TRUE;

static BOOL WINAPI IsProcessorFeaturePresentCPUID(DWORD ProcessorFeature)
{
	cpuid_result_t leaf1, leaf7, leaf81;

  memset(&leaf1, 0, sizeof(cpuid_result_t));
  memset(&leaf7, 0, sizeof(cpuid_result_t));
  memset(&leaf81, 0, sizeof(cpuid_result_t));

#ifndef OWN_CPUID
	__get_cpuid(0x00000001,  &leaf1.regs.regEAX,  &leaf1.regs.regEBX,  &leaf1.regs.regECX,  &leaf1.regs.regEDX);
	__get_cpuid(0x00000007,  &leaf7.regs.regEAX,  &leaf7.regs.regEBX,  &leaf7.regs.regECX,  &leaf7.regs.regEDX);
	__get_cpuid(0x80000001, &leaf81.regs.regEAX, &leaf81.regs.regEBX, &leaf81.regs.regECX, &leaf81.regs.regEDX);
#else
	cpuid(0x00000001, &leaf1);
	cpuid(0x00000007, &leaf7);
	cpuid(0x80000001, &leaf81);
#endif

	switch(ProcessorFeature)
	{
		case PF_FLOATING_POINT_PRECISION_ERRATA:  return FALSE;
		case PF_FLOATING_POINT_EMULATED:          return FALSE;
		case PF_COMPARE_EXCHANGE_DOUBLE:          FCHECK(leaf1,  EDX, CPUID_EDX_MCE);   break;
		case PF_MMX_INSTRUCTIONS_AVAILABLE:       FCHECK(leaf1,  EDX, CPUID_EDX_MMX);   break;
		case PF_XMMI_INSTRUCTIONS_AVAILABLE:      FCHECK(leaf1,  EDX, CPUID_EDX_SSE);   break;
		case PF_3DNOW_INSTRUCTIONS_AVAILABLE:     FCHECK(leaf81, EDX, CPUID_EDX_3DNOW); break;
		case PF_RDTSC_INSTRUCTION_AVAILABLE:      FCHECK(leaf1,  EDX, CPUID_EDX_TSC);   break;
		case PF_PAE_ENABLED:                      return FALSE; /* in theory on 2000 server, but assume no */
		case PF_XMMI64_INSTRUCTIONS_AVAILABLE:    FCHECK(leaf1,  EDX, CPUID_EDX_SSE2);  break;
		case PF_NX_ENABLED:                       return FALSE; /* not supported until XP SP2 */
		case PF_SSE3_INSTRUCTIONS_AVAILABLE:      FCHECK(leaf81, ECX, CPUID_ECX_SSE3);  break;
		case PF_COMPARE_EXCHANGE128:              FCHECK(leaf1,  ECX, CPUID_ECX_CMPXCHG16B); break;
		case PF_COMPARE64_EXCHANGE128:            FCHECK(leaf1,  ECX, CPUID_ECX_CMPXCHG16B); break;
		case PF_CHANNELS_ENABLED:                 return TRUE; /* ? */
		case PF_XSAVE_ENABLED:                    FCHECK(leaf1,  ECX, CPUID_ECX_OSXSAVE); break;
		case PF_SECOND_LEVEL_ADDRESS_TRANSLATION: return TRUE;
		case PF_VIRT_FIRMWARE_ENABLED:            FCHECK(leaf1,  ECX, CPUID_ECX_VMX); break;
		case PF_RDWRFSGSBASE_AVAILABLE:           return FALSE; /* only in 64b mode */
		case PF_FASTFAIL_AVAILABLE:               return FALSE;
		case PF_RDRAND_INSTRUCTION_AVAILABLE:     FCHECK(leaf1,  ECX, CPUID_ECX_RDRAND);   break;
		case PF_RDTSCP_INSTRUCTION_AVAILABLE:     FCHECK(leaf81, EDX, CPUID_EDX_RDTSCP);   break;
		case PF_RDPID_INSTRUCTION_AVAILABLE:      FCHECK(leaf7,  ECX, CPUID_ECX_RDPID);    break;
		
		case PF_MONITORX_INSTRUCTION_AVAILABLE:   FCHECK(leaf81, ECX, CPUID_ECX_MONITORX); break;
		case PF_SSSE3_INSTRUCTIONS_AVAILABLE:     FCHECK(leaf1,  ECX, CPUID_ECX_SSSE3);    break;
		case PF_SSE4_1_INSTRUCTIONS_AVAILABLE:    FCHECK(leaf1,  ECX, CPUID_ECX_SSE4_1);   break;
		case PF_SSE4_2_INSTRUCTIONS_AVAILABLE:    FCHECK(leaf1,  ECX, CPUID_ECX_SSE4_2);   break;
		case PF_AVX_INSTRUCTIONS_AVAILABLE:       FCHECK(leaf1,  ECX, CPUID_ECX_AVX);      break;
		case PF_AVX2_INSTRUCTIONS_AVAILABLE:      FCHECK(leaf7,  EBX, CPUID_EBX_AVX2);     break;
		case PF_AVX512F_INSTRUCTIONS_AVAILABLE:   FCHECK(leaf7,  EBX, CPUID_EBX_AVX512_F); break;

		/* NON x86 */
		case PF_PPC_MOVEMEM_64BIT_OK:
		case PF_ALPHA_BYTE_INSTRUCTIONS:
		case PF_ERMS_AVAILABLE:
		case PF_ARM_VFP_32_REGISTERS_AVAILABLE:
		case PF_ARM_NEON_INSTRUCTIONS_AVAILABLE:
		case PF_ARM_DIVIDE_INSTRUCTION_AVAILABLE:
		case PF_ARM_64BIT_LOADSTORE_ATOMIC:
		case PF_ARM_EXTERNAL_CACHE_AVAILABLE:
		case PF_ARM_FMAC_INSTRUCTIONS_AVAILABLE:
		case PF_ARM_V8_INSTRUCTIONS_AVAILABLE:
		case PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE:
		case PF_ARM_V8_CRC32_INSTRUCTIONS_AVAILABLE:
		case PF_ARM_V81_ATOMIC_INSTRUCTIONS_AVAILABLE:
		case PF_ARM_V82_DP_INSTRUCTIONS_AVAILABLE:
		case PF_ARM_V83_JSCVT_INSTRUCTIONS_AVAILABLE:
			break;
	}
	
	return FALSE;
}

#endif /* __IPFP_H__INCLUDED__ */
