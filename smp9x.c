#include <windows.h>
#include <stdio.h>
#include <process.h>
#include "smp9x.h"

typedef int (__cdecl *smp9x_is_bsp_t)(void);
typedef void (__cdecl *smp9x_fly_t)(void);
typedef void (__cdecl *smp9x_land_t)(void);
typedef void (__cdecl *smp9x_reattach_t)(void);
typedef unsigned (__cdecl *smp9x_cpuindex_t)(void);


typedef DWORD (WINAPI *smp9x_winthread_main_std_t)(LPVOID lpParameter);
typedef void (__cdecl *smp9x_winthread_main_bt_t)(void *);
typedef unsigned (__stdcall *smp9x_winthread_btx_t)(void *);

#define TT_STD 0
#define TT_BT 1
#define TT_BTX 2

typedef DWORD (WINAPI *GetCurrentProcessorNumber_t)(void);

static DWORD smp9x_address = 0;
static DWORD cpu_count = 0;
static HANDLE smp_vxd = INVALID_HANDLE_VALUE;
//			 DWORD smp9x_address_fly = 0;
//			 DWORD smp9x_address_land = 0;
static GetCurrentProcessorNumber_t gcpn = NULL;


static CRITICAL_SECTION cs = {};
static HANDLE memheap = NULL;

typedef struct smp9x_thread
{
	union
	{
		smp9x_winthread_main_std_t std;
		smp9x_winthread_main_bt_t bt;
		smp9x_winthread_btx_t btx;
	} tmain;
	DWORD tmain_type;
	void *param;
	DWORD  threadId;
	HANDLE threadHandle;
	struct smp9x_thread *prev;
} smp9x_thread_t;

static smp9x_thread_t *threads = NULL;

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
					NULL, 0, &smp9x_address, sizeof(DWORD),
					NULL, NULL))
				{
					//smp9x_address_fly = smp9x_address+SMP_OFFSET_FLY;
					//smp9x_address_land = smp9x_address+SMP_OFFSET_LAND;
					goto dingo; /* success */
				} // else{printf("E:address\n");}
				smp9x_address = 0;
			} // else{printf("E:cpucount 2\n");}
		} // else{printf("E:cpucount\n");}
		CloseHandle(smp_vxd);
	} //else{printf("E:CreateFileA\n");}
	
	dingo:
	memheap = HeapCreate(0, 0, 0);
	if(memheap == NULL)
	{
		memheap = GetProcessHeap();	
	}
	
	HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
	if(kernel32)
	{
		gcpn = (GetCurrentProcessorNumber_t)GetProcAddress(kernel32, "GetCurrentProcessorNumber");
	}
	
	InitializeCriticalSection(&cs);
}


void __cdecl smp9x_close()
{
	smp9x_address = 0;
	if(smp_vxd != INVALID_HANDLE_VALUE)
	{
		CloseHandle(smp_vxd);
		smp_vxd = INVALID_HANDLE_VALUE;
	}
}

