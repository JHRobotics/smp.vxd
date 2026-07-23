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
static BOOL smp_init_succ = FALSE;
static cpuid_result_t cpu_flags;

uint32_t xsave_flags = 0;
BOOL no_sys_fxsave = FALSE;

/* settings */
BOOL noavx = FALSE;
BOOL nosse = FALSE;
BOOL quiet = FALSE;
BOOL sys_pause = FALSE;
int  max_cpus = 256;

/* smp_init TSC freq */
extern DWORD tsc_ms_clock;
extern DWORD tsc_us_clock;

void Sys_Critical_Init_proc(){ }

void __declspec(naked) timeout_entry();
void __declspec(naked) monitor_entry();

#define AP_MONITOR_TIMEOUT 125

void enable_simd()
{
	DWORD vmm_ver = Get_VMM_Version();
	dbg_printf("VMM ver: 0x%lX\n", vmm_ver);
	if((cpu_flags.regs.regEDX & CPUID_EDX_FXSR) != 0)
	{
		if(vmm_ver < 0x40A)
		{
			no_sys_fxsave = TRUE;
		}
		
		if((cpu_flags.regs.regECX & CPUID_ECX_XSAVE) != 0 && !noavx)
		{
			_asm
			{
				push ecx
				push edx
				mov eax, cr4
				or  eax, 0x40600 /* OSFXSR(9) + OSXMMEXCPT(10) + OSXSAVE(18) */
				mov cr4, eax
				xor ecx, ecx
				db 0x0F, 0x01, 0xD0 /*xgetbv*/
				or eax, 0x7
				db 0x0F, 0x01, 0xD1 /*xsetbv*/
				pop edx
				pop ecx
			}
			
			/* reread CPUID when AVX is now avaible! */
			cpuid(1, &cpu_flags);
			if((cpu_flags.regs.regECX & CPUID_ECX_AVX) != 0)
			{
				xsave_flags = 0x7; /* FPU, XMM, AVX */
				dbg_printf("AVX on!\n");
			}
			else
			{
				dbg_printf("XSAVE but no AVX!\n");
			}
		}
		else
		{
			if(!nosse)
			{
				_asm
				{
					mov eax, cr4
					or  eax, 0x600 /* OSFXSR(9) + OSXMMEXCPT(10) */
					mov cr4, eax
				}
			}
			dbg_printf("no AVX support!\n");
		}
	}
}

