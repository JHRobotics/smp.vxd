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
#include "vxd.h"

void VXD_control();

/*
  VXD structure
  this variable must be in first address in code segment.
  In other cases VXD isn't loadable (WLINK bug?)
*/
DDB VXD_DDB = {
	NULL,                       // must be NULL
	DDK_VERSION,                // DDK_Version
	VXD_DEVICE_ID,              // Device ID
	VXD_MAJOR_VER,              // Major Version
	VXD_MINOR_VER,              // Minor Version
	NULL,
	VXD_DEVICE_NAME,
	Undefined_Init_Order,
	(DWORD)VXD_control,
	NULL,
	NULL,
	NULL,   // CS:IP of API entry point
	NULL,   // CS:IP of API entry point
	NULL,   // Reference data from real mode
	NULL,   // Pointer to service table
	NULL,   // Number of services
	NULL,   // DDB_Win32_Service_Table
	'Prev', // prev
	sizeof(DDB),
	'Rsv1',
	'Rsv2',
	'Rsv3',
};

DWORD ThisVM = 0;
uint8_t *smp_first_mb = NULL;
static BOOL smp_valid = FALSE;
static cpuid_result_t cpu_flags;

uint32_t xsave_flags = 0;
BOOL no_sys_fxsave = FALSE;

void Sys_Critical_Init_proc(){ }

void __declspec(naked) timeout_entry();


/* CommandTail: Address of the command tail retrieved from the program segment prefix (PSP) of VMM32.VXD. The first byte in the command tail specifies the length in bytes of the tail. 
*/
void __stdcall Device_Init_proc(DWORD vm, BYTE *command_tail)
{
	BOOL rc;
	cpuid_result_t cpuid_res;
	
	dbg_init();
	
	VMMCall(_Allocate_Device_CB_Area);
 	ThisVM = vm;
 	
 	memset(&cpu_flags, 0, sizeof(cpuid_result_t));
 	cpuid(0, &cpuid_res);
 	if(cpuid_res.regs.regEAX >= 1)
 	{
 		cpuid(1, &cpu_flags);
 	}
 	
 	if((cpu_flags.regs.regEDX & CPUID_EDX_APIC) != 0)
 	{
	 	smp_first_mb = (uint8_t*)_MapPhysToLinear(0, 1048576, 0);
		//alertf("Device_Init: %s\n", VXD_DDB.DDB_Name);
		
		ts_init();
		rc = smp_init();
		//alertf("SMP init status = %d\n", rc);
		if(rc != FALSE)
		{
			smp_switch_install();
			smp_valid = TRUE;
			alertf("SMP driver init success, CPUs=%d\n", cpu_count);
			
	
			tracef("sizeof(tdata_t) = %d, sizeof(CRS_32) = %d\n",
				sizeof(tdata_t), sizeof(CRS_32));
				
	#ifdef DEBUG
			Set_Global_Time_Out(1000, NULL, timeout_entry);
	#endif
		}
	}
}

void __stdcall Device_Dynamic_Init(DWORD vm)
{
	VMMCall(_Allocate_Device_CB_Area);
 	ThisVM = vm;
	dbg_printf("Device_Dynamic_Init: %s\n", VXD_DDB.DDB_Name);
	dbg_printf("Please load this device staticaly\n");
}

void __stdcall Device_Dynamic_Exit(DWORD vm)
{
	dbg_printf("Device_Dynamic_Exit\n");
}

DWORD __stdcall Device_IO_Control(DWORD vmhandle, struct DIOCParams *params)
{
	DWORD rc = 1;
	DWORD *inBuf  = (DWORD*)params->lpInBuffer;
	DWORD *outBuf = (DWORD*)params->lpOutBuffer;
	
	dbg_printf("Device IO: %ld\n", params->dwIoControlCode);

	switch(params->dwIoControlCode)
	{
		case DIOC_OPEN:
			if(smp_valid)
			{
				rc = 0;
			}
			break;
		case DIOC_CLOSEHANDLE:
			rc = 0;
			break;
		case DIOC_SMP_CPU_COUNT:
			outBuf[0] = cpu_count;
			rc = 0;
			break;
		case DIOC_SMP_GET_ADDRESS:
			outBuf[0] = kernel_flat;
			rc = 0;
			break;
		case DIOC_SMP_ELEVATE:
			smp_elevate(inBuf[0], inBuf[1], inBuf[2]);
			rc = 0;
			break;
		// all others
	}
	
	return rc;
}