void __cdecl smp9x_thread_idle_proc(volatile DWORD *lock_ptr)
{
	smp9x_reattach_t reattach = (smp9x_reattach_t)(smp9x_address + SMP_OFFSET_REATTACH);
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

void __cdecl smp9x_thread_elevate(volatile DWORD *lock_ptr, int mode)
{
	if(smp9x_address != 0)
	{
		smp9x_is_bsp_t is_bsp = (smp9x_is_bsp_t)(smp9x_address+SMP_OFFSET_IS_BSP);
		if(is_bsp())
		{
			DWORD params[3];
			params[0] = (DWORD)smp9x_thread_idle_proc;
			params[1] = (DWORD)lock_ptr;
			params[2] = mode;
			
			DeviceIoControl(smp_vxd, DIOC_SMP_ELEVATE, params, sizeof(params), NULL, 0, NULL, NULL);
		}
	}
}

void __cdecl smp9x_thread_fly()
{
	if(smp9x_address != 0)
	{
		smp9x_fly_t fly = (smp9x_fly_t)(smp9x_address + SMP_OFFSET_FLY);
		fly();
	}
}

void __cdecl smp9x_thread_land()
{
	if(smp9x_address != 0)
	{
		smp9x_land_t land = (smp9x_land_t)(smp9x_address + SMP_OFFSET_LAND);
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

/*
 *
 * WINAPI bindings
 *
 */
static smp9x_thread_t *thread_alloc()
{
	smp9x_thread_t *t = HeapAlloc(memheap, 0, sizeof(smp9x_thread_t));
	memset(t, 0, sizeof(smp9x_thread_t));
	if(t != NULL)
	{
		EnterCriticalSection(&cs);
		t->prev = threads;
		threads = t;
		LeaveCriticalSection(&cs);
	}
	return t;
}

void smp9x_thread_info_attach()
{
	/* only save our threads */
}

void smp9x_thread_info_detach()
{
	smp9x_thread_t **t;
	HANDLE h;
	DWORD id;
	
	EnterCriticalSection(&cs);
	
	h  = GetCurrentThread();
	id = GetCurrentThreadId();
	t = &threads;
	while(*t != NULL)
	{
		if((*t)->threadHandle == h || (*t)->threadId == id)
		{
			smp9x_thread_t *garbage = *t;
			*t = garbage->prev;
			HeapFree(memheap, 0, garbage);
			continue;
		}
		t = &((*t)->prev);
	}
	
	LeaveCriticalSection(&cs);
}

static DWORD WINAPI smp9x_winthread_main(LPVOID lpParameter)
{
	volatile DWORD lock;
	smp9x_thread_t *mythread;
	DWORD rc = 0;
	
	smp9x_thread_elevate(&lock, SMP_MODE_AUTORUN);
	smp9x_thread_fly();
	
	mythread = lpParameter;
	if(mythread != NULL)
	{
		switch(mythread->tmain_type)
		{
			case TT_STD:
				rc = mythread->tmain.std(mythread->param);
				break;
			case TT_BT:
				mythread->tmain.bt(mythread->param);
				break;
			case TT_BTX:
				rc = mythread->tmain.btx(mythread->param);
				break;
		}
	}
	smp9x_thread_land();
	return rc;
}

HANDLE WINAPI smp9x_CreateThread(
	LPSECURITY_ATTRIBUTES   lpThreadAttributes,
	SIZE_T                  dwStackSize,
	LPTHREAD_START_ROUTINE  lpStartAddress,
  LPVOID                  pParameter,
  DWORD                   dwCreationFlags,
  LPDWORD                 lpThreadId)
{
	smp9x_thread_t *t = thread_alloc();
	
	if(t != NULL)
	{
		t->tmain.std  = lpStartAddress;
		t->tmain_type = TT_STD;
		t->param      = pParameter;

		t->threadHandle = CreateThread(lpThreadAttributes, dwStackSize, smp9x_winthread_main, t, dwCreationFlags, &(t->threadId));
		if(t->threadHandle != NULL)
		{
			if(lpThreadId != NULL)
			{
				*lpThreadId = t->threadId;
			}
			
			return t->threadHandle;
		}
	}
	
	return NULL;
}

VOID WINAPI smp9x_ExitThread(DWORD dwExitCode)
{
	smp9x_thread_land();
	ExitThread(dwExitCode);
	/* do not call fly again */
}

VOID WINAPI smp9x_GetSystemInfo(LPSYSTEM_INFO lpSystemInfo)
{
	if(lpSystemInfo != NULL)
	{
		GetSystemInfo(lpSystemInfo);
		if(cpu_count != 0)
		{
			lpSystemInfo->dwNumberOfProcessors = cpu_count;
		}
	}
}

DWORD WINAPI smp9x_GetCurrentProcessorNumber()
{
	if(smp9x_address != 0)
	{
		smp9x_cpuindex_t cpuindex = (smp9x_cpuindex_t)(smp9x_address + SMP_OFFSET_CPUINDEX);
		return cpuindex()+1;
	}
	
	if(gcpn != NULL)
	{
		return gcpn();
	}
	
	return 1;
}

DWORD WINAPI smp9x_GetThreadId(HANDLE *h)
{
	smp9x_thread_t *t;
	DWORD rc = 0;
	
	EnterCriticalSection(&cs);
	for(t = threads; t != NULL; t = t->prev)
	{
		if(t->threadHandle == h)
		{
			rc = t->threadId;
			break;
		}
	}
	LeaveCriticalSection(&cs);
	
	return rc;
}

BOOL WINAPI smp9x_InitOnceExecuteOnce(
	PINIT_ONCE    InitOnce,
  PINIT_ONCE_FN InitFn,
  PVOID         Parameter,
  LPVOID        *Context
)
{
	EnterCriticalSection(&cs);
	if(InitOnce->Ptr == 0)
	{
		if(InitFn(InitOnce, Parameter, Parameter))
		{
			InitOnce->Ptr = (void*)(~0);
		}
	}
	LeaveCriticalSection(&cs);
	
	if(InitOnce->Ptr != 0)
	{
		return TRUE;
	}
	return FALSE;
}


VOID WINAPI smp9x_InitializeConditionVariable(PCONDITION_VARIABLE ConditionVariable)
{
	ConditionVariable->Ptr = (void*)~0;
}

BOOL WINAPI smp9x_SleepConditionVariableCS(
	PCONDITION_VARIABLE ConditionVariable,
	PCRITICAL_SECTION   CriticalSection,
	DWORD               dwMilliseconds)
{
	return TRUE;
}

VOID WINAPI smp9x_WakeAllConditionVariable(PCONDITION_VARIABLE ConditionVariable){}
VOID WINAPI smp9x_WakeConditionVariable(PCONDITION_VARIABLE ConditionVariable){}

BOOL WINAPI smp9x_SwitchToThread()
{
	Sleep(0);
	return TRUE;
}

/*
 * _beginthread
 *
 */
uintptr_t __cdecl smp9x__beginthread(
   _beginthread_proc_type start_address,
   unsigned stack_size,
   void *arglist
)
{
	smp9x_thread_t *t = thread_alloc();
	
	if(t != NULL)
	{
		t->tmain.bt   = start_address;
		t->tmain_type = TT_BT;
		t->param      = arglist;

		t->threadHandle = CreateThread(NULL, stack_size, smp9x_winthread_main, t, 0, &(t->threadId));
		if(t->threadHandle != INVALID_HANDLE_VALUE)
		{
			return (uintptr_t)t->threadHandle;
		}
	}
	
	return (uintptr_t)NULL;
}

/*
 * _beginthreadex
 *
 */
uintptr_t __cdecl smp9x__beginthreadex(
   void *security,
   unsigned stack_size,
   _beginthreadex_proc_type start_address,
   void *arglist,
   unsigned initflag,
   unsigned *thrdaddr
)
{
	smp9x_thread_t *t = thread_alloc();
	
	if(t != NULL)
	{
		t->tmain.btx  = start_address;
		t->tmain_type = TT_BTX;
		t->param      = arglist;

		t->threadHandle = CreateThread((LPSECURITY_ATTRIBUTES)security, stack_size, smp9x_winthread_main, t, initflag, &(t->threadId));
		if(t->threadHandle != INVALID_HANDLE_VALUE)
		{
			if(thrdaddr != NULL)
			{
				*thrdaddr = t->threadId;
			}
			
			return (uintptr_t)t->threadHandle;
		}
	}
	
	return (uintptr_t)NULL;
}

void __cdecl smp9x__endthread(void)
{
	smp9x_ExitThread(0);
}

void __cdecl smp9x__endthreadex(unsigned _Retval)
{
	smp9x_ExitThread(_Retval);
}
