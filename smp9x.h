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
#ifndef __SMPAPI_H__INCLUDED__
#define __SMPAPI_H__INCLUDED__

#define DIOC_SMP_CPU_COUNT 2
#define DIOC_SMP_GET_ADDRESS 3
#define DIOC_SMP_ELEVATE 4
#define DIOC_SMP_CPU_FEATURES 5
#define DIOC_SMP_CPU_STATS 6

#define SMP_OFFSET_IS_BSP     0x200
#define SMP_OFFSET_CPUINDEX   0x280
#define SMP_OFFSET_FLY        0x300
#define SMP_OFFSET_LAND       0x400
#define SMP_OFFSET_REATTACH   0x500
#define SMP_OFFSET_TRAMPOLINE 0x580
#define SMP_OFFSET_INT        0x600

#define SMP_FN_BLOCK_SIZE     0x100

#define SMP_MODE_SYSTEM  0
#define SMP_MODE_MANUAL  1
#define SMP_MODE_AUTORUN 2

void __cdecl smp9x_init();
void __cdecl smp9x_close();
void __cdecl smp9x_thread_elevate(volatile DWORD *lock_ptr, int mode);
void __cdecl smp9x_thread_fly();
void __cdecl smp9x_thread_land();
int  __cdecl smp9x_cpus();

#define SMP_CPU_SMPVXD      0x01 /* smp.vxd is present and loaded */
#define SMP_CPU_SMPVXD_HTT  0x02 /* smp.vxd multi-cpu present and working */
#define SMP_CPU_MMX         0x04 /* CPU and OS supports MMX */
#define SMP_CPU_SSE         0x08 /* CPU and OS supports SSE */
#define SMP_CPU_AVX         0x10 /* CPU and OS supports AVX */
#define SMP_CPU_AVX512      0x20 /* CPU ans OS supports AVX512 */

#define SMP_STAT_CPU 0
#define SMP_STAT_APIC_ID 1
#define SMP_STAT_THREADS 2

BOOL __cdecl smp9x_stats(int type, int unit, DWORD *ret);
typedef BOOL (__cdecl *smp9x_stats_t)(int type, int unit, DWORD *ret);

#define SMP_VXD "smp.vxd"
#define SMP_DLL "libsmp.dll"

#ifndef VXD_DEVICE_ID

/* WINAPI replacements */
VOID WINAPI smp9x_GetSystemInfo(LPSYSTEM_INFO lpSystemInfo);
typedef int (WINAPI *smp9x_GetSystemInfo_t)(LPSYSTEM_INFO lpSystemInfo);

HANDLE WINAPI smp9x_CreateThread(
	LPSECURITY_ATTRIBUTES   lpThreadAttributes,
	SIZE_T                  dwStackSize,
	LPTHREAD_START_ROUTINE  lpStartAddress,
  LPVOID                  pParameter,
  DWORD                   dwCreationFlags,
  LPDWORD                 lpThreadId);

DWORD WINAPI smp9x_GetCurrentProcessorNumber();
BOOL WINAPI smp9x_IsProcessorFeaturePresent(DWORD ProcessorFeature);
typedef BOOL (WINAPI *smp9x_IsProcessorFeaturePresent_t)(DWORD ProcessorFeature);

#endif

#endif /* __SMPAPI_H__INCLUDED__ */
