#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "smp9x.h"

static char buffer[4096];

void gprintf(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  vsprintf(buffer, fmt, args);
  va_end(args);
  
  MessageBox(NULL, buffer, "Info", MB_OK);
}

int main(int argc, char **argv)
{
	HANDLE vxd = CreateFileA("\\\\.\\smp.vxd", 0, 0, 0, CREATE_NEW, FILE_FLAG_DELETE_ON_CLOSE, 0);
	if(vxd != INVALID_HANDLE_VALUE)
	{
		DWORD cpu_count = -1;
		if(DeviceIoControl(vxd, DIOC_SMP_CPU_COUNT, NULL, 0, &cpu_count, sizeof(DWORD),
			NULL, NULL))
		{
			gprintf("DIOC_SMP_CPU_COUNT = %X", cpu_count);
		}
		else
		{
			gprintf("DIOC_SMP_CPU_COUNT failed");
		}
		
		CloseHandle(vxd);
	}
	else
	{
		gprintf("CreateFileA('smp.vxd') failure\n");
	}
}
