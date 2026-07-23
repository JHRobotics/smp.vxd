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

/* TODO: calibate the timer! - this expect ~3 GHz CPU */
DWORD tsc_ms_clock = 3000000;
DWORD tsc_us_clock = 3000;
/* NOTE: currently longer delays never mind, but keep on ming on future! */

/* On modern C compiler this should be writen in pure C, but WCC not supporting
	uint64_t, so it's written on assembly...
 */

void tsc_get(void *counter_qw_ptr)
{
	_asm
	{
		mov ecx, [counter_qw_ptr]
		rdtsc
		mov [ecx], eax
		mov [ecx+4], edx
	};
}

BOOL tsc_ring(const void *counter_qw_ptr, uint32_t extra_ticks)
{
	BOOL result = FALSE;
	_asm
	{
		push esi
		push edi
		mov ecx, [counter_qw_ptr]
		mov edi, [ecx]
		mov esi, [ecx+4]
		mov ecx, [extra_ticks]
		add edi, ecx
		adc esi, 0
		rdtsc
		cmp edx, esi
		jb not_ring
			cmp eax, edi
			jb not_ring
				mov [result], 1
		not_ring:
		pop edi
		pop esi
	};
	
	return result;
}

/***
 * looks like VMM hasn't any function for sync wait (delay, sleep, whatever)
 * 1 = one cycle
 *
 **/
static void delay_loop(DWORD repeats)
{
	_asm
	{
		push esi
		push edi
		rdtsc
		mov esi, eax
		mov edi, edx
		mov eax, [repeats]
		add esi, eax
		adc edi, 0
	
		loop_repeat:
			pause
			rdtsc
			cmp edx,edi
			jb loop_repeat
			cmp eax,esi
			jb loop_repeat

		pop edi
		pop esi
	};
}

void mdelay(DWORD ms)
{
	delay_loop(ms*tsc_ms_clock);
}

void udelay(DWORD us)
{
	delay_loop(us*tsc_us_clock);
}
