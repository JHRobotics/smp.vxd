#ifndef __SMPAPI_H__INCLUDED__
#define __SMPAPI_H__INCLUDED__

#define DIOC_SMP_CPU_COUNT 2
#define DIOC_SMP_GET_ADDRESS 3
#define DIOC_SMP_ELEVATE 4

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

#define SMP_VXD "smp.vxd"

#ifndef VXD_DEVICE_ID

/* WINAPI replacements */
VOID WINAPI smp9x_GetSystemInfo(LPSYSTEM_INFO lpSystemInfo);
HANDLE WINAPI smp9x_CreateThread(
	LPSECURITY_ATTRIBUTES   lpThreadAttributes,
	SIZE_T                  dwStackSize,
	LPTHREAD_START_ROUTINE  lpStartAddress,
  LPVOID                  pParameter,
  DWORD                   dwCreationFlags,
  LPDWORD                 lpThreadId);
DWORD WINAPI smp9x_GetCurrentProcessorNumber();

#endif

#endif /* __SMPAPI_H__INCLUDED__ */
