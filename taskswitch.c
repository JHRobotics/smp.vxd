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

#define THT_PRIME 113

static DWORD act_tid = -1;

static ts_thread_t *ts_ht[THT_PRIME];

DWORD ts_thread_tid()
{
	return act_tid;
}

#define TS_HASH(_id) ((_id) % THT_PRIME)

ts_thread_t *ts_thread_create(DWORD tid)
{
	ts_thread_t *ts = (ts_thread_t *)_PageAllocate(TS_THREAD_PAGES, PG_SYS, 0, 0x0, PAGE_ALLOC_MIN, PAGE_ALLOC_MAX, NULL, PAGEFIXED|PAGEZEROINIT);
	ts_thread_t **tindex;
	DWORD hash;
		
	if(ts != NULL)
	{
		ts->tid = tid;
		ts->next = NULL;
		ts->smp_status = 0;
		ts->smp_apid = 0;
		ts->smp_bsp_idle_proc = 0;
		ts->smp_bsp_idle_lock = NULL;
		ts->dirty = 1;
		ts->data = (DWORD*)(ts+1);
		ts->mode = SMP_MODE_SYSTEM;
		
		hash = TS_HASH(tid);
		
		tindex = &(ts_ht[hash]);
		
		while(*tindex != NULL)
		{
			if((*tindex)->tid == tid)
			{
				_PageFree(ts, 0);
				return *tindex;
			}
			tindex = &((*tindex)->next);
		}
		
		*tindex = ts;
		
		return ts;
	}
	return NULL;
}

void ts_thread_destroy(DWORD tid)
{
	ts_thread_t **tindex;
	DWORD hash = TS_HASH(tid);
	
	tindex = &(ts_ht[hash]);
	
	while(*tindex != NULL)
	{
		if((*tindex)->tid == tid)
		{
			ts_thread_t *garbage = *tindex;
			*tindex = garbage->next;
			_PageFree(garbage, 0);
			return;
		}
			
		tindex = &((*tindex)->next);
	}
}

ts_thread_t *ts_thread_get(DWORD tid)
{
	ts_thread_t *tindex;
	DWORD hash = TS_HASH(tid);
	
	tindex = ts_ht[hash];
	
	while(tindex != NULL)
	{
		if(tindex->tid == tid)
		{
			return tindex;
		}
		tindex = tindex->next;
	}
	return NULL;
}

int switch_to_ap(PCRS_32 crs, DWORD thread_id);

void __stdcall thread_switch(DWORD cur_tid, DWORD old_tid)
{
	ts_thread_t *ts;
	DWORD orig_cr0 = GetCR0();
	act_tid = cur_tid;
	
	/* if TS flag is set, we can't save/restore FPU/XMM/YMM state, so clear it */
	if(orig_cr0 & 0x8)
	{
		_asm clts;
	}

	//dbg_printf("TS: %X => %X ", cur_tid, old_tid);
	
	ts = ts_thread_get(old_tid);
	if(ts)
	{
		if(fpu_need_extra_save())
		{
			TCB_t *cbs_old = (TCB_t*)old_tid;
			if((cbs_old->TCB_Flags & THFLAG_RING0_THREAD) == 0)
			{
				//dbg_printf(" S");
				fpu_save(TRUE, ts->fpu_state);
				ts->dirty = 0;
				//dbg_printf(".");
			}
		}
		
		if(ts->mode == SMP_MODE_AUTORUN && ts->smp_status == 0)
		{
			TCB_t *cbs = (TCB_t*)old_tid;
			//dbg_printf("TS: tid=%X, eip=%X esp=%X\n", old_tid, cbs->TCB_ClientPtr->Client_EIP, cbs->TCB_ClientPtr->Client_ESP);
			//dbg_printf("    TCB_Flags=%X TCB_PMLockOrigEIP=%X\n", cbs->TCB_Flags, cbs->TCB_PMLockOrigEIP);
			
			if(cbs->TCB_ClientPtr->Client_EIP >= 0x00100000 && cbs->TCB_ClientPtr->Client_EIP < 0x80000000)
			{
				switch_to_ap(cbs->TCB_ClientPtr, old_tid);
			}
		}
	}

	if(fpu_need_extra_save())
	{
		TCB_t *cbs_new = (TCB_t*)cur_tid;
		if((cbs_new->TCB_Flags & THFLAG_RING0_THREAD) == 0)
		{
			ts = ts_thread_get(cur_tid);
			if(ts)
			{
				if(ts->dirty == 0)
				{
					//dbg_printf(" R");
					fpu_restore(TRUE, ts->fpu_state);
					//dbg_printf(".");
				}
			}
		}
	}
	
	//dbg_printf("OK\n");
	
	/* set back TS flag, not necessary in theory but i don't want to confuse os */
	if(orig_cr0 & 0x8)
	{
		_asm
		{
			mov eax, [orig_cr0]
			mov cr0, eax
		};
	}
	
}

