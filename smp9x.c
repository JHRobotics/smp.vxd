#include <windows.h>
#include "smp9x.h"

static DWORD smp_address = 0;
static DWORD cpu_count = 0;
static HANDLE smp_vxd = INVALID_HANDLE_VALUE;

void __cdecl smp9x_init()
{
	smp_vxd = CreateFileA("\\\\.\\" SMP_VXD, 0, 0, 0, CREATE_NEW, FILE_FLAG_DELETE_ON_CLOSE, 0);
	if(smp_vxd != INVALID_HANDLE_VALUE)
	{
		if(DeviceIoControl(smp_vxd, DIOC_SMP_CPU_COUNT, NULL, 0, &cpu_count, sizeof(DWORD),
			NULL, NULL))
		{
			if(cpu_count >= 2)
			{
				if(DeviceIoControl(smp_vxd, DIOC_SMP_GET_ADDRESS,
					NULL, 0, &smp_address, sizeof(DWORD),
					NULL, NULL))
				{
					return; /* success */
				}
				smp_address = 0;
			}
		}
		CloseHandle(smp_vxd);
	}
}

typedef int (__cdecl *smp9x_is_bsp_t)(void);
typedef void (__cdecl *smp9x_fly_t)(void);
typedef void (__cdecl *smp9x_land_t)(void);
typedef void (__cdecl *smp9x_reattach_t)(void);

void __cdecl smp9x_close()
{
	smp_address = 0;
	if(smp_vxd != INVALID_HANDLE_VALUE)
	{
		CloseHandle(smp_vxd);
		smp_vxd = INVALID_HANDLE_VALUE;
	}
}

void __cdecl smp9x_thread_idle_proc(volatile DWORD *lock_ptr)
{
	smp9x_reattach_t reattach = (smp9x_reattach_t)(smp_address + SMP_OFFSET_REATTACH);
	for(;;)
	{
		if(lock_ptr != NULL)
		{
			while(*lock_ptr != 0)
			{
				Sleep(0);
			}
		}
		reattach();
	}
	/* never return! */
}

void __cdecl smp9x_thread_elevate(volatile DWORD *lock_ptr)
{
	if(smp_address != 0)
	{
		smp9x_is_bsp_t is_bsp = (smp9x_is_bsp_t)(smp_address+SMP_OFFSET_IS_BSP);
		if(is_bsp())
		{
			DWORD params[2];
			params[0] = (DWORD)smp9x_thread_idle_proc;
			params[1] = (DWORD)lock_ptr;
			
			DeviceIoControl(smp_vxd, DIOC_SMP_ELEVATE, params, sizeof(params), NULL, 0, NULL, NULL);
		}
	}
}

void __cdecl smp9x_thread_fly()
{
	if(smp_address != 0)
	{
		smp9x_fly_t fly = (smp9x_fly_t)(smp_address + SMP_OFFSET_FLY);
		fly();
	}
}

void __cdecl smp9x_thread_land()
{
	if(smp_address != 0)
	{
		smp9x_land_t land = (smp9x_land_t)(smp_address + SMP_OFFSET_LAND);
		land();
	}
}

int  __cdecl smp9x_cpus()
{
	if(cpu_count == 0)
	{
		SYSTEM_INFO si;
		GetSystemInfo(&si);
		return si.dwNumberOfProcessors;
	}
	
	return cpu_count;
}

