; Slimmed-down, optimized source from LongModeDirectly.asm
; Originally used verbatim, from:
; http://wiki.osdev.org/Entering_Long_Mode_Directly
; Optimized for use in the TXP-VOS Kernel, which already handles some steps outside the boundaries of the original tutorial.
; 50 lines shorter than the original source file - talk about big changes :)

%define PAGE_PRESENT	(1 << 0)
%define PAGE_WRITE		(1 << 1)

%define CODE_SEG		0x0008
%define DATA_SEG		0x0010

; Function to switch directly to long mode from real mode.
; Identity maps the first 2MiB.
; Uses Intel syntax.

; es:edi								Should point to a valid page-aligned 16KiB buffer, for the PML4, PDPT, PD and a PT.
; ss:esp								Should point to memory that can be used as a small (1 uint32_t) stack

  global _GoLong
  _GoLong:
	; Zero out the 16KiB buffer.
	; Since we are doing a rep stosd, count should be bytes/4.   
	push di						  		; REP STOSD alters DI.
	mov ecx, 0x1000
	xor eax, eax
	cld
	rep stosd
	pop di								; Get DI back.
	
	; Build the Page Map Level 4.
	; es:di points to the Page Map Level 4 table.
	lea eax, [es:di + 0x1000]			; Put the address of the Page Directory Pointer Table in to EAX.
	or eax, PAGE_PRESENT | PAGE_WRITE	; Or EAX with the flags - present flag, writable flag.
	mov [es:di], eax				 	; Store the value of EAX as the first PML4E.
	
	; Build the Page Directory Pointer Table.
	lea eax, [es:di + 0x2000]			; Put the address of the Page Directory in to EAX.
	or eax, PAGE_PRESENT | PAGE_WRITE	; Or EAX with the flags - present flag, writable flag.
	mov [es:di + 0x1000], eax			; Store the value of EAX as the first PDPTE.
	
	; Build the Page Directory.
	lea eax, [es:di + 0x3000]			; Put the address of the Page Table in to EAX.
	or eax, PAGE_PRESENT | PAGE_WRITE	; Or EAX with the flags - present flag, writeable flag.
	mov [es:di + 0x2000], eax			; Store to value of EAX as the first PDE.
	
	push di						  		; Save DI for the time being.
	lea di, [di + 0x3000]				; Point DI to the page table.
	mov eax, PAGE_PRESENT | PAGE_WRITE	; Move the flags into EAX - and point it to 0x0000.
	
	; Build the Page Table.
.LoopPageTable:
	mov [es:di], eax
	add eax, 0x1000
	add di, 8
	cmp eax, 0x200000					; If we did all 2MiB, end.
	jb .LoopPageTable
 
	pop di								; Restore DI.
	
	; Disable IRQs - Performed later, in Stage 2
	
	mov eax, 10100000b					; Set the PAE and PGE bit.
	mov cr4, eax
	
	mov edx, edi						; Point CR3 at the PML4.
	mov cr3, edx
	
	mov ecx, 0xC0000080					; Read from the EFER MSR. 
	rdmsr
	
	or eax, 0x00000100					; Set the LME bit.
	wrmsr
	
	mov ebx, cr0						; Activate long mode -
	or ebx,0x80000001					; - by enabling paging and protection simultaneously.
	mov cr0, ebx
	
	ret									; We'll come back when the GDT is loaded...


  [BITS 64]
; Function for switching directly into Long mode - 64
  global _LongMode
  _LongMode:
	mov ax, DATA_SEG
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax

	; jmp _LongKernel.GDT64				; You should replace this jump to wherever you want to jump to - Done :)
	; Already taken care of...
	ret