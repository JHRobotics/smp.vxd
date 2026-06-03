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

void Sys_Critical_Init_proc(){ }

void __declspec(naked) timeout_entry();

/* CommandTail: Address of the command tail retrieved from the program segment prefix (PSP) of VMM32.VXD. The first byte in the command tail specifies the length in bytes of the tail. 
*/
void __stdcall Device_Init_proc(DWORD vm, BYTE *command_tail)
{
	BOOL rc;
	VMMCall(_Allocate_Device_CB_Area);
 	ThisVM = vm;
 	smp_first_mb = (uint8_t*)_MapPhysToLinear(0, 1048576, 0);
	dbg_printf("Device_Init: %s\n", VXD_DDB.DDB_Name);
	ts_init();
	rc = smp_init();
	dbg_printf("SMP init status = %d\n", rc);
	if(rc != 0)
	{
		smp_switch_install();
	}
	
	{
		int i = 0;
		BYTE *p = (BYTE*)smp_ap_main;
		for(i = 0; i < 32; i++)
		{
			dbg_printf("%02X ", *p);
			p++;
		}
		dbg_printf("\n");
		
	}
	
	tinit(TERROR_COM2);
	terrorf(TERROR_COM2, "sizeof(tdata_t) = %d, sizeof(CRS_32) = %d\n",
		sizeof(tdata_t), sizeof(CRS_32)
	);
	
	Set_Global_Time_Out(1000, NULL, timeout_entry);
}

DWORD test_mem = 0;

void __stdcall Device_Dynamic_Init(DWORD vm)
{
	VMMCall(_Allocate_Device_CB_Area);
 	ThisVM = vm;
	dbg_printf("Device_Dynamic_Init: %s\n", VXD_DDB.DDB_Name);
	//rc = smp_init();
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
			rc = 0;
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
			smp_elevate(inBuf[0], inBuf[1]);
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

#if 0
void __declspec(naked) Device_IO_Control_entry()
{
	_asm {
		push esi /* struct DIOCParams */
		push ebx /* VMHandle */
		call Device_IO_Control
		retn
	}
}
#endif

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

void __stdcall timeout(DWORD vm, DWORD tardiness, DWORD refdata, PCRS_32 crs)
{
	int i;
	for(i = 0; i < MAX_CORES; i++)
	{
		if(ttable[i].data != NULL)
		{
			terrorf(TERROR_COM2, "CPU#%d = %d ", i, ttable[i].data->status);
		}
	};
	terrorf(TERROR_COM2, "\n");
	
	{
		for(i = 0; i < MAX_CORES; i++)
		{
			if(ttable[i].data != NULL)
			{
				PCRS_32 is = (PCRS_32)(ttable[i].data->init_state);
				PCRS_32 ps = (PCRS_32)(ttable[i].data->proc_state);
				terrorf(TERROR_COM2, "CPU#%d: init_state->Client_EIP=%X, proc_state->Client_EIP=%X\n",
					i, is->Client_EIP, ps->Client_EIP);
			}
		}
	}
	
#if 0
	for(i = 0; i < MAX_CORES; i++)
	{
		if(ttable[i].data != NULL)
		{
			terrorf(TERROR_COM2, "CPU %d GDT:\n", i);
			terrorf(TERROR_COM2, "%08X %08X, %08X %08X, %08X %08X\n",
				ttable[i].data->gdt[0], ttable[i].data->gdt[1],
				ttable[i].data->gdt[2], ttable[i].data->gdt[3],
				ttable[i].data->gdt[4], ttable[i].data->gdt[5]
			);
			terrorf(TERROR_COM2, "%08X %08X, %08X %08X, %08X %08X\n",
				ttable[i].data->gdt[6], ttable[i].data->gdt[7],
				ttable[i].data->gdt[8], ttable[i].data->gdt[9],
				ttable[i].data->gdt[10], ttable[i].data->gdt[11]
			);
		}
	}
#endif
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

