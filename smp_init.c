#include "vxd.h"

typedef DWORD size_t;

#include "x86.h"
#include "apboot.h"
#include "apkernel.h"

/***
 * looks like VMM hasn't any function for sync wait (delay, sleep, whatever)
 * 1 000 000 = below 1 ms on modern cpus = 0.8 Intel, 0.2 AMD
 *
 **/
void delay_loop(DWORD repeats)
{
	_asm
	{
		mov ecx, [repeats]
		loop_repeat:
			jmp loop_loop
			loop_loop:
				loop loop_repeat
	};
}

void mdelay(DWORD ms)
{
	delay_loop(ms*5000000);
}

void udelay(DWORD us)
{
	delay_loop(us*5000);
}

static uint8_t mp_checksum(uint8_t *ptr, unsigned int len)
{
	uint8_t sum = 0;

	while(len--)
	{
		sum += *ptr;
		ptr++;
	}
	return sum;
}

static mpfp_t *smp_scan_mpfp(uint32_t from, uint32_t to)
{
	uint8_t *start = smp_first_mb+from;
	uint8_t *end   = smp_first_mb+to;
	uint8_t *ptr;
	
	// (on win9x is first MB maped to linear space 1:1
	for(ptr = start; ptr < end; ptr += 16)
	{
		if(memcmp(ptr, MPFP_MAGIC, 4) == 0)
		{
			mpfp_t *mpft = (mpfp_t*)ptr;
			if(mp_checksum(ptr, sizeof(mpfp_t)) == 0)
			{
				return mpft;
			}
			else
			{
				dbg_printf("MPFP found but checksum (%X)\n", ptr-smp_first_mb);
			}
		}
	}
	
	return NULL;
}

mpfp_t *smp_get_mpfp()
{
	mpfp_t *fp;

	if((fp = smp_scan_mpfp(0x0009F000, 0x0009FFFF)) != NULL) return fp;
	if((fp = smp_scan_mpfp(0x0007FC00, 0x0007FCFF)) != NULL) return fp;
	if((fp = smp_scan_mpfp(0x000F0000, 0x000FFFFF)) != NULL) return fp;

	return NULL;
}

#define AP_BOOT_ADDR 0x8000

titem_t *ttable = NULL;

static uint8_t *kernel = NULL;
uint32_t kernel_flat = 0;
static mpfp_t *mpfp = NULL;
int cpu_count = 0;
static volatile uint32_t *lapic32 = NULL;

#define PAGE_BACKUP_SIZE 4096
static uint8_t page_backup[PAGE_BACKUP_SIZE];

BOOL smp_wakeup(int apid)
{
	if(lapic32 != NULL)
	{
		//dbg_printf("smp_wakeup(%d)", apid);
		
		lapic32[0x310/4] = (lapic32[0x310/4] & 0x00ffffff) | (apid << 24); // select AP   
		lapic32[0x300/4] = (lapic32[0x300/4] & 0xfff00000) | 0x005400; // NMI
		do{
			_asm pause;
		} while((lapic32[0x300/4] & (1 << 12)) != 0); // wait for delivery
		return TRUE;
	}
	return FALSE;
}

