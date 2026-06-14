format MS COFF

JACKPOT equ 0xAAAAAAAA

extrn '__imp__InitializeCriticalSection@4' as InitializeCriticalSection:dword
extrn '__imp__EnterCriticalSection@4' as EnterCriticalSection:dword
extrn '__imp__LeaveCriticalSection@4' as LeaveCriticalSection:dword
extrn '__imp__DeleteCriticalSection@4' as DeleteCriticalSection:dword
extrn '__imp__Sleep@4' as Sleep:dword
extrn '__imp__printf' as printf:dword

extrn '_cs_create' as cs_create
extrn '_cs_get' as cs_get
extrn '_cs_delete' as cs_delete
extrn '_TryEnterCriticalSection9x' as TryEnterCriticalSection9x

macro pub fn
{
	public fn
	fn:
}

section '.text' code readable executable

pub _smp9x_InitializeCriticalSection
	mov ecx, [esp+4]
	push ecx
	call cs_create
	pop ecx
	test eax,eax
	jz ics_system
		mov ecx, JACKPOT
		lock xchg [eax], ecx
		; also initialize CS by system for sure
	ics_system:
	jmp [InitializeCriticalSection]

pub _smp9x_TryEnterCriticalSection
	mov ecx, [esp+4]
	push ecx
	call cs_get
	pop ecx
	test eax, eax
	jz tecs_system
		xor edx, edx
		lock xchg [eax], edx
		cmp edx, JACKPOT
		jz tecs_critical
		xor eax, eax
		ret 4
	
	tecs_critical:
		mov eax, 1
		ret 4
	
	tecs_system:
		;mov ecx, [esp+4]
		push ecx
		call TryEnterCriticalSection9x
		mov eax, 1
		ret 4


pub _smp9x_EnterCriticalSection
	mov ecx, [esp+4]
	push ecx
	call cs_get
	add esp, 4
	test eax, eax
	jz ecs_system
	
	ecs_loop:
		xor edx, edx
		lock xchg [eax], edx
		cmp edx, JACKPOT
		jz ecs_critical
		;push eax
		;push 0
		;call [Sleep]
		;pop eax
		pause
		jmp ecs_loop
	
	ecs_critical:
		xor eax, eax
		ret 4
	
	ecs_system:
		jmp [EnterCriticalSection]

pub _smp9x_LeaveCriticalSection
	mov ecx, [esp+4]
	push ecx
	call cs_get
	pop ecx
	test eax, eax
	jz lcs_system
	mov edx, JACKPOT
	lock xchg [eax], edx
	xor eax, eax
	ret 4
	
	lcs_system:
		jmp [LeaveCriticalSection]

pub _smp9x_InitializeCriticalSectionAndSpinCount
	mov ecx, [esp+4]
	push ecx
	call _smp9x_InitializeCriticalSection
	ret 8

pub _smp9x_DeleteCriticalSection
	;jmp [DeleteCriticalSection]
	mov ecx, [esp+4]
	push ecx
	call cs_delete
	pop ecx
	
	jmp [DeleteCriticalSection]
	ret 4


section '.data' data readable writeable
