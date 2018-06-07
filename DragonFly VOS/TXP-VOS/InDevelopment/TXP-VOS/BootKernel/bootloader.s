; bootloader.s - Stage 1 of the new Bootloader
; The third paradigm shift in TXP-VOS Development
; An attempt at a fresh start, using a more refined approach - compartmentalizing all capabilities.
; Should be much easier to update and debug over time than its predecessors.
; Keeping as many improvements from 0.0.01 InDev and 4.2 BK Rededign as possible.
; The standards have changed a bit since the last time.
; For future notice, all code is organized using the following scheme:
; 	One newline char seperates sections of code that perform similar or related tasks
; 	Two newline chars seperate distinct code blocks/functions that are unrelated - delineates when a new course of action will be taken
; 	HARD Tab is used to format code that is nested under a specific block/function
; 	Comments are expected to appear at pretty much every line and/or code block - if there is none, ask me for one or Google it
; 	This OS is meant to be heavily verbose - if something important is about to occur, say so during runtime or log it to a file
; 	As usual, Intel syntax is preferred over AT&T style
; version - 0.0.6_r2 InDev


; Definitions and Includes for later use...
%define FREE_SPACE 0x90000

; Starting Memory and Bit-ness Specifiers, for initial functionality on a wide range of CPUs
; Bit-ness will change later on in the Second Stage...
  [ORG 0x7c00]				; I'm loaded into memory, by BIOS, at the address following ORG
; org 7C00h					; Alternative implementation for start address given to BIOS at boot

  [BITS 16]					; Starting out in 16-bits, but this will change soon enough
; bits 16					; Alternative implementation


  jmp BIOSLoader			; The first jump - not as prolific as it sounds...


; OEM Parameter Chunk - Pulled and Modified from BrokenThorn Entertainment's Tutorials
bpbBytesPerSector:  	DW 512
bpbSectorsPerCluster: 	DB 1
bpbReservedSectors: 	DW 1
bpbNumberOfFATs: 	    DB 2
bpbRootEntries: 	    DW 224
bpbTotalSectors: 	    DW 2880
bpbMedia: 	            DB 0xF0
bpbSectorsPerFAT: 	    DW 9
bpbSectorsPerTrack: 	DW 18
bpbHeadsPerCylinder: 	DW 2
bpbHiddenSectors: 	    DD 0
bpbTotalSectorsBig:     DD 0
bsDriveNumber: 	        DB 0
bsUnused: 	            DB 0
bsExtBootSignature: 	DB 0x29
bsSerialNumber:	        DD 0xa0a1a2a3
bsVolumeLabel: 	        DB "TXP BOOT CD"
bsFileSystem: 	        DB "FAT12   "


; Multiboot Header Chunk - Pulled straight from the past attempt...
; constant values:											; Not a function, but some implementaions do it as such - look into it!
	MB_HFLAG_MODALIGN	equ  1<<0							; Align loaded modules on page boundaries
;	.set MB_HFLAG_MODALIGN,  1<<0
	MB_HFLAG_MEMINFO	equ  1<<1							; Memory map
;	.set MB_HFLAG_MEMINFO,   1<<1
	MB_HFLAG_KLUDGE		equ  1<<16							; Use a.out kludge
	FLAGS	equ  MB_HFLAG_MODALIGN | MB_HFLAG_MEMINFO		; this is the Multiboot 'flag' field
;	.set FLAGS,    MB_HFLAG_MODALIGN | MB_HFLAG_MEMINFO
	MAGIC	equ  0x1BADB002									; 'magic value' lets bootloader find the header
;	.set MAGIC,    0x1BADB002
	CHECKSUM    equ -(MAGIC + FLAGS)						; checksum of above, to prove we are multiboot
;	.set CHECKSUM, -(MAGIC + FLAGS)

; Header/section, from MutiBoot Standard - forces this to be in the initial part of the final program
  section .multiboot
  align 4
	dd MAGIC
	dd FLAGS
	dd CHECKSUM

