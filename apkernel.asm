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
;                                 kernel
;
;
; Code layout
org_boot_addr equ 0xC0000000
org_is_bsp    equ 0xC0000200
org_fly       equ 0xC0000300
org_land      equ 0xC0000400
org_reattach  equ 0xC0000500
org_int       equ 0xC0000600
org_data      equ 0xC0000700
org_idt       equ 0xC0000800

fn_block_size equ 0x100

; tagCRS_32
Client_EDI    equ 0x00
Client_ESI    equ 0x04
Client_EBP    equ 0x08
Client_EBX    equ 0x10
Client_EDX    equ 0x14
Client_ECX    equ 0x18
Client_EAX    equ 0x1C
Client_Error  equ 0x20
Client_EIP    equ 0x24
Client_CS     equ 0x28
Client_EFlags equ 0x2C
Client_ESP    equ 0x30
Client_SS     equ 0x34
Client_ES     equ 0x38
Client_DS     equ 0x3C
Client_FS     equ 0x40
Client_GS     equ 0x44

; tdata
tdata_status     equ 0
tdata_thread_id  equ 4
tdata_entry      equ 8
tdata_cpu_cr3    equ 12
tdata_proc_cr4   equ 16
tdata_pd         equ 20
tdata_index      equ 24
tdata_gdt        equ 32
tdata_init_state equ 160
tdata_proc_state equ 288
tdata_proc_tss   equ 416
tdata_fpu_state  equ 544

; AP state
S_BUSY    equ 0
S_READY   equ 1
S_SLEEP   equ 2
S_LOADED  equ 3
S_RUNNING equ 4
S_CARGO   equ 5
S_DISCARD equ 6

; segments
sys_cs     equ 0x8
sys_ds     equ 0x10
user_cs    equ (0x18 or 3)
user_ds    equ (0x20 or 3)

; isr types
int_gate   equ 0x8E00
trap_gate  equ 0x8F00
sw_gate    equ 0xEF00

; eflags manipulation
eflags_set equ 0x00003200
; ^ IOPL IF
eflags_clr equ 0x001F4100
; VIP VIF AC VM RF NT IF
eflags_mask equ (eflags_set or eflags_clr)

; point reg to ap_data, eat flags 
macro GetData reg
{
	local m_start
	local m_call

	m_start:
	call m_call
	m_call:
	pop reg
	add reg, (ap_data - m_call)
}

; get APID into EBX, eat EAX, ECX, EDX
macro GetAPID
{
	mov     eax, 1
	cpuid
	shr    ebx, 24
}

; item in IDT
macro IDTEntry isr, type
{
	dd ((isr and 0x0000FFFF) or (sys_cs shl 16))
	dd ((isr and 0xFFFF0000) or type)
}

use32

; Startup
; equred params from apboot:
;   ecx: sys_start
;   esp: sys_stack
;   esi: bsp_id
;   edi: ttable
org org_boot_addr
_kernel_boot:
	jmp skip_header
	rb 14
	db "JH APC/S 2026.0 "
	skip_header:
	mov ebp, ecx
	add ebp, ap_data-_kernel_boot    ; epb -> ap_data
	mov [ebp+(bsp_id-ap_data)], esi  ; save bsp_id
	mov [ebp+(ttable-ap_data)], edi  ; save ttable ptr

	push ecx
	GetAPID
	pop ecx
	push ebx                         ; save ebx to arg to call
	
	shl ebx, 3                       ; sizeof(titem) = 8 bytes
	add edi, ebx                     ; eax -> [titem]
	mov esi, [edi+4]                 ; esi -> [tdata]
	
	lea edx, [esi+tdata_entry]       ; esi -> [entry]
	mov ebx, esi                     ; ebx -> [status]
	add esi, tdata_init_state        ; esi -> [init_state]
	
	mov [esi+Client_EBX], ebx        ; save EBX
	mov [esi+Client_EDX], edx        ; save EDX

	add ecx, goentry-_kernel_boot    ; move saved EIP to goentry
	mov [esi+Client_EIP], ecx        ; save EIP

	mov word [esi+Client_CS], sys_cs ; save CS
	mov ax, ss
	mov [esi+Client_SS], ax          ; save SS
	pushfd
	pop eax
	or  eax, 0x3000                  ; set IOPL
	push eax
	popfd
	mov [esi+Client_EFlags], eax     ; save EFlags
	mov ax, ds
	mov [esi+Client_DS], ax
	mov ax, es
	mov [esi+Client_ES], ax
	mov ax, fs
	mov [esi+Client_FS], ax
	mov ax, gs
	mov [esi+Client_GS], ax
	
	; exchange order on stack
	pop  eax ; -> APID
	push ebx ; <- &status
	push edx ; <- &idle_task
	push eax ; <- APID
	
	mov [esi+Client_ESP], esp    ; save ESP
	sti

	; unlock AP lock
	mov eax, S_READY
	lock xchg [ebx], eax
	
	goentry:
		call dword [edx] ; int __cdecl idle_task(uint32_t apid)
		
		test eax, eax
		jz skip_st
			call _switchtask
		skip_st:
		; restore regs from stack
		mov edx, [esp+4]
		;mov ebx, [esp+8]
		jmp goentry