/* MSDN: ThreadSwitchCallback */
static void __declspec(naked) thread_switch_entry()
{
	_asm
	{
		push eax,  ; OldThreadHandle
		push edi   ; CurThreadHandle
		call thread_switch
		ret
	}
}

void ts_init()
{
	memset(ts_ht, 0, sizeof(ts_thread_t*)*THT_PRIME);
	Call_When_Thread_Switched((DWORD)thread_switch_entry);
}

#define CALLBACK_HEADER_SIZE 8
void callback_breakpoint_entry();

static  void __declspec(naked) callback_int3_entry();

BOOL smp_switch_install()
{
	Hook_PM_Fault(0x3, ((DWORD)callback_int3_entry) + CALLBACK_HEADER_SIZE);
	return TRUE;
}

void smp_switch_uninstall()
{
	Unhook_PM_Fault(0x3, ((DWORD)callback_int3_entry) + CALLBACK_HEADER_SIZE);
}

#define SAVE_NOP 0
#define SAVE_FXSAVE 1
#define SAVE_XSAVE_ALL 2
#define SAVE_XSAVE_AVX 3

static int fpu_save_method(sys)
{
	int m = SAVE_NOP;
	if(sys)
	{
		if(xsave_flags == 0 && no_sys_fxsave) /* no AVX and Windows 95 */
		{
			m = SAVE_FXSAVE;
		}
		else if(xsave_flags != 0 && no_sys_fxsave) /* AVX and Windows 95 */
		{
			m = SAVE_XSAVE_ALL;
		}
		else if(xsave_flags != 0) /* AVX and > Windows 95 */
		{
			m = SAVE_XSAVE_AVX;
		}
	}
	else
	{
		if(xsave_flags == 0)
		{
			m = SAVE_FXSAVE;
		}
		else
		{
			m = SAVE_XSAVE_ALL;
		}
	}
	
	return m;
}

void fpu_save(BOOL sys, uint8_t *dst)
{
	int m = fpu_save_method(sys);
	//dbg_printf("fpu_save, m=%d, CR0=%X CR4=%X\n", m, GetCR0(), GetCR4());
	
	switch(m)
	{
		case SAVE_NOP:
			break;
		case SAVE_FXSAVE:
			_asm
			{
				push edx
				mov edx, [dst]
				fxsave [edx]
				pop edx
			};
			break;
		case SAVE_XSAVE_ALL:
			_asm
			{
				push edx
				push edi
				mov edi, [dst]
				mov eax, [xsave_flags]
				xor edx,edx
				db 0x0f,0xAE,0x27  /* xsave [edi] */
				;db 0x0F, 0xC7, 0x2F /* xsaves [edi] */
				pop edi
				pop edx
			}
			break;
		case SAVE_XSAVE_AVX:
			_asm
			{
				push edx
				push edi
				mov edi, [dst]
				mov eax, [xsave_flags]
				xor edx,edx
				and eax, 0xFFFFFFFC
				db 0x0f,0xAE,0x27  /* xsave [edi] */
				;db 0x0F, 0xC7, 0x2F /* xsaves [edi] */
				pop edi
				pop edx
			}
			break;
	}
}

void fpu_restore(BOOL sys, const uint8_t *src)
{
	int m = fpu_save_method(sys);
	
	switch(m)
	{
		case SAVE_NOP:
			break;
		case SAVE_FXSAVE:
			_asm
			{
				push edx
				mov edx, [src]
				fxrstor [edx]
				pop edx
			};
			break;
		case SAVE_XSAVE_ALL:
			_asm
			{
				push edx
				push edi
				mov edi, [src]
				mov eax, [xsave_flags]
				xor edx,edx
				db 0x0F,0xAE,0x2F  /* xrstor [edi]*/
				;db 0x0F, 0xC7, 0x1F /* xrstors [edi] */
				pop edi
				pop edx
			}
			break;
		case SAVE_XSAVE_AVX:
			_asm
			{
				push edx
				push edi
				mov edi, [src]
				mov eax, [xsave_flags]
				xor edx,edx
				and eax, 0xFFFFFFFC
				db 0x0F,0xAE,0x2F  /* xrstor [edi] */
				;db 0x0F, 0xC7, 0x1F /* xrstors [edi] */
				pop edi
				pop edx
			}
			break;
	}
}

BOOL fpu_need_extra_save()
{
	if(fpu_save_method(TRUE) == SAVE_NOP)
	{
		return FALSE;
	}
	return TRUE;
}

