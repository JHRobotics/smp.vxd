use16
align 1

org 0x8000
ap_boot16:
	cli
	cld
	xor    ax, ax
	mov    ds, ax
	lgdt   [GDT_value_TMP]
	mov    eax, [sys_cr3]
	mov    ecx, [sys_start]
	mov    esp, [sys_stack]
	mov    esi, [bsp_id]
	mov    edi, [ttable]
	mov    cr3, eax
	mov    eax, cr0
;	or     eax, 0x80000001
	or     eax, 0x00000001
	mov    cr0, eax
	jmp far 0x8:0x8100

  rb 0x100 - $ + ap_boot16

use32
org 0x8100
ap_boot32:
	mov ax, 0x10
	mov ds, ax
	mov ss, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov [far_jmp_address], ecx

	; enable PG
	mov    eax, cr0
	or     eax, 0x80000000 ; PG
	mov    cr0, eax

	mov    eax, cr0
	and    eax, 0xFFFEFFFF ; -WP
	mov    cr0, eax
	
	; load new GDT
	lgdt   [GDT_value]
	lidt   [IDT_value]
	; reload segments
	mov ax, 0x10
	mov ds, ax
	mov ss, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ax, 0x28
	ltr ax
	; we need another far jmp to load new GDT
	db 0xEA ; jmp far
far_jmp_address:
	dd 0x0
	dw 0x0008
  rb 0x100 - $ + ap_boot32

org 0x8200
GDT_value:
	dw 47 ; size - 1 (6 * 2 * 4) 
	dd 0xDDDDDDDD ; GDT address
IDT_value:
	dw 1535 ; (256*6) - 1
	dd 0x11111111 ; IDT address
sys_cr3:   dd 0xC3C3C3C3
sys_start: dd 0xAAAAAAAA
sys_stack: dd 0xDDDDDDDD
bsp_id:    dd 0x1D1D1D1D
ttable:    dd 0xEEEEEEEE

GDT_table_TMP:
	dd 0, 0
	dd 0x0000FFFF, 0x00CF9A00    ; flat code
	dd 0x0000FFFF, 0x00CF9200    ; flat data
	dd 0x00000067, 0x00CF8900    ; tss
GDT_value_TMP:
  dw GDT_value_TMP - GDT_table_TMP - 1
	dd GDT_table_TMP
	dd 0, 0
