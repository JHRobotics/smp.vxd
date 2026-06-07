#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "smp9x.h"

DWORD WINAPI thread_main(LPVOID lpParameter)
{
	volatile DWORD lock;
	/* ^ lock is per thread, simplest option is put it on thread stack */
	smp9x_thread_elevate(&lock, SMP_MODE_MANUAL);
	smp9x_thread_fly();

	printf("Hello from thread!\n");

	smp9x_thread_land();

	return 0;
}

int main(int argc, char **argv)
{
	smp9x_init();
	DWORD id = 0; /* id param is mandatory on win9x, even if no use for it */
	
	HANDLE th = CreateThread(NULL, 0, thread_main, NULL, 0, &id);
	
	printf("Hello from main!\n");
	WaitForSingleObject(th, INFINITE);
	
	return EXIT_SUCCESS;
}
