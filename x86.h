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

#pragma pack(pop)

#endif /* __X86_H__INCLUDED__ */