BOOL smp_init_bsp(titem_t *ttable, void *kernel, DWORD lapic)
{
	int i, j;
	apboot_vars_t *vars = (apboot_vars_t*)(smp_first_mb+AP_BOOT_ADDR+AP_BOOT_VAR_OFF);
	uint8_t bspid = 0;

	lapic32 = (volatile uint32_t*)_MapPhysToLinear(lapic, 4096, 0);
	dbg_printf("lapic32: %lX\n", lapic32);
	
	// get the BSP's Local APIC ID
	_asm{
		mov eax, 1
		cpuid
		shr ebx, 24
		mov [bspid], bl

		cli
	};

	memcpy(page_backup, smp_first_mb+AP_BOOT_ADDR, PAGE_BACKUP_SIZE);
	// copy the AP trampoline code to a fixed address in low conventional memory (to address 0x0800:0x0000)
	memcpy(smp_first_mb+AP_BOOT_ADDR, apboot, apboot_len);

	dbg_printf("lapic = 0x%lX\n", lapic);
	
	// for each Local APIC ID we do...
	for(i = 0; i < MAX_CORES; i++)
	{
		// CPU without stack is is not present
		if(ttable[i].stack == 0) continue;
		
		// do not start BSP, that's already running this code
		if(i == bspid) continue;
		
		dbg_printf("starting AP: #%d data=0x%lX stack=0x%lX cr3=0x%lX\n",
			i, ttable[i].data, ttable[i].stack, ttable[i].data->cpu_cr3);
			
		copy_pd(ttable[i].data->pd, 1);

		vars->gdt_size    = sizeof(def_gdt)-1;
		vars->gdt_address = (uint32_t)(ttable[i].data->gdt);
		vars->idt_size    = (256*8) - 1;
		vars->idt_address = kernel_flat + AP_KERNEL_ISR_OFF;
		vars->sys_cr3     = ttable[i].data->cpu_cr3;
		vars->kernel_addr = kernel_flat;
		vars->stack_addr  = ttable[i].stack + STACK_SIZE - 4; // stack TOP
		vars->bsp_id      = bspid;
		vars->ttable      = (uint32_t)ttable;
		
		//memcpy((void*)(AP_BOOT_ADDR), smp_first_mb+AP_BOOT_ADDR, apboot_len);

		dbg_printf("call APIC 1!\n");

		// send INIT IPI
		lapic32[0x310/4] = (lapic32[0x310/4] & 0x00ffffff) | (i << 24); // select AP   
		lapic32[0x300/4] = (lapic32[0x300/4] & 0xfff00000) | 0x00C500; // trigger INIT IPI
		do{
			_asm pause;
		} while((lapic32[0x300/4] & (1 << 12)) != 0); // wait for delivery
		
		dbg_printf("call APIC 2!\n");
		lapic32[0x310/4] = (lapic32[0x310/4] & 0x00ffffff) | (i << 24); // select AP
		lapic32[0x300/4] = (lapic32[0x300/4] & 0xfff00000) | 0x008500; // deassert
		do {
			_asm pause;
		} while((lapic32[0x300/4] & (1 << 12)) != 0); // wait for delivery

		mdelay(10);  // wait 10 msec
		// send STARTUP IPI (twice)
		
		dbg_printf("startup!\n");
		for(j = 0; j < 2; j++) {
			lapic32[0x280/4] =  0; // clear APIC errors
			lapic32[0x310/4] = (lapic32[0x310/4] & 0x00ffffff) | (i << 24); // select AP
			lapic32[0x300/4] = (lapic32[0x300/4] & 0xfff0f800) | 0x000608;  // trigger STARTUP IPI for 0800:0000
			udelay(200); // wait 200 usec
			do{
				_asm pause;
			} while((lapic32[0x300/4] & (1 << 12)) != 0); // wait for delivery
		}
		
		dbg_printf("wait for boot, apid=0x%X, data=0x%lX stack=0x%lX\n", i, ttable[i].data, ttable[i].stack);
		
		// wait for CPU boot
		while(ttable[i].data->status == S_BUSY)
		{
			_asm pause;
		}
	}
		
	memcpy(smp_first_mb+AP_BOOT_ADDR, page_backup, PAGE_BACKUP_SIZE);
	_asm sti;
	
	return TRUE;
}

ULONG _PageCode(unsigned int pages, int zero, uint32_t *pphy)
{
	ULONG a;
	ULONG phy = 0;
	uint32_t flags = PAGEFIXED|PAGEUSEALIGN|PAGECONTIG;
	if(zero)
	{
		flags |= PAGEZEROINIT;
	}
	
	a = _PageAllocate(pages, PG_SYS, 0, 0x0, PAGE_ALLOC_MIN, PAGE_ALLOC_MAX, &phy, flags);
	if(a != 0)
	{
		_PageModifyPermissions(_PAGE(a), pages, PC_USER | PC_WRITEABLE, PC_USER | PC_WRITEABLE);
		//_PageModifyPermissions(_PAGE(a), pages, PC_WRITEABLE, PC_WRITEABLE);
		
		if(pphy != NULL)
		{
			*pphy = phy;
		}
		
	}
	
	return a;
}

