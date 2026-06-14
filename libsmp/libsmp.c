#include <windows.h>
#include "smp9x.h"
#include <stdio.h>

void smp9x_thread_info_attach();
void smp9x_thread_info_detach();

BOOL WINAPI DllMain(
	HINSTANCE hinstDLL,  // handle to DLL module
	DWORD fdwReason,     // reason for calling function
	LPVOID lpvReserved)  // reserved
{
	// Perform actions based on the reason for calling.
	switch(fdwReason) 
	{ 
		case DLL_PROCESS_ATTACH:
			smp9x_init();
			break;
		case DLL_THREAD_ATTACH:
			smp9x_thread_info_attach();
			break;
		case DLL_THREAD_DETACH:
			smp9x_thread_info_detach();
			break;
		case DLL_PROCESS_DETACH:
			if(lpvReserved != NULL)
			{
				break;
				// do not do cleanup if process termination scenario
			}
			break;
	}
	return TRUE;
}