; Switch AP do context in proc_state.
; Must be called from RING-0 AP code.
; 
_switchtask:
	cli
	GetData esi
	GetAPID
	
	shl ebx, 3 ; ebx * 8
	mov edi,[esi+(ttable-ap_data)]
	add edi, ebx
	mov esi, [edi+4] ; data
	
	mov eax, [esi+tdata_cpu_cr3]
	mov cr3, eax ; (re)load task space
	
	add esi, tdata_proc_state ; proc_state
	
	; restore iret stack
	mov   eax, user_ds
	push  eax
	mov   eax, [esi+Client_ESP]
	push  eax

	mov   eax, [esi+Client_EFlags]
	or    eax, eflags_set
	and   eax, not eflags_clr
	push  eax ; iret eflags
	
	mov   eax, user_cs ; user code segment
	;mov   eax, sys_cs
	push  eax ; iret cs
	
	mov   eax, [esi+Client_EIP]
	push  eax ; iret eip
	
	; restore general registry (popad)
	mov   eax, [esi+Client_EAX]
	push 	eax
	mov   eax, [esi+Client_ECX]
	push 	eax
	mov   eax, [esi+Client_EDX]
	push 	eax
	mov   eax, [esi+Client_EBX]
	push 	eax
	mov   eax, [esi+Client_ESP] ; but ignored by popad
	push 	eax
	mov   eax, [esi+Client_EBP]
	push 	eax
	mov   eax, [esi+Client_ESI]
	push 	eax
	mov   eax, [esi+Client_EDI]
	push 	eax
	
	; restore FPU/MMX/SSE state
	add   esi, tdata_fpu_state-tdata_proc_state ; fpu_state
	fxrstor [esi]
	
	; set all data segments
	mov eax, user_ds
;	mov ss, ax
	mov ds, ax
	mov es, ax
	mov gs, ax
	; set null to fs segment
	xor eax, eax
	mov fs, ax
	
	; switch to context
	popad	
	iret
	
;	add esp, 5*4
;	ret

; Save current state to proc_state and then
; restore restore CPU to init_state state
;
switch_back:
	; [ebp+4] - EIP from INT
	; [esi] - ap_data
	cld
	GetAPID
	shl ebx, 3
	mov edi, [esi+(ttable-ap_data)]
	add edi, ebx     ; titem
	mov ebx, [edi+4] ; data
	
	; fix ebp
	mov eax, [ebp]
	mov [ebp-6*4], eax
	
	; 
	; save proc state
	;
	lea edi, [ebx+tdata_proc_state]
	; EFLAGS
	mov eax, [ebp+12]
	and eax, (not eflags_mask)
	mov edx, [edi+Client_EFlags]
	and edx, eflags_mask
	or  eax, edx
	mov [edi+Client_EFlags], eax
	; ESP
	mov eax, [ebp+16] 
	mov [edi+Client_ESP], eax
	; EIP
	mov eax, [ebp+4] 
	mov [edi+Client_EIP], eax

	mov ecx, 8
	lea esi, [ebp-(8*4)]
	rep movsd
	
	;
	; save FPU state
	;
	lea edi, [ebx+tdata_fpu_state]
	fxsave [edi]
	
	;
	; load init state
	;
	; use original sp
	lea  esi, [ebx+tdata_init_state]
	mov  esp, [esi+Client_ESP]
	; iret stack
	mov  eax, [esi+Client_EFlags]
	push eax
	mov  eax, sys_cs
	push eax
	mov  eax, [esi+Client_EIP]
	push eax
	; restore general registers
	mov  eax, [esi+Client_EAX]
	push eax
	mov  eax, [esi+Client_ECX]
	push eax
	mov  eax, [esi+Client_EDX]
	push eax
	mov  eax, [esi+Client_EBX]
	push eax
	sub  esp, 4
	mov  eax, [esi+Client_EBP]
	push eax
	mov  eax, [esi+Client_ESI]
	push eax
	mov  eax, [esi+Client_EDI]
	push eax
	
	;
	; segments
	;
	mov eax, sys_ds
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax

	popad

	;mov eax, [esp]
	;mov ebx, [esp+4]
	;mov ecx, [esp+8]
	;mov edx, [esp+12]
	;mov esi, [esp+16]
	;mov eax, [esp+12]
	;mov ebx, [eax]

	iret

	;;; PAD	
	rb (2*fn_block_size) - $ + _kernel_boot

