#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "smp9x.h"

DWORD WINAPI thread_main(LPVOID lpParameter)
{
	volatile DWORD lock;
	smp9x_thread_elevate(&lock, SMP_MODE_MANUAL);
	char msg[256];
	
	DWORD thread_id = 0;
	DWORD thread_id_smp = 0;
	HANDLE thread_handle = NULL;
	HANDLE thread_handle_smp = NULL;
	
	thread_id = smp9x_GetCurrentThreadId();
	thread_handle = smp9x_GetCurrentThread();
	
	smp9x_thread_fly();
	thread_id_smp = smp9x_GetCurrentThreadId();
	thread_handle_smp = smp9x_GetCurrentThread();
	smp9x_thread_land();
	
	sprintf(msg, "GetCurrentThreadId: %lX | %lX\nGetCurrentThread: %p | %p",
		thread_id_smp,
		thread_id,
		thread_handle_smp,
		thread_handle
	);
	
	MessageBoxA(NULL, msg, "Result", MB_OK);

	return 0;
}

int main(int argc, char **argv)
{
	smp9x_init();
	DWORD id = 0;
	
	HANDLE th = CreateThread(NULL, 0, thread_main, NULL, 0, &id);
	
	WaitForSingleObject(th, INFINITE);
	
	return EXIT_SUCCESS;
}
