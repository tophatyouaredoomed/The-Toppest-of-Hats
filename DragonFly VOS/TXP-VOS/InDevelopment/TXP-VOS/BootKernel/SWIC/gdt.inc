; gdt.asm - the newest addition to TXP-VOS as it moves toward modular design.
; Originally was written into Stage 2, but now has its own source file.
; This should decrease the difficulty of debugging when I finally get around to it...
; Also removes the following dependency - LongModeDirectly.asm
; Based upon sources extracted from:
; http://wiki.osdev.org/Setting_Up_Long_Mode
; http://wiki.osdev.org/Entering_Long_Mode_Directly

; Bitness - 16 until otherwise stated, as usual.
; [BITS 16]


; Definitions and Includes for later use...
  %include "longmode.asm"


  global _GDTBitness
  _GDTBitness:
	call checkCPUID			; Check for CPUID support
	jc CPUIDUnsupported
	
	call _TripleCheck
	push ComeBack
	jmp longModeSupported
	
	; enable the A20 Line here

  ComeBack:
	mov si, ComeBackM
	call Print
	jmp _kbContinue


; Function for if CPUID does not come with this hardware setup
  CPUIDUnsupported:
	mov si, CPUIDUnsupportedM
	call Print
	call KeelOver
	
; Function for if the CPU does happen to support Long mode
  longModeSupported:
	mov si, longModeSupportedM
	call Print
	mov edi, FREE_SPACE				; Point @edi to the predefined free_space bracket
	call _GoLong
	call _LongKernel
	push ComeBack
	jmp CODE_SEG:_LongMode			; Load CS with 64 bit segment and flush the instruction cache
	
; Function for if the CPU does not support Long mode
  longModeUnsupported:
	mov si, longModeUnsupportedM
	call Print
	call _ShortKernel
	push ComeBack


; Text section for if the CPUID support is absent - an error message
  CPUIDUnsupportedM db "ERR:_CPUID_SUPPORT_ABSENT_-_EXITING", 0x0A, 0x0D, 0
; Text section for if the CPU supports Long mode
  longModeSupportedM db "NOTICE:_CPU_LONG_MODE_SUPPORT_PRESENT", 0x0A, 0x0D, 0
; Text section for if the CPU doesn't support Long mode - another error message
  longModeUnsupportedM db "ERR:_CPU_LONG_MODE_SUPPORT_ABSENT", 0x0A, 0x0D, 0
; Text section for when everything Comes Back...
  ComeBackM db "You have returned. Now, on with your journey...", 0x0A, 0x0D, 0

; Global function that checks for CPUID support
; Taken from the following tutorial:
; http://wiki.osdev.org/Setting_Up_Long_Mode
  global checkCPUID
  checkCPUID:
	pushfd
	pop eax
	mov ecx, eax
	xor eax, 1 << 21
	push eax
	popfd
	pushfd
	pop eax
	push ecx
	popfd
	xor eax, ecx
	jz CPUIDUnsupportedF
	
	ret

  CPUIDUnsupportedF:
	stc
	ret

; Global function to check for the 'extended' functionality, check for long mode support, and disable any forms of Paging that are currently enabled
  global _TripleCheck
  _TripleCheck:
	; _ExtFunc
	mov eax, 0x80000000
	cpuid
	cmp eax, 0x80000001
	jb longModeUnsupported
	
	; _DetectLongMode
	mov eax, 0x80000001
	cpuid
	test edx, 1 << 29
	jz longModeUnsupported

	; _DisableOldPaging
	mov eax, cr0
	and eax, 01111111111111111111111111111111b
	mov cr0, eax
	
	ret
	

; Function used to end the boot routine if CPUID support is absent
  KeelOver:
	hlt
	jmp KeelOver


  [BITS 32]
  global _ShortKernel
  _ShortKernel:
	push ComeBack
	jmp GDT32

; Due to the way the 32-Bit GDT is setup here, with code and data clearly separated for simplicity, whatever we load at 4MB will appear as CS:0, and so on...
; Probably not the best design, but we're doing this simply as a hobbyist OS in its beginning stages :) 

