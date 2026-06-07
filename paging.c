#include "vxd.h"

static uint32_t *sys_pd = NULL;

void copy_pd(uint32_t *dest_pd, int full)
{
	if(sys_pd == NULL)
	{
		sys_pd = (uint32_t *)_MapPhysToLinear(GetCR3(), P_SIZE, 0);
		dbg_printf("sys pd: %X\n", sys_pd);
		/* Note: PD is on static address 0xFFBFE000,
		   but for make sure, map it again
		 */
		if(sys_pd == NULL)
		{
			return;
		}
	}

	//memcpy(dest_pd, sys_pd, 4096);
	_asm
	{
		push esi
		push edi
		pushfd
		cld
		mov ecx, 1024
		mov esi, [sys_pd]
		mov edi, [dest_pd]
		rep movsd
		popfd
		pop esi
		pop edi
	}
	
	if(full == 0)
	{
		/* trap for kernel32 range (0xBFC0 0000 - BFFF FFFF) */
		dest_pd[0x2FF] = 0;
		
		/* there are monsters under the bed */
		dest_pd[0x000] = 0;
	}
}
