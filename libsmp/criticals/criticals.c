#include <windows.h>
#include "smp9x.h"
#include <stdio.h>

extern CRITICAL_SECTION cs;
extern HANDLE memheap;

#define CS_PRIME 1013

typedef struct alt_cs
{
	LPCRITICAL_SECTION ptr;
	volatile DWORD lock;
	struct alt_cs *next;
} alt_cs_t;

alt_cs_t *cstable[CS_PRIME] = {NULL};
static unsigned cscounter = 0;

volatile DWORD __cdecl *cs_create(LPCRITICAL_SECTION ptr)
{
	DWORD ptr_flat = (DWORD)ptr;
	
	alt_cs_t **acs;
	for(acs = &(cstable[ptr_flat % CS_PRIME]); (*acs) != NULL; acs = &((*acs)->next))
	{
		if((*acs)->ptr == ptr)
		{
			return &((*acs)->lock);
		}
	}
	
	alt_cs_t *ni = HeapAlloc(memheap, 0, sizeof(alt_cs_t));
	if(ni != NULL)
	{
		ni->next = NULL;
		ni->lock = 0;
		ni->ptr = ptr;
		*acs = ni;

		//printf("new cs created: %u\n", cscounter);
		cscounter++;
		
		return &(ni->lock);
	}
	
	return NULL;
}

volatile DWORD __cdecl *cs_get(LPCRITICAL_SECTION ptr)
{
	DWORD ptr_flat = (DWORD)ptr;
	
	alt_cs_t *acs;
	for(acs = cstable[ptr_flat % CS_PRIME]; acs != NULL; acs = acs->next)
	{
		if(acs->ptr == ptr)
		{
			return &(acs->lock);
		}
	}
	return NULL;
}

void __cdecl cs_delete(LPCRITICAL_SECTION ptr)
{
	DWORD ptr_flat = (DWORD)ptr;

	alt_cs_t **acs;
	for(acs = &(cstable[ptr_flat % CS_PRIME]); (*acs) != NULL; acs = &((*acs)->next))
	{
		if((*acs)->ptr == ptr)
		{
			alt_cs_t *garbage = *acs;
			*acs = garbage->next;
			HeapFree(memheap, 0, garbage);
			//printf("cs_destroy: %u\n", cscounter);
			cscounter--;
			
			return;
		}
	}
	
	printf("cs not found\n");
	
}