;
; int __cdecl is_bsp()
;
; @return: 0 if current CPU is BSP
;
org org_is_bsp
_is_bsp:
	GetData eax
	push esi
	push ebx
	mov esi, eax
	
	GetAPID
	mov    edx, ebx
	xor    eax, eax
	cmp    ebx, [esi+(bsp_id-ap_data)]
	jnz    not_bsp
		mov  eax, 1

	not_bsp:
	pop ebx
	pop esi
	ret
	
	;;; PAD
	rb (fn_block_size shr 1) - $ + _is_bsp
	
_cpuindex:
	push ebx
	GetAPID
	GetData ecx
	cmp    ebx, [esi+(bsp_id-ap_data)]
	jnz    not_bsp
		mov  eax, eax
		jmp cpuindex_done

	shl ebx,  3 ; ebx * 8
	mov ecx, [ecx+(ttable-ap_data)]
	add ecx,  ebx      ; table[apid]
	mov ebx, [ecx+4]   ; table[apid]->data
	mov eax, [ebx+tdata_index]
	
	cpuindex_done:
	pop ebx
	ret
	rb (fn_block_size shr 1) - $ + _cpuindex
;
; void __cdecl fly();
;
; Switch task to another CPU. NOP when CPU in not BSP, saves all
;
org org_fly
_fly:
	pushfd
	push eax
	push ecx
	push edx
	call _is_bsp
	test eax, eax
	jz skip_fly
		pop edx
		pop ecx
		pop eax
		popfd
		int3
		ret
	skip_fly:
	pop edx
	pop ecx
	pop eax
	popfd
	ret

	;;; PAD
	rb fn_block_size - $ + _fly

;
; void __cdecl land();
;
; Switch task back to BSP, NOP when CPU is BSP, saves all
;
org org_land
_land:
	pushfd
	push eax
	push ecx
	push edx
	call _is_bsp
	test eax, eax
	jnz skip_land
		pop edx
		pop ecx
		pop eax
		popfd
		int3
		ret
	skip_land:
	pop edx
	pop ecx
	pop eax
	popfd
	ret

	;;; PAD
	rb fn_block_size - $ + _land

;
; void __cdecl reattach();
;
; Called by BSP idle function, when AP is done, this load
; AP context and return it to BSP thread. When AP is not ready yet
; NOP.
;
org org_reattach
_reattach:
	call _is_bsp
	test eax, eax
	jz skip_reattach
		int3
	skip_reattach:
	ret
	
	;;; PAD
	rb (fn_block_size shr 1) - $ + _reattach
	
;
; Call idle function (and restart it when returs)
; params:
;   eax - pointer to lock
;   ecx - pointer to idle function
;
_trampoline:
	push ecx
	push eax
	trampoline_loop:
		mov ecx, [esp+4] ; -> void __cdecl idle_func(uint32_t *lock)
		call ecx
		jmp trampoline_loop

	;;; PAD
	rb (fn_block_size shr 1) - $ + _trampoline

;
; ISRs (IDT entries points here)
;
org org_int
; no action
; can be also used as trampoline to process so leave it as first entry
_dummy_isr:
	iret

_error_isr_code:
	add esp, 4

_error_isr:
;	mov esi,[esp]
;	mov edi,[esp+4]
;	ll: jmp ll
	push  ebp
	mov   ebp, esp
	pushad
	GetData esi
	jmp switch_back

_int3_isr:
	push  ebp
	mov   ebp, esp
	pushad
	
	GetData esi
	mov ecx, esi
	
	sub ecx, ap_data-_land
	mov edx, [ebp+4] ; original EIP
	cmp edx, ecx
	jb skip
	add ecx, fn_block_size
	cmp edx, ecx
	jae skip
	jmp switch_back
	skip:
		; mov EIP one byte back, so when code is execuded on BSP it'll be exec system handler
		mov eax, [ebp+4]
		dec eax
		mov [ebp+4], eax
		jmp switch_back
		
	;;; PAD
	rb fn_block_size - $ + _dummy_isr

