; int_utils.s
; Derived from:
; https://github.com/dweinstein/vmxos-osx/blob/e603381a0ba2b63bfd2ea3d1bbf95e992d202762/src/kernel/interrupt/interrupt_hw.asm


 extern _int_syscall
 extern _keyboard_handler
 extern _int_exception
 extern _int_error
 
 ; Function for starting an infinite loop
 global _hang
 _hang:						; Infinite Loop mechanism :)
	hlt						; Halt
	jmp Hang				; And Stay like that...
	; And Apple thought they were having fun with Infinite Loops...
 
 global _int48
 _int48:
	push eax
	push ds
	push es
	call _int_syscall
	pop es
	pop ds
	pop eax
	iret
 
 global _intHW
 _intHW:
	push eax
	push ds
	push es
	call _keyboard_handler
	pop es
	pop ds
	pop eax
	iret
 
%macro _intERR 1
 global _int%1
 _int%1:
	mov eax, %1
	push eax
	push ds
	push es
	call _int_error
	pop es
	pop ds
	pop eax
	iret
%endmacro
 
%macro _intEXC 1
 global _int%1
 _int%1:
	mov eax, %1
	push byte 0
	push eax
	push ds
	push es
	call _int_exception
	pop es
	pop ds
	pop eax
	iret
%endmacro


_int_exception		0	; "#00 Divide By Zero Error",
_int_exception		1	; "#DB Debug Error",
_int_exception		2	; "#-- NMI Interrupt",
_int_exception		3	; "#BP Breakpoint",
_int_exception		4	; "#OF Overflow",
_int_exception		5	; "#BR BOUND Range Exceeded"
_int_exception		6	; "#UD Invalid Opcode",
_int_exception		7	; "#NM Device Not Available",
_int__int_error		8	; "#DF Double Fault",
_int_exception		9	; "#-- Coprocessor Segment Overrun",
_int__int_error		10	; "#TS Invalid TSS",
_int__int_error		11	; "#NP Segment Not Present",
_int__int_error		12	; "#SS Stack Segment Fault",
_int__int_error		13	; "#GP Gneral Protection Fault",
_int__int_error		14	; "#PF Page Fault",
_int_exception		15	; "#15 reserved",
_int_exception		16	; "#MF FPU Floating-Point Exception",
_int__int_error		17	; "#AC Alignment Check",
_int_exception		18	; "#MC Machine Check",
_int_exception		19	; "#XF SIMD Floating-Point Exception",
_int_exception		20	; "#20 reserved",
_int_exception		21	; "#21 reserved",
_int_exception		22	; "#22 reserved",
_int_exception		23	; "#23 reserved",
_int_exception		24	; "#24 reserved",
_int_exception		25	; "#25 reserved",
_int_exception		26	; "#26 reserved",
_int_exception		27	; "#27 reserved",
_int_exception		28	; "#28 reserved",
_int_exception		29	; "#29 reserved",
_int_exception		30	; "#SX Security Exception",
_int_exception		31	; "#31 reserved",