void __stdcall Device_Thread_Init(DWORD thread_handle)
{
	//dbg_printf("Created %lX\n", thread_handle);
	ts_thread_create(thread_handle);
}

void __stdcall Device_Thread_Destroy(DWORD thread_handle)
{
	ts_thread_t *ts = ts_thread_get(thread_handle);
	if(ts)
	{
		if(ts->smp_status != 0)
		{
			int cpu = ts->smp_apid;
			volatile uint32_t *sp = &(ttable[cpu].data->status);
			uint32_t s;
			do
			{
				atomic_lock(sp, &s);
			} while(s == S_BUSY);
			s = S_DIRCARD;
			smp_wakeup(cpu);
			atomic_unlock(sp, s);
		}
	}
	
	ts_thread_destroy(thread_handle);
	//dbg_printf("Destroyed %lX\n", thread_handle);
}

void __stdcall print_control_code(DWORD code)
{
	dbg_printf("VXD_control: %d\n", code);
}

void __declspec(naked) VXD_control()
{
	// eax = 0x00 - Sys Critical Init
	_asm {
		;pushad
		;push eax
		;call print_control_code
		;popad

		cmp eax,Sys_Critical_Init
		jnz control_1
			pushad
			call Sys_Critical_Init_proc
			popad
			clc
			ret
		control_1:
		cmp eax,Device_Init
		jnz control_2
			pushad
			push esi ; command tail
			push ebx ; VM handle
		  call Device_Init_proc
			popad
			clc
			ret
		control_2:
		cmp eax,Sys_Dynamic_Device_Init
		jnz control_4
			pushad
			push ebx ; VM handle
		  call Device_Dynamic_Init
		  popad
			clc
			ret
		control_4:
		cmp eax,Sys_Dynamic_Device_Exit
		jnz control_5
			pushad
			push ebx ; VM handle
		  call Device_Dynamic_Exit
		  popad
			clc
			ret
		control_5:
		cmp eax,W32_DEVICEIOCONTROL
		jnz control_6
			;jmp Device_IO_Control_entry
			push esi /* struct DIOCParams */
			push ebx /* VMHandle */
			call Device_IO_Control
			retn
		control_6:
		cmp eax,Thread_Init
		jnz control_7
			pushad
			push edi ; thread handle (tid)
			call Device_Thread_Init
			popad
			clc
			ret
		control_7:
		cmp eax,Destroy_Thread
		jnz control_8
			pushad
			push edi ; thread handle (tid)
			call Device_Thread_Destroy
			popad
			clc
			ret	
		control_8:
			clc
	  ret
	};
}

#ifdef DEBUG

void __stdcall timeout(DWORD vm, DWORD tardiness, DWORD refdata, PCRS_32 crs)
{
	int i;
	for(i = 0; i < MAX_CORES; i++)
	{
		if(ttable[i].data != NULL)
		{
			tracef("CPU#%d = %d ", i, ttable[i].data->status);
		}
	};
	tracef("\n");
	
	{
		for(i = 0; i < MAX_CORES; i++)
		{
			if(ttable[i].data != NULL)
			{
				PCRS_32 is = (PCRS_32)(ttable[i].data->init_state);
				PCRS_32 ps = (PCRS_32)(ttable[i].data->proc_state);
				tracef("CPU#%d: proc_state->Client_EFlags=%X proc_state->Client_EIP=%X\n",
					i, ps->Client_EFlags, ps->Client_EIP);
			}
		}
	}

	Set_Global_Time_Out(1000, NULL, timeout_entry);
}

void __declspec(naked) timeout_entry()
{
	_asm
	{
		push ebx ; vmhandle
		push ecx ; Tardiness
		push edx ; RefData
		push ebp ; crs
		call timeout
		retn
	};
}

#endif