/* CommandTail: Address of the command tail retrieved from the program segment prefix (PSP) of VMM32.VXD. The first byte in the command tail specifies the length in bytes of the tail. 
*/
void __stdcall Device_Init_proc(DWORD vm, BYTE *command_tail)
{
	BOOL rc;
	cpuid_result_t cpuid_res;
	
	dbg_init();
	
	VMMCall(_Allocate_Device_CB_Area);
 	ThisVM = vm;
 	
 	sys_pause = Get_Profile_Boolean(VXD_PROFILE, "pause", sys_pause);
 	noavx = Get_Profile_Boolean(VXD_PROFILE, "noavx", noavx);
 	nosse = Get_Profile_Boolean(VXD_PROFILE, "nosse", nosse);
 	quiet = Get_Profile_Boolean(VXD_PROFILE, "quiet", quiet);
 	max_cpus = Get_Profile_Decimal_Int(VXD_PROFILE, "maxcpus", max_cpus);
 	
 	if(nosse)
 	{
 		noavx = TRUE;
 	}
 	
 	dbg_printf("Settings: pause=%d noavx=%d nosse=%d quiet=%d max_cpus=%d\n",
 		sys_pause, noavx, nosse, quiet, max_cpus
 	);
 	
 	memset(&cpu_flags, 0, sizeof(cpuid_result_t));
 	if(cpuid(0, &cpuid_res) == 0)
 	{
 		if(!quiet)
 		{
 			alertf(VXD_DEVICE_NAME ": CPUID not supported! Sorry.\n");
 		}
 		return;
 	}
 	
 	if(cpuid_res.regs.regEAX >= 1)
 	{
 		cpuid(1, &cpu_flags);
		enable_simd();
		dbg_printf("CPUID(1) EBX=%X, ECX=%X EDX=%X\n",
			cpu_flags.regs.regEBX,
			cpu_flags.regs.regECX,
			cpu_flags.regs.regEDX);
		
		if((cpu_flags.regs.regEDX & (CPUID_EDX_FXSR | CPUID_EDX_MMX | CPUID_EDX_TSC | CPUID_EDX_MSR))
			!= (CPUID_EDX_FXSR | CPUID_EDX_MMX | CPUID_EDX_TSC | CPUID_EDX_MSR))
		{
	 		if(!quiet)
	 		{
	 			alertf(VXD_DEVICE_NAME ": needs support in CPU at minimum: MSR, MMX, FXSR, RSC. Sorry.\n");
	 		}
			return;
		}
		
		if(cpuid_res.regs.regEAX >= 0x16)
		{
			/* read TSC from CPUID */
			DWORD tsc_freq = cpuid_tsc_freq();
			dbg_printf("TSC freq = %u\n", tsc_freq);
			
			if(tsc_freq != 0)
			{
				tsc_ms_clock = tsc_freq * 1000;
				tsc_us_clock = tsc_freq;
			}
#ifdef DEBUG
			else
			{
				cpuid_result_t leaf15h;
				cpuid(0x15, &leaf15h);
				dbg_printf("15h: %X %X %X\n", leaf15h.regs.regEAX, leaf15h.regs.regEBX, leaf15h.regs.regECX);
				cpuid(0x16, &leaf15h);
				dbg_printf("16h: %X\n", leaf15h.regs.regEAX);
			}
#endif
		}
 	}
 	else
 	{
 		if(!quiet)
 		{
 			alertf(VXD_DEVICE_NAME ": CPUID is supported, but only leaf 00H, sorry.\n");
 		}
 		return;
 	}
 	
 	if(((cpu_flags.regs.regEDX & CPUID_EDX_HTT) != 0) && max_cpus > 1)
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

			tracef("sizeof(tdata_t) = %d, sizeof(CRS_32) = %d\n",
				sizeof(tdata_t), sizeof(CRS_32));
				
	#ifdef DEBUG
			Set_Global_Time_Out(1000, NULL, timeout_entry);
	#endif
			Set_Global_Time_Out(AP_MONITOR_TIMEOUT, NULL, monitor_entry);
		}
	}
	else if(cpu_flags.regs.regEDX & CPUID_EDX_SSE)
	{
		ts_init();
	}
	
	/* print info for user */
	if(!quiet)
	{
		if(smp_valid)
		{
			alertf(VXD_DEVICE_NAME ": multi-CPU system, CPUs=%d", cpu_count);
		}
		else
		{
			alertf(VXD_DEVICE_NAME ": single-CPU system");
		}
		
		if((cpu_flags.regs.regEDX & CPUID_EDX_FPU) != 0){alertf(", x87");}
		if((cpu_flags.regs.regEDX & CPUID_EDX_MMX) != 0){alertf(", MMX");}
		if((cpu_flags.regs.regEDX & CPUID_EDX_SSE) != 0)
		{
			if((cpu_flags.regs.regECX & CPUID_ECX_SSE4_2) != 0)
			{
				alertf(", SSE4.2");
			}
			else if((cpu_flags.regs.regECX & CPUID_ECX_SSE4_1) != 0)
			{
				alertf(", SSE4.1");
			}
			else if((cpu_flags.regs.regECX & (CPUID_ECX_SSE3 | CPUID_ECX_SSSE3)) != 0)
			{
				if((cpu_flags.regs.regECX & (CPUID_ECX_SSE3 | CPUID_ECX_SSSE3)) == (CPUID_ECX_SSE3 | CPUID_ECX_SSSE3))
				{
					alertf(", SSE3+SSSE3");
				}
				else if((cpu_flags.regs.regECX & CPUID_ECX_SSE3) != 0)
				{
					alertf(", SSE3");
				}
				else
				{
					alertf(", SSSE3");
				}
			}
			else if((cpu_flags.regs.regEDX & CPUID_EDX_SSE2) != 0)
			{
				alertf(", SSE2");
			}
			else
			{
				alertf(", SSE");
			}
		}
		if(xsave_flags != 0)
		{
			if((cpu_flags.regs.regECX & CPUID_ECX_AVX) != 0)
			{
				cpuid(7, &cpuid_res);
				if((cpuid_res.regs.regEBX & CPUID_EBX_AVX2))
				{
					alertf(", AVX2");
				}
				else
				{
					alertf(", AVX");
				}
			}
		}
		alertf("\n");
	} // quiet
	if(sys_pause)
	{
		tpause();
	}
	
	smp_init_succ = TRUE;
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

