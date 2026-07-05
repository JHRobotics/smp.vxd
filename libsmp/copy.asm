format MS COFF

section '.text' code readable executable

public _fast_memset
_fast_memset:
	pushfd
	push edi
	cld
	; [esp+ 0] old edi
	; [esp+ 4] old eflags
	; [esp+ 8] return
	; [esp+12] dest
	; [esp+16] pattern
	; [esp+20] num
	mov eax,[esp+16]	
	; expand 0x??????XY to 0xXYXYXYXY
	mov ah,al
	mov ecx,eax
	bswap eax
	mov ax,cx

	mov edi, [esp+12]
	mov ecx, [esp+20]
	shr ecx, 2
	rep stosd

	mov ecx, [esp+20]
	and ecx, 0x3
	rep stosb

	mov eax, [esp+12] ; std c return value

	pop edi
	popfd
	
	ret
