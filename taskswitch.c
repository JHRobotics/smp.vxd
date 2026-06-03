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

void __stdcall thread_switch(DWORD cur_tid, DWORD old_tid)
{
	act_tid = cur_tid;
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

#if 0
static void __declspec(naked) idle_trampoline()
{
	_asm
	{
		push eax
		idle_loop:
		call [ecx]
		jmp idle_loop
	}
}
#endif

static DWORD __stdcall callback_int3(DWORD vm, PCRS_32 crs)
{
	if(crs->Client_EIP >= (kernel_flat + SMP_OFFSET_FLY) &&
		crs->Client_EIP < (kernel_flat + SMP_OFFSET_FLY + SMP_FN_BLOCK_SIZE))
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
						ts_thread_t *ts = ts_thread_get(ts_thread_tid());
						if(ts)
						{
							if(ts->smp_status == 0 && ts->smp_bsp_idle_proc != 0)
							{
								if(ts->smp_bsp_idle_lock != NULL)
								{
									*(ts->smp_bsp_idle_lock) = 1;
								}
								
								ttable[cpu].data->proc_cr4 = GetCR4();
								ttable[cpu].data->thread_id = ts_thread_tid();
								
								memcpy(ttable[cpu].data->proc_state, crs, sizeof(CRS_32));
								copy_pd(ttable[cpu].data->pd, 0);
								
								/*
								crs->Client_ECX = ts->smp_bsp_idle_proc;
								crs->Client_EAX = (DWORD)ts->smp_bsp_idle_lock;
								crs->Client_EIP = (DWORD)idle_trampoline;
								*/
								
								// TODO: check if stack is writeable
								crs->Client_ESP -= 4;
								*((DWORD*)(crs->Client_ESP)) = (DWORD)ts->smp_bsp_idle_lock;
								crs->Client_ESP -= 4;
								*((DWORD*)(crs->Client_ESP)) = 0xFFFFFFF0; /* return to invalid address */
								crs->Client_EIP = ts->smp_bsp_idle_proc;
							
								ts->smp_status = 1;
								ts->smp_apid = cpu;
							
								s = S_LOADED;
								found = 1;
								
								dbg_printf("SWITCH: thread=%lX, CR3=%lX\n", ttable[cpu].data->thread_id, ttable[cpu].data->cpu_cr3);
							}
						}
						else
						{
							dbg_printf("TID = %lX, lookup fail\n", ts_thread_tid());
						}
						
						break;
					}
				} // switch
				
				if(found)
				{
					/* must be before unlock to prevent deadlock */
					smp_wakeup(cpu);
				}

				atomic_unlock(lck, s);
					
				if(found)
				{
					terrorf(TERROR_COM1, "int3 to AP: %d\n", cpu);

					return 1;
				}
			}
		}
		terrorf(TERROR_COM1, "int3 but no AP\n");
		return 1;
	}
	else if(crs->Client_EIP >= (kernel_flat + SMP_OFFSET_REATTACH) &&
		crs->Client_EIP < (kernel_flat + SMP_OFFSET_REATTACH + SMP_FN_BLOCK_SIZE))
	{
		ts_thread_t *ts = ts_thread_get(ts_thread_tid());
		if(ts)
		{
			if(ts->smp_status != 0)
			{
				volatile uint32_t *lck = &(ttable[ts->smp_apid].data->status);
				uint32_t s;
				//BOOL found = TRUE;;
				do
				{
					atomic_lock(lck, &s);
				} while(s == S_BUSY);
				
				if(s == S_CARGO)
				{
					memcpy(crs, ttable[ts->smp_apid].data->proc_state, sizeof(CRS_32));
					ts->smp_status = 0;
					s = S_READY;
					terrorf(TERROR_COM1, "int3 from %d to BSP\n", ts->smp_apid);
				}
					
				atomic_unlock(lck, s);
			}
		}
		
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
		popad
		jz callback_int3_next
			popfd
			retn ; handled!
		
		callback_int3_next:
		popfd
		jmp [int3_next_hook]
	}
}

