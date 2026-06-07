#include <windows.h>
#include "smp9x.h"
#include <stdio.h>

extern DWORD smp9x_address_fly;
extern DWORD smp9x_address_land;

//DWORD __mb_cur_max_smp = 0;
//FILE _iob_smp[_IOB_ENTRIES];
//FILE *_iob_smp[_IOB_ENTRIES];

void crt_fixer()
{
	//__mb_cur_max_smp = __mb_cur_max;
	//memcpy(_iob_smp, _iob, sizeof(FILE)*_IOB_ENTRIES);
}

BOOL WINAPI DllMain(
	HINSTANCE hinstDLL,  // handle to DLL module
	DWORD fdwReason,     // reason for calling function
	LPVOID lpvReserved)  // reserved
{
	// Perform actions based on the reason for calling.
	switch(fdwReason) 
	{ 
		case DLL_PROCESS_ATTACH:
			crt_fixer();
			smp9x_init();
			break;
		case DLL_THREAD_ATTACH:
			break;
		case DLL_THREAD_DETACH:
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
