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
#ifndef __X86_H__INCLUDED__
#define __X86_H__INCLUDED__

#pragma pack(push)
#pragma pack(1)

static uint32_t def_gdt[] = 
{
	0x00000000, 0x00000000,
	0x0000FFFF, 0x00CF9A00,
	0x0000FFFF, 0x00CF9200,
	0x0000FFFF, 0x00CFFA00,
	0x0000FFFF, 0x00CFF200,
  //0x00000067, 0x00CF8900
  0x00000067, 0x00008900
};

typedef struct _isr_desc32 {
	uint16_t offset_low;      // offset bits 0..15
	uint16_t selector;        // a code segment selector in GDT or LDT
	uint8_t  zero;            // unused, set to 0
	uint8_t  type_attributes; // gate type, dpl, and p fields
	uint16_t offset_high;     // offset bits 16..31
} isr_desc32_t;

#define ISR_MERGE_ADDRESS(_isr) (((uint32_t)((_isr)->offset_high) << 16) | ((uint32_t)((_isr)->offset_low)))
#define ISR_SPLIT_ADDRESS(_isr, _a) (_isr)->offset_high = (_a) >> 16; (_isr)->offset_low = (_a) & 0xFFFF

#define IA32_APIC_BASE 0x1B
#define APIC_GLOBAL_ENABLE 0x00000800
#define APIC_BSP_FLAG 0x00000100

#pragma pack(pop)

#endif /* __X86_H__INCLUDED__ */
