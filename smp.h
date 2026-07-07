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
#ifndef __SMP_H__INCLUDED__
#define __SMP_H__INCLUDED__

/* from: https://wiki.osdev.org/Symmetric_Multiprocessing */
#pragma pack(push)
#pragma pack(1)

typedef struct mpfp
{
	char signature[4];
	uint32_t configuration_table;
	uint8_t length; // In 16 bytes (e.g. 1 = 16 bytes, 2 = 32 bytes)
	uint8_t mp_specification_revision;
	uint8_t checksum; // This value should make all bytes in the table equal 0 when added together
	uint8_t default_configuration; // If this is not zero then configuration_table should be 
                                 // ignored and a default configuration should be loaded instead
	uint32_t features; // If bit 7 is then the IMCR is present and PIC mode is being used, otherwise 
                     // virtual wire mode is; all other bits are reserved
} mpfp_t;

#define MPFP_MAGIC "_MP_"

typedef struct mpct
{
	char signature[4]; // "PCMP"
	uint16_t length;
	uint8_t mp_specification_revision;
	uint8_t checksum; // Again, the byte should be all bytes in the table add up to 0
	char oem_id[8];
	char product_id[12];
	uint32_t oem_table;
	uint16_t oem_table_size;
	uint16_t entry_count; // This value represents how many entries are following this table
	uint32_t lapic_address; // This is the memory mapped address of the local APICs 
	uint16_t extended_table_length;
	uint8_t extended_table_checksum;
	uint8_t reserved;
} mpct_t;

#define MPCT_MAGIC "PCMP"

typedef struct mp_processor {
	uint8_t type; // Always 0
	uint8_t local_apic_id;
	uint8_t local_apic_version;
	uint8_t flags; // If bit 0 is clear then the processor must be ignored
		// If bit 1 is set then the processor is the bootstrap processor
	union
	{
		struct
		{
			uint32_t signature;
			uint32_t feature_flags;
			uint8_t reserved[8];
		} processor;
		struct
		{
			uint32_t address;
		} apic;
	} bytype;
} mp_processor_t;

#define	MP_TYPE_PROCESSOR	0
#define	MP_TYPE_BUS		    1
#define	MP_TYPE_IOAPIC	  2
#define	MP_TYPE_INTSRC	  3
#define	MP_TYPE_LINTSRC	  4

#define MP_SIZE_PROCESSOR 20
#define MP_SIZE_APIC 8

#define AP_BOOT_VAR_OFF 0x0180
#define AP_KERNEL_ISR_OFF 0x800
#define AP_KERNEL_BASE 0xC0000000

typedef struct apboot_vars
{
	uint16_t gdt_size;
	uint32_t gdt_address;
	uint16_t idt_size;
	uint32_t idt_address;
	uint32_t sys_cr3;
	uint32_t kernel_addr;
	uint32_t stack_addr;
	uint32_t bsp_id;
	uint32_t ttable;
} apboot_vars_t;

typedef struct tdata
{
	volatile uint32_t status;  // 0
	uint32_t thread_id;        // 4
	uint32_t entry;            // 8
	uint32_t cpu_cr3;          // 12
	uint32_t cpu_cr4;          // 16
	uint32_t *pd;              // 20
	uint32_t index;            // 24
	uint32_t xsaveflags;       // 28
	uint32_t gdt[32];          // 32
	uint8_t  init_state[128];  // 160  -> tagCRS_32
	uint8_t  proc_state[128];  // 288 -> tagCRS_32
	uint32_t tss[32];          // 416 (size 104)
	uint8_t pad[32];           // 544
	uint8_t  fpu_state[1024];  // 576
} tdata_t;                   // size: 1600

typedef struct titem
{
	uint32_t stack;
	tdata_t *data;
} titem_t;

#define STACK_PAGES 8
#define STACK_SIZE (STACK_PAGES*P_SIZE)
#define STACK_OFF_KERNEL (P_SIZE*4 - 4)
#define STACK_OFF_PLACEHOLDER (P_SIZE*6 - 4)
#define STACK_OFF_TSS (P_SIZE*8 - 4)

#define DATA_PAGES 2
/* 1 - tdata, 2 - PD */

#define S_BUSY  0
#define S_READY 1
#define S_SLEEP 2
#define S_LOADED 3
#define S_RUNNING 4
#define S_CARGO 5
#define S_DIRCARD 6

#define MAX_CORES 256

#pragma pack(pop)

extern titem_t *ttable;

BOOL smp_init();
BOOL smp_wakeup(int apid);

int __cdecl smp_ap_main(uint32_t apid);

extern uint8_t *smp_first_mb;

extern uint32_t kernel_flat;
extern int cpu_count;

uint32_t __cdecl atomic_lock(volatile uint32_t *lockptr, uint32_t *presult);
void __cdecl atomic_unlock(volatile uint32_t *lockptr, int v);

void smp_elevate(DWORD proc, DWORD lockaddr, DWORD mode);
BOOL smp_switch_install();

void copy_pd(uint32_t *dest_pd, int full);

extern uint32_t xsave_flags;
extern BOOL no_sys_fxsave;

void fpu_save(BOOL sys, uint8_t *dst);
void fpu_restore(BOOL sys, const uint8_t *src);
BOOL fpu_need_extra_save();

#endif /* __SMP_H__INCLUDED__ */