; END MultiBoot Header definitions and declarations - environment variables
; You know what the alternate form would be in a different implementaion


; Where BIOS left off, with the header data for MultiBoot - where I start
  BIOSLoader:
	call NULLSegments
	
	msg    db	"Stage one of boot initiating...", 0
	mov si, msg
	call Print
	
	jmp 0x0000:flushCS		; Different BIOS' may load me at different locations - a far jump will fix this, and I'll set CS to 0x0000
	
	; GDT calls here, instead of in Part 2 - coming soon...
	
	call BIOSGetMemSize
	call disableIRQ
	call CPUStop
	jmp Staging				; The final prep for Stage 2
	


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

; NULL the data segments within code segment 0x7c00_0
  NULLSegments:				; We are currently in code segment 0x7c00_0
	xor ax, ax
	mov ds, ax
	mov es, ax

; BIOS INT 0x12 spills the beans on the KB of available memory
  BIOSGetMemSize:			; Might not be very accurate...
	xor ax, ax
	int 0x12

; Function for reseting the @cs to 0x0000
  flushCS:
	xor ax, ax
	mov ss, ax				; Set up segment registers
	mov sp, BIOSLoad		; Set up a temporary stack that sits below BIOSLoad - will be overwritten in the second sector
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	cld

; Disable IRQs - still have yet to set those
  disableIRQ:
	mov al, 0xFF                      ; Out 0xFF to 0xA1 and 0x21 to disable all IRQs.
	out 0xA1, al
	out 0x21, al

; A primitive pausing mechanism for the System, until we're ready to proceed
  CPUStop:
	cli						; Clear Interrupts
	hlt						; Halt until I say otherwise

; Function for killing off the boot succession in an infinite loop
  Die:						; Infinite Loop mechanism will be explained later on in Stage Two
	hlt						; Halt
	jmp Die					; And Stay there...


; Modified the following block/chunk of code using advice from:
; http://forum.osdev.org/viewtopic.php?p=213987&sid=e56a9e082e0dd864ef799c578d97879b#p213987

; Function for reseting the Floppy
; Will most likely be removed/replaced with better variants in the near future... 
; .Reset:
;	mov		ah, 0					; reset floppy or CD function
;	mov		dl, 0					; drive 0 is floppy or CD drive
;	int		0x13					; call BIOS
;	jc		.Reset					; If Carry Flag (CF) is set, there was an error. Try resetting again

;	mov		ax, 0x1000				; we are going to read sector to into address 0x1000:0
;	mov		es, ax
;	xor		bx, bx
;
; jmp .Read

; Function for reading the first sector of FAT12/FAT16 - calls stage two
; Will most likely be removed/replaced with better variants in the near future - possibly for a different FS...
; .Read:
;	mov		ah, 0x42				; function 42
;	mov		al, 1					; read 1 sector
;	mov		ch, 1					; we are reading the second sector past us, so its still on track 1
;	mov		cl, 2					; sector to read (The second sector)
;	mov		dh, 0					; head number
;	mov		dl, 0					; drive number. Remember Drive 0 is floppy drive.
;	int		0x13					; call BIOS - Read the sector
;	jc		.Read					; Error, so try again

;	jmp		0x1000:0x0				; jump to execute the sector - Stage 2 of kernel bootstrap


; End first Sector - CPU Instruction Set and MultiBoot header

  Staging:
;	call .Reset
;	call .Read
;	still required until a better solution is found - need to load Stage 2 into memory before execution...
	times 510 - ($-$$) db 0		; Pad out the current file/sector
	dw 0xAA55					; End Stage One, Sector One
;	dw 0AA55h					; Alternative implementation for boot Sector One end-signature
	org 0x1000					; Initiate Stage, Two Sector Two
;	[ORG 01000h]				; Alternative implementation
	extern _kboot				;	_kboot is where this bootloader will leave off - we will call kboot in the linker script - ld