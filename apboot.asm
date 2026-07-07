; ******************************************************************************
; * Copyright (c) 2026 Jaroslav Hensl <emulator@emulace.cz>                    *
; *                                                                            *
; * Permission is hereby granted, free of charge, to any person obtaining a    *
; * copy of this software and associated documentation files (the "Software"), *
; * to deal in the Software without restriction, including without limitation  *
; * the rights to use, copy, modify, merge, publish, distribute, sublicense,   *
; * and/or sell copies of the Software, and to permit persons to whom the      *
; * Software is furnished to do so, subject to the following conditions:       *
; *                                                                            *
; * The above copyright notice and this permission notice shall be included in *
; * all copies or substantial portions of the Software.                        *
; *                                                                            *
; * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR *
; * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   *
; * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    *
; * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER *
; * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    *
; * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        *
; * DEALINGS IN THE SOFTWARE.                                                  *
; ******************************************************************************
;
;
;                             JH APC/S 2026.0
;                                bootloader
;
;
; Needs be placed at 0x00008000 on physical address,
; when kernel is loaded is posible to discard this code.
;
; After load, kernel is full position independent.
;

;
; STEP I: from real mode to PM32 (PG=0)
;
use16
align 1

org 0x8000
ap_boot16:
	cli
	cld
	xor    ax, ax
	mov    ds, ax
	; temporary GDT when is PG=0
	lgdt   [GDT_value_TMP]
	; load vars to regs
	mov    eax, [sys_cr3]
	mov    ecx, [sys_start]
	mov    esp, [sys_stack]
	mov    esi, [bsp_id]
	mov    edi, [ttable]
	mov    cr3, eax
	mov    eax, cr0
	; enable PE (but not PG yet)
	or     eax, 0x00000001
	mov    cr0, eax
	; jump to PM
	jmp far 0x8:0x8100
	; pad
  rb 0x100 - $ + ap_boot16

;
; STEP II: enable pagging
;
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
	or     eax, 0x80000022 ; PG MP NE
;	mov    cr0, eax

;	mov    eax, cr0
	and    eax, 0xFFFEFFFB ; -WP -EM
	mov    cr0, eax

;
; STEP III: point tables to paged linear addresses
;
  ; (all )
	; load new GDT (on virtual addresses)
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

;
; STEP IV: pass control to the kernel
;
	; we need another far jmp to load new GDT
	db 0xEA ; jmp far
far_jmp_address:
	dd 0x0
	dw 0x0008
  rb 0x80 - $ + ap_boot32

;
; variables, sets by BSP before execution
;
org 0x8180
ap_vars:
GDT_value:
	dw 47                         ; GDT size-1  (6*64 bit)
	dd 0xDDDDDDDD                 ; GDT address (linear PG=1)
IDT_value:
	dw 1535                       ; IDT size-1 (256*6 bytes)
	dd 0x11111111                 ; IDT address (linear PG=1)
sys_cr3:   dd 0xC3C3C3C3        ; PD for kernel boot (physical address)
sys_start: dd 0xAAAAAAAA        ; kernel position (linear address, PG=1)
sys_stack: dd 0xDDDDDDDD        ; stack top for kernel
bsp_id:    dd 0x1D1D1D1D        ; ID of BSP (usually: 0) 
ttable:    dd 0xEEEEEEEE        ; pointer to ttable (AP individual data: ttable[APID])

; temporary GDT (PG=0)
GDT_table_TMP:
	dd 0, 0
	dd 0x0000FFFF, 0x00CF9A00    ; flat code
	dd 0x0000FFFF, 0x00CF9200    ; flat data
	dd 0x00000067, 0x00CF8900    ; tss
GDT_value_TMP:
  dw GDT_value_TMP - GDT_table_TMP - 1
	dd GDT_table_TMP
	dd 0, 0

	; pad
  rb 0x7C - $ + ap_vars
  dd 0xFFFFFFFF
