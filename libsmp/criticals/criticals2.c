#include <windows.h>
#include "smp9x.h"
#include <stdio.h>

BOOL smp9x_is_ap();
BOOL WINAPI TryEnterCriticalSection9x(LPCRITICAL_SECTION lpCriticalSection);

void WINAPI smp9x_InitializeCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
{
	BOOL was_ap = smp9x_is_ap();
	if(was_ap) smp9x_thread_land();
	InitializeCriticalSection(lpCriticalSection);
	if(was_ap) smp9x_thread_fly();
}


BOOL WINAPI smp9x_TryEnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
{
	smp9x_thread_land();
	return TryEnterCriticalSection9x(lpCriticalSection);
}

void WINAPI smp9x_EnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
{
	smp9x_thread_land();
	EnterCriticalSection(lpCriticalSection);
}

void WINAPI smp9x_LeaveCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
{
	smp9x_thread_land();
	LeaveCriticalSection(lpCriticalSection);
	smp9x_thread_fly();
}

BOOL WINAPI smp9x_InitializeCriticalSectionAndSpinCount(LPCRITICAL_SECTION lpCriticalSection, DWORD dwSpinCount)
{
	BOOL was_ap = smp9x_is_ap();
	if(was_ap) smp9x_thread_land();
	InitializeCriticalSection(lpCriticalSection);
	if(was_ap) smp9x_thread_fly();
	
	return TRUE;
}

void WINAPI smp9x_DeleteCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
{
	BOOL was_ap = smp9x_is_ap();
	if(was_ap) smp9x_thread_land();
	DeleteCriticalSection(lpCriticalSection);
	if(was_ap) smp9x_thread_fly();
}
