; bootloader.asm - Stage 2 of the new Bootloader
; The third paradigm shift in TXP-VOS Development
; An attempt at a fresh start, using a more refined approach - compartmentalizing all capabilities.
; I shall not repeat myself - read the comment block from bootloader.s for a reason supporting this rewrite
; You know the rules
; version - 0.0.6_r2 InDev
; http://arjunsreedharan.org/post/82710718100/kernel-101-lets-write-a-kernel
; http://arjunsreedharan.org/post/99370248137/kernel-201-lets-write-a-kernel-with-keyboard


; Definitions and Includes for later use...
  %include "gdt.inc"

  [BITS 16]					; The bit-ness only changes when the kernel is called in - not yet...


; Where Stage 1 left off, Stage 2 begins...
  global _kboot
  _kboot:
	call _GDTBitness
	
  global _kbContinue
  _kbContinue:
	call _enableATwenty
	call _DisableOldPaging
	
	mov eax, cr0 
	or al, 1     ; set PE (Protection Enable) bit in CR0 (Control Register 0)
	mov cr0, eax

	; Perform far jump to selector 08h (offset into GDT, pointing at a 32bit PM code segment descriptor) 
	; to load CS with proper PM32 descriptor)
	jmp 08h:PModeMain
	; [...]


; Function(s) for printing a prepared ASCIIZ text chain @es:si
  Print:
	lodsb					; Load value of es:si into @al
	or		al, al			; Stop printing if @al is the terminating char
	jz		PrintDone
	mov		ah,	0x0E
	int		0x10
	jmp		Print			; Loop until all chars have been handled in the current text chain
  PrintDone:
	ret


  PModeMain:
	mov ax, 0x10
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax
	mov esp, 0x9000
	; load DS, ES, FS, GS, SS, and ESP with corresponding values for PM
	; hope I didn't screw up...
	; We are now in protected mode - good bye real mode...

; Function for Enabling the A20, by DeveloperTIG - Rohitab
; External code segment from:
; http://www.rohitab.com/discuss/topic/38969-nasm-how-to-set-up-a-functional-idt/
  global _enableATwenty
  _enableATwenty:
	pusha
	mov  ax,  0x2401
	int  0x15
	popa
	
	pusha
	cli
	in  al , 0x92
	or  al , 2
	out   0x92 , al
	sti
	popa


; Now to initiate the handling of exceptions - let's start eh_frames
  section .eh_frames
; TODO - end with 4 bytes of zeros - resb, .long?
  resb 4

; A new environment variable has been made - enter, the Stack
  section .bootstrap_stack, "aw", nobits
  align 4
; Time to set up a stack. That's right - I said SET UP a stack, not USE an EXISTING stack.
; That's because the current stack pointer, ESP, points to pretty much anything - using that could break something.
; Instead, I'll use a temporary stack, to enforce predictability and safety. It was originally 64 kb (kilobytes) - resb 67108864.
; Now, I have reduced it to 8 kb, so that this implementation concurs with OS-Dev Standards observed on the internet for most tutorials (4-8 kb in most examples read).
; Also, the section was originally, 'tbs' - it's now called bss.
  section .bss
  stack_bottom:
  resb 8192
  stack_top:
; The ESP stack will be loaded at 'stack_top' soon

; The linker script will specify _start as the entry point to the kernel and the
; bootloader will jump to this position once the kernel has been loaded. It
; won't make sense to return from this function as the bootloader will be gone.

  section .text
; The kernel is called from this function
  global _start
; type _start, @function
  _start:
	; Welcome to kernel mode! We now have sufficient code for the bootloader to
	; load and run our operating system. It doesn't do anything interesting yet.
	; Let's set stack pointer to stack_space
	mov esp, stack_top
	; C and C++ can call ASM - Yay!
	; But this doesn't go both ways - extern anyone?
	
	; External code block - partial Interrupt support, from the links above...
	global keyboard_handler
	global read_port
	global write_port
	global load_idt
	
	; kmain is defined in the cpp file
	extern kmain
	call kmain
	; Since the external C++ function might have a return, we'll want to stick
	; the machine into an infinite loop. Strange directions, right?
	; In order to accomplish this, we've already cleared interrupts, which has
	; disabled interrupts - this happened when you entered 'protected mode'.
	; This also prevents the System Timer INT from throwing an ambiguous 
	; Divide-by-Zero error, which would kill this OS without ISR_IVT_IDT setup.
	; Since we did that already, lets halt the CPU until the next interrupt
	; arrives. Then, have the machine jump to halt if execution continues.


; Function for starting an infinite loop
  Hang:						; Infinite Loop mechanism :)
	hlt						; Halt
	jmp Hang				; And Stay like that...
	; And Apple thought they were having fun with Infinite Loops...


; New definitions, for handling ports - I/O instructions
 read_port:
	mov edx, [esp + 4]
	in al, dx	
	ret

 write_port:
	mov   edx, [esp + 4]    
	mov   al, [esp + 4 + 4]  
	out   dx, al  
	ret


; For loading the IVT
 load_idt:
	mov edx, [esp + 4]
	lidt [edx]
	sti
	ret

; Keyboard Interrupt Handler is now called from the Kernel, not the Bootloader :)


; Set the size of the _start symbol to the current location '.' minus its start.
; This is useful when debugging or when you implement call tracing.
; .size _start, . - _start