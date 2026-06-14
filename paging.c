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
		//dest_pd[0x000] = 0;
	}
}
