#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <winperf.h>

#include "counter.h"

#include "smp9x.h"
#include "perf.h"
#include "smpcon.h"

#include "nocrt.h"

#define BUFFERSIZE (1024*1024)
#define INCREMENT (512*1024)

#define CPU_MAX 256

DWORD bufferSize = 0;
PPERF_DATA_BLOCK perfDataBlock = NULL;

typedef struct cpu_nt_stat
{
	LONGLONG last_counter;
	LONGLONG last_timer;
	DWORD    result;
} cpu_nt_stat_t;

cpu_nt_stat_t cpu_stats[CPU_MAX] = {};

DWORD cpu_perf_index = 6;

#if 0
BOOL perf_nt_names()
{
	char regname[128];
	HKEY hKey;
	DWORD lid = MAKELANGID(LANG_ENGLISH, SUBLANG_NEUTRAL);

	sprintf(regname, "Software\\Microsoft\\Windows NT\\CurrentVersion\\Perflib\\%03X", lid);

	if(RegOpenKeyExA(HKEY_LOCAL_MACHINE, regname, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
	{
		DWORD dwType, dwSize = 0;
		if(RegQueryValueExA(hKey, "Counter", NULL, &dwType, NULL, &dwSize) == ERROR_SUCCESS)
		{
			BYTE *namebuffer = malloc(dwSize+1);
			if(namebuffer != NULL)
			{
				if(RegQueryValueExA(hKey, "Counter", NULL, &dwType, namebuffer, &dwSize) == ERROR_SUCCESS)
				{
					
				}
			}
		}
		RegCloseKey(hKey);
	}
	
	return TRUE;
}
#endif

typedef BOOL (WINAPI * GetThreadPriorityBoostF)(HANDLE hThread, PBOOL pDisablePriorityBoost);
BOOL is_nt()
{
	HANDLE h = GetModuleHandleA("kernel32.dll");
	if(h)
	{
		GetThreadPriorityBoostF GetThreadPriorityBoostH = (GetThreadPriorityBoostF)GetProcAddress(h, "GetThreadPriorityBoost");
		if(GetThreadPriorityBoostH != NULL)
		{
			BOOL junk;
			if(GetThreadPriorityBoostH(GetCurrentThread(), &junk))
			{
				return TRUE;	
			}
			
			if(GetLastError() != ERROR_CALL_NOT_IMPLEMENTED)
			{
				return TRUE;
			}
		}
	}
	
	return FALSE;
}

static void w2a(const wchar_t *wsrc, char *dst, size_t dstmax)
{
	while(*wsrc != 0 && dstmax > 1)
	{
		if(*wsrc < 0x80)
		{
			*dst = (char)(*wsrc);
		}
		else
		{
			*dst = '?';
		}
		dst++;
		wsrc++;
		dstmax--;
	}

	*dst = '\0';
}

static BOOL perf_nt_read_cpu()
{
	PPERF_OBJECT_TYPE perfObj = (PPERF_OBJECT_TYPE)((BYTE*)perfDataBlock + perfDataBlock->HeaderLength);
	PPERF_COUNTER_DEFINITION perfCounterDef = NULL;
	DWORD i, j, k;
	
	//printf("NumObjectTypes = %d\n", perfDataBlock->NumObjectTypes);
	for(j = 0; j < perfDataBlock->NumObjectTypes; j++)
	{
		perfCounterDef = (PPERF_COUNTER_DEFINITION)((BYTE*)perfObj + perfObj->HeaderLength);
		for(i = 0; i < perfObj->NumCounters; i++)
		{
			//printf("%d: perfCounterDef->CounterOffset = %d\n", j, perfCounterDef->CounterOffset);
			if(perfCounterDef->CounterNameTitleIndex == cpu_perf_index) // ...
			{
				if((perfCounterDef->CounterType & 0x00300300) == 0x100100)
				{
					goto /*dingo*/ found_cpu_perf;
				}
			}
			perfCounterDef++;
		}
		perfObj = (PPERF_OBJECT_TYPE)(((BYTE*)perfObj) + perfObj->TotalByteLength);
	}
	return FALSE;
	
	found_cpu_perf:
	//printf("size=%d, type=0x%X, scale=%d\n", perfCounterDef->CounterSize, perfCounterDef->CounterType, perfCounterDef->DefaultScale);
	
	if(perfObj->NumInstances == 0)
	{ 
		// not usable
	}
	else
	{
		PPERF_INSTANCE_DEFINITION perfInst = (PPERF_INSTANCE_DEFINITION)((BYTE*)perfObj + perfObj->DefinitionLength);
		for(k = 0; k < (DWORD)perfObj->NumInstances; k++)
		{
			PPERF_COUNTER_BLOCK block = (PPERF_COUNTER_BLOCK)((BYTE*)perfInst + perfInst->ByteLength);
			char uname[256];
			wchar_t *wname = (wchar_t*)((BYTE*)perfInst + perfInst->NameOffset);
			//WideCharToMultiByte(CP_ACP, 0, wname, -1, uname, sizeof(uname), NULL, NULL);
			w2a(wname, uname, sizeof(uname));
			if(strcmp(uname, "_Total") != 0)
			{
				int index = atoi(uname);
				if(index < CPU_MAX)
				{
					LONGLONG v = *((LONGLONG*)(((BYTE*)block) + perfCounterDef->CounterOffset));
					LONGLONG t = perfDataBlock->PerfTime.QuadPart;
					if(cpu_stats[index].last_timer != 0)
					{
						double d_v = v - cpu_stats[index].last_counter;
						double d_t = t - cpu_stats[index].last_timer;
						
						if(t > 0)
						{
							double r = (d_v / d_t);
							if(r > 1.0) r = 1.0;
							
							cpu_stats[index].result = 100 - r*100;
						}
						else
						{
							cpu_stats[index].result = 0;
						}
					}
					cpu_stats[index].last_counter = v;
					cpu_stats[index].last_timer = t;
				}
			}
			perfInst = (PPERF_INSTANCE_DEFINITION)((BYTE*)block + block->ByteLength);
		}
	}
	
	return TRUE;
}

BOOL perf_nt_read()
{
	if(bufferSize == 0)
	{
		bufferSize = BUFFERSIZE;
		perfDataBlock = malloc(bufferSize);
		
		if(perfDataBlock == NULL)
		{
			return FALSE;
		}
	}
	
	while(RegQueryValueExA(HKEY_PERFORMANCE_DATA, "Global", NULL, NULL, (LPBYTE)perfDataBlock, &bufferSize) == ERROR_MORE_DATA)
	{
		bufferSize += INCREMENT;
		perfDataBlock = realloc(perfDataBlock, bufferSize);
		//printf("buffer: %u\n", bufferSize);
		if(perfDataBlock == NULL)
		{
			return FALSE;
		}
	}
	
	//printf("readed: %u\n", bufferSize);
	
	return perf_nt_read_cpu();
}

BOOL perf_cpu_load(int cpu_index, DWORD *result)
{
	if(cpu_index >= CPU_MAX) return FALSE;
	
	if(cpu_stats[cpu_index].last_timer != 0)
	{
		if(result != NULL)
		{
			*result = cpu_stats[cpu_index].result;
		}
		
		return TRUE;
	}
	
	return FALSE;
}

static BOOL fecher_run = FALSE;
static HANDLE fetcher_handle = NULL;

static DWORD WINAPI fetcher_nt_thread(LPVOID lpParameter)
{
	DWORD sleep_time = (DWORD)lpParameter;
	while(fecher_run)
	{
		perf_nt_read();
		Sleep(sleep_time);
	}
	
	return 0;
}

static DWORD WINAPI fetcher_libsmp_thread(LPVOID lpParameter)
{
	DWORD sleep_time = (DWORD)lpParameter;
	while(fecher_run)
	{
		int n = 0;
		DWORD data;
		LONGLONG ticks = GetTickCount();
		while(smpcon_stats(SMP_STAT_CPU, n, &data))
		{
			//printf("%d - %X\n", n, data);
			cpu_stats[n].last_timer = ticks;
			cpu_stats[n].result = data;
			n++;
		}
		
		Sleep(sleep_time);
	}
	
	return 0;
}

static DWORD WINAPI fetcher_9x_thread(LPVOID lpParameter)
{
	DWORD sleep_time = (DWORD)lpParameter;
	HKEY hKey, hKey2;

	/* read StartStat to Start Collecting */
#if 0
	if(RegOpenKey(HKEY_DYN_DATA, "PerfStats\\StartStat", &hKey2) == ERROR_SUCCESS)
	{
		DWORD start_cb = 4, start_type = 0, start_data = 0;
		if(RegQueryValueEx(hKey2, "KERNEL\\CPUUsage", 0, &start_type, (LPBYTE)&start_data, &start_cb) == ERROR_SUCCESS)
		{
			printf("start success\n");
		}
		RegCloseKey(hKey2);
	}
#endif
	
	while(fecher_run)
	{
		LONGLONG ticks;
		Sleep(sleep_time);
		
		ticks = GetTickCount();
		if(RegOpenKey(HKEY_DYN_DATA, "PerfStats\\StartStat", &hKey2) == ERROR_SUCCESS)
		{
			DWORD start_cb = 4, start_type = 0, start_data = 0;
			if(RegQueryValueEx(hKey2, "KERNEL\\CPUUsage", 0, &start_type, (LPBYTE)&start_data, &start_cb) == ERROR_SUCCESS)
			{
				if(RegOpenKey(HKEY_DYN_DATA, "PerfStats\\StatData", &hKey) == ERROR_SUCCESS)
				{
					DWORD type;
					DWORD cbData = 4;
					DWORD data;
					if(RegQueryValueEx(hKey, "KERNEL\\CPUUsage", 0, &type, (LPBYTE)&data, &cbData) == ERROR_SUCCESS)
					{
						cpu_stats[0].last_timer = ticks;
						cpu_stats[0].result = data;
						//printf("Update: %X\n", data);
					}
					RegCloseKey(hKey);
				}
			}
			RegCloseKey(hKey2);
		}
	}

	return 0;
}

BOOL perf_fetcher_start(DWORD delay_ms)
{
	DWORD id;
	fecher_run = TRUE;
	
	if(is_nt())
	{
		//printf("using nt stats\n");
		fetcher_handle = CreateThread(NULL, 0, fetcher_nt_thread, (void*)delay_ms, 0, &id);
	}
	else
	{
		if(smpcon_usable)
		{
			//printf("using smp.vxd stats\n");
			fetcher_handle = CreateThread(NULL, 0, fetcher_libsmp_thread, (void*)delay_ms, 0, &id);
		}
		else
		{
			//printf("using 9x stats\n");
			fetcher_handle = CreateThread(NULL, 0, fetcher_9x_thread, (void*)delay_ms, 0, &id);
		}
	}
	
	if(fetcher_handle != NULL)
	{
		return TRUE;
	}
	return FALSE;
}

void perf_fetcher_stop()
{
	fecher_run = FALSE;
	if(fetcher_handle != NULL)
	{
		WaitForSingleObject(fetcher_handle, INFINITE);
	}
}