BOOL smp_init()
{
	isr_desc32_t *isr;
	mp_processor_t *cpu;
	int ap_count = 0;
	uint32_t lapic = 0;
	int i;
	
	mpfp = smp_get_mpfp();
	
	if(mpfp != NULL)
	{
		DWORD off;
		mpct_t *ct = (mpct_t *)(smp_first_mb + mpfp->configuration_table);
		
		dbg_printf("mpfp->configuration_table = %lX\n", mpfp->configuration_table);
		
		if(memcmp(ct->signature, MPCT_MAGIC, 4) != 0)
		{
			dbg_printf("Invalid PCMP signature!\n");
			return NULL;
		}
				
		ttable = (titem_t*)_PageCode(RoundToPages(sizeof(titem_t)*MAX_CORES), 1, NULL);
		kernel = (uint8_t*)_PageCode(1, 0, NULL);
		if(kernel == 0)
		{
			dbg_printf("cannot allocate kernel\n");
			return FALSE;
		}
		dbg_printf("kernel alloc in %lX\n", kernel);
		
		memcpy(kernel, apkernel, apkernel_len);
		kernel_flat = (DWORD)kernel;
		/* relocate ISRs */
		isr = (isr_desc32_t*)(kernel + AP_KERNEL_ISR_OFF);
		for(i = 0; i < MAX_CORES; i++)
		{
			DWORD a = ISR_MERGE_ADDRESS(isr);
			a = (a - AP_KERNEL_BASE) + kernel_flat;
			ISR_SPLIT_ADDRESS(isr, a);
			isr++;
		}
		
		dbg_printf("mpfp->configuration_table = %lX\n", mpfp->configuration_table);
		
		dbg_printf("going to list CPU: %d entries\n", ct->entry_count);
		off = sizeof(mpct_t);
		//for(i = 0; i < ct->entry_count;)
		while(off < ct->length)
		{
			cpu = (mp_processor_t *)(smp_first_mb + mpfp->configuration_table + off);
			if(cpu->type == 0)
			{
				if((cpu->flags & 0x1) != 0) /* cpu usable */
				{
					if((cpu->flags & 0x2) == 0) /* is AP */
					{
						uint32_t tss_flat;
						uint32_t phy = 0;
						void *mem = NULL;
						uint8_t apid = cpu->local_apic_id;
						dbg_printf("Found AP: %u\n", apid);
						
						ttable[apid].stack = _PageCode(STACK_PAGES, 1, NULL);
						memset((void*)(ttable[apid].stack), 0xCC, STACK_SIZE);
						
						mem = (void*)_PageCode(DATA_PAGES, 1, &phy);
						
						ttable[apid].data = mem;

						memset(ttable[apid].data->gdt, 0, 32*sizeof(uint32_t));
						memcpy(ttable[apid].data->gdt, def_gdt, sizeof(def_gdt));
						
						tss_flat = (uint32_t)ttable[apid].data->tss;
						
						ttable[apid].data->gdt[11] |=  (tss_flat & 0xFF000000);
						ttable[apid].data->gdt[11] |= ((tss_flat & 0x00FF0000) >> 16);
						ttable[apid].data->gdt[10] |= ((tss_flat & 0x0000FFFF) << 16);

						memset(ttable[apid].data->tss, 0, 128);

						ttable[apid].data->tss[1] = ttable[apid].stack + P_SIZE; // esp0
						ttable[apid].data->tss[2] = 0x10; // ss0
						ttable[apid].data->tss[16] = (104 << 16); // IOPB 

						ttable[apid].data->cpu_cr3 = phy + P_SIZE;
						ttable[apid].data->pd = (uint32_t*)(((uint8_t*)mem) + P_SIZE);

						ttable[apid].data->entry = (uint32_t)smp_ap_main;
						ttable[apid].data->status = S_BUSY;
						
						ap_count++;
					}
					else
					{
						dbg_printf("Found BSP: %u\n", cpu->local_apic_id);
					}
				}
				off += MP_SIZE_PROCESSOR;
			}
			else
			{
				//dbg_printf("Found APIC %u\n", cpu->type);
				off += MP_SIZE_APIC;
			}
		}
		
		lapic = ct->lapic_address;
	}
	else
	{
		dbg_printf("can't find MPFP\n");
	}
	
	if(ap_count > 0)
	{
		cpu_count = ap_count + 1;
		dbg_printf("AP %u x, going to init bsp\n", ap_count);
		return smp_init_bsp(ttable, kernel, lapic);
	}
	else
	{
		dbg_printf("There no AP\n");
	}
	
	cpu_count = 1;
	
	return FALSE;
}


