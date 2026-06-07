#include "vxd.h"

#if 1
uint32_t __cdecl atomic_lock(volatile uint32_t *lockptr, uint32_t *presult)
{
	uint32_t result = 0;
	_asm
	{
		mov ecx, [lockptr]
		xor eax, eax
		lock xchg [ecx], eax
		mov [result], eax
	}
	
	if(presult != NULL)
	{
		*presult = result;
	}
	
	if(result == 0)
	{
		_asm wait;
	}
	
	return result;
}

void __cdecl atomic_unlock(volatile uint32_t *lockptr, int v)
{
	_asm
	{
		mov ecx, [lockptr]
		mov eax, [v]
		lock xchg [ecx], eax
	}
}
#endif

void smp_elevate(DWORD proc, DWORD lockaddr, DWORD mode)
{
	
	ts_thread_t *ts = ts_thread_get(ts_thread_tid());
	if(ts)
	{
		ts->smp_bsp_idle_proc = proc;
		ts->smp_bsp_idle_lock = (DWORD*)lockaddr;
		ts->mode = mode;
		dbg_printf("Thread: %lX <- proc=%lX, lock=%lX cr3=%lX\n",
			ts_thread_tid(), proc, lockaddr, GetCR3()
		);
	}
	else
	{
		dbg_printf("can't elevate TID=%lX\n", ts_thread_tid());
		
	}
}

void smp_sleep()
{
	_asm
	{
		sti
		hlt
	};
}

int __cdecl smp_ap_main(uint32_t apid)
{
	uint32_t s;
	int rc = 0;
	volatile uint32_t *sp = &(ttable[apid].data->status);
	
//	*sp = apid;
//	smp_sleep();

	do
	{
		atomic_lock(sp, &s);
	} while(s == S_BUSY);
	
	switch(s)
	{
		case S_READY:
			s = S_SLEEP;
			break;
		case S_SLEEP:
			// sheeps++
			break;
		case S_LOADED:
			rc = 1;
			s = S_RUNNING;
			break;
		case S_RUNNING:
		{
			/* notify idle proc, that task is ready for switch back */
			ts_thread_t *ts = ts_thread_get(ttable[apid].data->thread_id);
			if(ts != NULL)
			{
				if(ts->smp_bsp_idle_lock != NULL)
				{
					*(ts->smp_bsp_idle_lock) = 0;
				}
			}
			s = S_CARGO;
			break;
		}
		case S_CARGO:
			// wait to unload...
			break;
		case S_DIRCARD:
			s = S_READY;
			break;
	}
			
	atomic_unlock(sp, s);
	
	if(s == S_SLEEP || s == S_CARGO)
	{
		smp_sleep();
	}
	
	return rc;
}
