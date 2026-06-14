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
	ts_thread_t *ts = (ts_thread_t *)_PageAllocate(TS_THREAD_PAGES, PG_SYS, 0, 0x0, PAGE_ALLOC_MIN, PAGE_ALLOC_MAX, NULL, PAGEFIXED);
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
	act_tid = cur_tid;
	
	ts = ts_thread_get(old_tid);
	if(ts)
	{
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

int switch_to_ap(PCRS_32 crs, DWORD thread_id)
{
	BOOL found = FALSE;
	int cpu;
	for(cpu = 0; cpu < MAX_CORES; cpu++)
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
							
								ttable[cpu].data->proc_cr4 = GetCR4();
								ttable[cpu].data->thread_id = thread_id;
							
								memcpy(ttable[cpu].data->proc_state, crs, CRS_32_EFECTIVE_SIZE);

								copy_pd(ttable[cpu].data->pd, 0);
								/* we need change stack otherwise BSP idle and AP will overwrite own data */
								crs->Client_ESP = ttable[cpu].stack + STACK_OFF_PLACEHOLDER;
								crs->Client_ECX = ts->smp_bsp_idle_proc;
								crs->Client_EAX = (DWORD)(ts->smp_bsp_idle_lock);
								crs->Client_EIP = kernel_flat + SMP_OFFSET_TRAMPOLINE;
								ts->smp_status = 1;
								ts->smp_apid = cpu;
							
								s = S_LOADED;
								found = 1;
							
								//dbg_printf("SWITCH TO AP#%d: thread=%lX, proc=%X\n", cpu, ttable[cpu].data->thread_id, ts->smp_bsp_idle_proc);
							} // have lock
						} // status
					} // rs
					break;
				} // case
			} // switch
				
			if(found)
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