; Experimental, hand-written 32-bit GDT - will have errors, since I'm converting it from a C-based example...
; Meant only for use on x86 systems, which may benefit from the TaskStateSegment
; http://wiki.osdev.org/GDT_Tutorial
  GDT32:						; Global Descriptor Table (32-bit).
	.Null: equ $ - GDT32		; The null descriptor - 0
	dq 0						; The null descriptor.
	.Code: equ $ - GDT32		; The code descriptor - 1
	dw 0x04000000				; Base
	dw 0x03ffffff				; Limit
	db 0x9A						; Type
	.Data: equ $ - GDT32		; The data descriptor - 2
	dw 0x08000000				; Base
	dw 0x03ffffff				; Limit
	db 0x92						; Type
	
	.uCode: equ $ - GDT32		; The code descriptor - 1
	dw 0x04000000				; Base
	dw 0x03ffffff				; Limit
	db 0x9A						; Type
	.uData: equ $ - GDT32		; The data descriptor - 2
	dw 0x08000000				; Base
	dw 0x03ffffff				; Limit
	db 0x92						; Type
	.TSS: equ $ - GDT32			; The TSS descriptor - probably has an error or two
	dw 0						; Base - placeholder for myTTS
	dw 0						; Limit - placeholder for sizeof(myTTS)
	db 0x89						; Type
	.pointer:					; The GDT-pointer for 32-bit
	dw $ - GDT32 - 1			; Limit for pointer
	dq GDT32					; Base for pointer
;	Here it goes...
	lgdt [GDT32.pointer]		; Load the 32-bit global descriptor table.
	jmp GDT32.Code				; Set the code segment
	
;	Should I be using gdtr, or GDT32?
	cli							; clear and disable interrupts, to avoid any trouble - if I haven't already caused any :P 
	lgdt [GDT32]				; load GDT register with start address of Global Descriptor Table
	ret


  [BITS 64]
  global _LongKernel
  _LongKernel:
	push ComeBack
	jmp GDT64

; Values in this table still need to be populated and converted - I'm on it...
  GDT64:						; Global Descriptor Table (64-bit).
	.Null: equ $ - GDT64		; The null descriptor.
	dw 0						; A standard chain of zeros
	dw 0						; .....
	db 0						; ....
	db 0						; ...
	db 0						; ..
	db 0						; .
	.Code: equ $ - GDT64		; The code descriptor.
	dw 0FFFFh					; Limit (low).
	dw 0						; Base (low).
	db 0						; Base (middle)
	db 10011010b				; Access (exec/read).
	db 00100000b				; Granularity.
	db 0						; Base (high).
	.Data: equ $ - GDT64		; The data descriptor.
	dw 0FFFFh					; Limit (low).
	dw 0						; Base (low).
	db 0						; Base (middle)
	db 10010010b				; Access (read/write).
	db 00000000b				; Granularity.
	db 0						; Base (high).
	
	.uCode: equ $ - GDT64		; The code descriptor.
	dw 0FFFFh					; Limit (low).
	dw 0						; Base (low).
	db 0						; Base (middle)
	db 10011010b				; Access (exec/read).
	db 00100000b				; Granularity.
	db 0						; Base (high).
	.uData: equ $ - GDT64		; The data descriptor.
	dw 0FFFFh					; Limit (low).
	dw 0						; Base (low).
	db 0						; Base (middle)
	db 10010010b				; Access (read/write).
	db 00000000b				; Granularity.
	db 0						; Base (high).
	.Pointer:					; The GDT-pointer.
	dw $ - GDT64 - 1			; Limit.
	dq GDT64					; Base.
	
;	Here it goes...

	lgdt [GDT64.Pointer]		; Load the 64-bit global descriptor table.
;	This last line needs some modifying - already in long mode...
	jmp GDT64.Code				; Set the code segment and enter 64-bit long mode.

	cli							; clear and disable interrupts, to avoid any trouble - if I haven't already caused any :P 
	lgdt [GDT64]				; load GDT register with start address of Global Descriptor Table
	ret