int switch_to_ap(PCRS_32 crs, DWORD thread_id)
{
	BOOL found = FALSE;
	BOOL wakeup = FALSE;
	int cpu;
	for(cpu = 0; cpu <= ttable_max; cpu++)
	{
		if(ttable[cpu].data != NULL)
		{
			uint32_t s;
			volatile uint32_t *lck = &(ttable[cpu].data->status);
				
			do
			{
				atomic_lock(lck, &s);
			} while(s == S_BUSY);
				
			switch(s)
			{
				case S_READY:
				case S_SLEEP:
				{
					ts_thread_t *ts = ts_thread_get(thread_id);
					if(ts)
					{
						if(ts->smp_status == 0 && ts->smp_bsp_idle_proc != 0)
						{
							if(ts->smp_bsp_idle_lock != NULL)
							{
								*(ts->smp_bsp_idle_lock) = 1;
							
								//ttable[cpu].data->proc_cr4 = GetCR4();
								ttable[cpu].data->thread_id = thread_id;
								fpu_save(FALSE, ttable[cpu].data->fpu_state);
							
								memcpy(ttable[cpu].data->proc_state, crs, CRS_32_EFECTIVE_SIZE);

								copy_pd(ttable[cpu].data->pd, 0);
								/* we need change stack otherwise BSP idle and AP will overwrite own data */
								crs->Client_ESP = ttable[cpu].stack + STACK_OFF_PLACEHOLDER;
								crs->Client_ECX = ts->smp_bsp_idle_proc;
								crs->Client_EAX = (DWORD)(ts->smp_bsp_idle_lock);
								crs->Client_EIP = kernel_flat + SMP_OFFSET_TRAMPOLINE;
								ts->smp_status = 1;
								ts->smp_apid = cpu;
								
								if(s == S_SLEEP)
								{
									wakeup = TRUE;
								}
							
								s = S_LOADED;
								found = 1;
							
								//dbg_printf("SWITCH TO AP#%d: thread=%lX, proc=%X\n", cpu, ttable[cpu].data->thread_id, ts->smp_bsp_idle_proc);
							} // have lock
						} // status
					} // rs
					break;
				} // case
			} // switch
				
			if(wakeup)
			{
				/* must be before unlock to prevent deadlock */
				smp_wakeup(cpu);
			}

			atomic_unlock(lck, s);
					
			if(found)
			{
				//dbg_printf("int3 to AP: %d\n", cpu);
				return 1;
			}
		}
	}
	//dbg_printf("int3 but no AP\n");
	return 0;
}

int switch_to_bsp(PCRS_32 crs, DWORD thread_id)
{
	int rc = 0;
	ts_thread_t *ts = ts_thread_get(thread_id);
	if(ts)
	{
		if(ts->smp_status != 0)
		{
			volatile uint32_t *lck = &(ttable[ts->smp_apid].data->status);
			uint32_t s;

			do
			{
				atomic_lock(lck, &s);
			} while(s == S_BUSY);
				
			if(s == S_CARGO)
			{
				DWORD *stack;
				DWORD tmp_eip;
				memcpy(crs, ttable[ts->smp_apid].data->proc_state, CRS_32_EFECTIVE_SIZE);
				fpu_restore(FALSE, ttable[ts->smp_apid].data->fpu_state);
				
				tmp_eip = crs->Client_EIP;
				
				stack = (DWORD*)(crs->Client_ESP);
				
				stack--;
				*stack = crs->Client_EFlags;
				stack--;
				*stack = crs->Client_CS;
				stack--;
				*stack = crs->Client_EIP;
					
				crs->Client_ESP -= 3*4;
				crs->Client_EIP = kernel_flat + SMP_OFFSET_INT;
					
				ts->smp_status = 0;
				s = S_READY;
				//dbg_printf("SWITCH BSP: AP#%d return=%X\n", ts->smp_apid, tmp_eip);
				
				rc = 1;
			}
			
			atomic_unlock(lck, s);
		}
	}
	return rc;
}

static DWORD __stdcall callback_int3(DWORD vm, PCRS_32 crs)
{
	if(crs->Client_EIP >= (kernel_flat + SMP_OFFSET_FLY) &&
		crs->Client_EIP < (kernel_flat + SMP_OFFSET_FLY + SMP_FN_BLOCK_SIZE))
	{
		switch_to_ap(crs, ts_thread_tid());
		return 1;
	}
	else if(crs->Client_EIP >= (kernel_flat + SMP_OFFSET_REATTACH) &&
		crs->Client_EIP < (kernel_flat + SMP_OFFSET_REATTACH + SMP_FN_BLOCK_SIZE))
	{
		switch_to_bsp(crs, ts_thread_tid());
		return 1;
	}
	
	dbg_printf("INT3 EIP: 0x%lX\n", crs->Client_EIP);
	
	return 0;
}

static DWORD int3_next_hook = 0;
static void __declspec(naked) callback_int3_entry()
{
	_asm
	{
		; header (8 bytes)
		jmp short callback_int3_realentry
		jmp [int3_next_hook]
		
		callback_int3_realentry:
		pushfd
		pushad
		push ebp ; crs
		push ebx ; VM
		call callback_int3
		test eax, eax
		jz callback_int3_next
			popad
			popfd
			retn ; handled!
		
		callback_int3_next:
		popad
		popfd
		jmp [int3_next_hook]
	}
}
