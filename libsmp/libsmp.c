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
#include <windows.h>
#include <stdint.h>
#include "smp9x.h"
#include "cpuid_smp.h"
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
			if(cpuid(0, NULL) == 0)
			{
				/* cannot continue without CPUID */
				return FALSE;
			}
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