org org_data
ap_data:
	bsp_id: dd 0xDDDDDDDD
	ttable: dd 0xEEEEEEEE

	;;; PAD
	rb fn_block_size - $ + ap_data

;
; IDT (1536 bytes)
;
org org_idt
_idt:
; 0x00 	0 	#DE 	Fault 	No 	Divide Error 	DIV and IDIV instructions.
IDTEntry _error_isr, int_gate
; 0x01 	1 	#DB 	Trap 	No 	Debug Exception 	Instruction, data, and I/O breakpoints; single-step; and others.
IDTEntry _error_isr, int_gate
; 0x02 	2 	NMI 	Interrupt 	No 	NMI Interrupt 	Nonmaskable external interrupt.
;IDTEntry _error_isr, int_gate
; We use it to wakeup CPU, no other action needed
IDTEntry _dummy_isr, int_gate

; 0x03 	3 	#BP 	Trap 	No 	Breakpoint 	INT3 instruction.
; request task switch if in specific range
IDTEntry _int3_isr, sw_gate

; 0x04 	4 	#OF 	Trap 	No 	Overflow 	INTO instruction.
IDTEntry _error_isr, trap_gate
; 0x05 	5 	#BR 	Fault 	No 	BOUND Range Exceeded 	BOUND instruction.
IDTEntry _error_isr, int_gate
; 0x06 	6 	#UD 	Fault 	No 	Invalid Opcode (Undefined Opcode) 	UD instruction or reserved opcode.
;IDTEntry _error_isr, int_gate
IDTEntry _error_isr, int_gate

; 0x07 	7 	#NM 	Fault 	No 	Device Not Available (No Math Coprocessor) 	Floating-point or WAIT/FWAIT instruction.
IDTEntry _dummy_isr, int_gate
; 0x08 	8 	#DF 	Abort 	Yes (zero) 	Double Fault 	Any instruction that can generate an exception, an NMI, or an INTR.
IDTEntry _error_isr_code, int_gate
; 0x09 	9 	N/A 	Fault 	No 	Coprocessor Segment Overrun (reserved) 	Floating-point instruction.
IDTEntry _error_isr, int_gate
; 0x0A 	10 	#TS 	Fault 	Yes 	Invalid TSS 	Task switch or TSS access.
IDTEntry _error_isr_code, int_gate
; 0x0B 	11 	#NP 	Fault 	Yes 	Segment Not Present 	Loading segment registers or accessing system segments.
IDTEntry _error_isr_code, int_gate
; 0x0C 	12 	#SS 	Fault 	Yes 	Stack-Segment Fault 	Stack operations and SS register loads.
IDTEntry _error_isr_code, int_gate
; 0x0D 	13 	#GP 	Fault 	Yes 	General Protection 	Any memory reference and other protection checks.
IDTEntry _error_isr_code, int_gate
; 0x0E 	14 	#PF 	Fault 	Yes 	Page Fault 	Any memory reference.
;IDTEntry _error_isr, int_gate
IDTEntry _error_isr_code, int_gate

; 0x0F 	15 	N/A 	No 	Intel reserved. Do not use.
dd 0, 0
; 0x10 	16 	#MF 	Fault 	No 	x87 FPU Floating-Point Error (Math Fault) 	x87 FPU floating-point or WAIT/FWAIT instruction.
IDTEntry _error_isr, int_gate
; 0x11 	17 	#AC 	Fault 	Yes (zero) 	Alignment Check 	Any data reference in memory.
IDTEntry _error_isr_code, int_gate
; 0x12 	18 	#MC 	Abort 	No 	Machine Check 	Error codes (if any) and source are model dependent.
IDTEntry _error_isr, int_gate
; 0x13 	19 	#XM 	Fault 	No 	SIMD Floating-Point Exception 	SSE/SSE2/SSE3 floating-point instructions
IDTEntry _error_isr, int_gate
; 0x14 	20 	#VE 	Fault 	No 	Virtualization Exception 	EPT violations
IDTEntry _error_isr, int_gate
; 0x15 	21 	#CP 	Fault 	Yes 	Control Protection Exception 	RET, IRET, RSTORSSP, and SETSSBSY instructions can generate this exception. When CET indirect branch tracking is enabled, this exception can be generated due to a missing ENDBRANCH instruction at target of an indirect call or jump. 
IDTEntry _error_isr_code, int_gate
; 0x16 - 0x31
repeat 10
  IDTEntry _dummy_isr, int_gate
end repeat
repeat 224
  IDTEntry _error_isr, int_gate
end repeat