static DWORD ap_usage(DWORD stats)
{
	DWORD r = 0;
	int i;
	for(i = 0; i < 20; i++)
	{
		if((stats & 0x1) == 1)
		{
			r += 5;
		}
		stats >>= 1;
	}
	return r;
}

static DWORD bsp_usage()
{
	DWORD result = 0xFFFFFFFFFF;
	DWORD hKey;
	if(_RegOpenKey(HKEY_DYN_DATA, "PerfStats\\StartStat", &hKey) == ERROR_SUCCESS)
	{
		DWORD type;
		DWORD cbData = 4;
		DWORD data;
		if(_RegQueryValueEx(hKey, "KERNEL\\CPUUsage", 0, &type, (char*)&data, &cbData) != ERROR_SUCCESS)
		{
			/* error */
			return result;
		}
		_RegCloseKey(hKey);
	}
	
	if(_RegOpenKey(HKEY_DYN_DATA, "PerfStats\\StatData", &hKey) == ERROR_SUCCESS)
	{
		DWORD type;
		DWORD cbData = 4;
		DWORD data;
		if(_RegQueryValueEx(hKey, "KERNEL\\CPUUsage", 0, &type, (char*)&data, &cbData) == ERROR_SUCCESS)
		{
			result = data;
		}
		_RegCloseKey(hKey);
	}
	return result;
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
			if(smp_init_succ)
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
		case DIOC_SMP_CPU_FEATURES:
		{
			DWORD features = SMP_CPU_SMPVXD;
			if((cpu_flags.regs.regEDX & CPUID_EDX_MMX) != 0)
			{
				features |= SMP_CPU_MMX;
			}
			if((cpu_flags.regs.regEDX & CPUID_EDX_SSE) != 0)
			{
				features |= SMP_CPU_SSE;
			}
			if(xsave_flags != 0)
			{
				features |= SMP_CPU_AVX;
			}
			outBuf[0] = features;
			rc = 0;
			break;
		}
		case DIOC_SMP_CPU_STATS:
		{
			DWORD id = inBuf[0];
			if(id == 0)
			{
				outBuf[0] = bsp_usage();
				rc = 0;
			}
			else
			{
				int i;
				for(i = 0; i <= ttable_max; i++)
				{
					if(ttable[i].data->index == id)
					{
						outBuf[0] = ap_usage(ttable[i].data->stats_usage);
						rc = 0;
						break;
					}
				}
			}
			break;
		}
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

void __stdcall monitor_proc(DWORD vm, DWORD tardiness, DWORD refdata, PCRS_32 crs)
{
	int i;
	for(i = 0; i <= ttable_max; i++)
	{
		if(ttable[i].data != NULL)
		{
			uint32_t s = ttable[i].data->status;
			
			ttable[i].data->stats_usage <<= 1;
			if(s > S_SLEEP || s == S_BUSY)
			{
				ttable[i].data->stats_usage |= 1;
			}
		}
	}
	Set_Global_Time_Out(AP_MONITOR_TIMEOUT, NULL, monitor_entry);
}

void __declspec(naked) monitor_entry()
{
	_asm
	{
		push ebx ; vmhandle
		push ecx ; Tardiness
		push edx ; RefData
		push ebp ; crs
		call monitor_proc
		retn
	};
}

#ifdef DEBUG

void __stdcall timeout(DWORD vm, DWORD tardiness, DWORD refdata, PCRS_32 crs)
{
	int i;
	for(i = 0; i <= ttable_max; i++)
	{
		if(ttable[i].data != NULL)
		{
			tracef("CPU#%d = %d (%X) ", i, ttable[i].data->status,  ttable[i].data->stats_usage);
		}
	};
	tracef("\n");
	
	for(i = 0; i <= ttable_max; i++)
	{
		if(ttable[i].data != NULL)
		{
			PCRS_32 is = (PCRS_32)(ttable[i].data->init_state);
			PCRS_32 ps = (PCRS_32)(ttable[i].data->proc_state);
			tracef("CPU#%d: proc_state->Client_EFlags=%X proc_state->Client_EIP=%X\n",
				i, ps->Client_EFlags, ps->Client_EIP);
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
