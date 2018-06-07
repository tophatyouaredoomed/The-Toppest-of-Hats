; BIOS Attempt - 00.00.01
; Inspired entirely by the tutorial found at
; http://pete.akeo.ie/2011/06/crafting-bios-from-scratch.html
; All credit goes to the author of the post, and their efforts are greatly appreciated
; Possibly very hardware-specific, but such is difficult to avoid at this stage


; GNU Assembler Settings
.intel_syntax noprefix		; Using Intel Assembler Syntax, as usual
.code16						; A redundancy, seeing that our bootloader sets this as well


; BIOS Parameters for Registers, Buffers, and UART setup
SIO_BASE		= 0X2E		; The VMware BIOS bootblock is a lie
PC97338_FER		= 0x00		; PC97338 Function Enable Register
PC97338_FAR		= 0x01		; PC97338 Function Address Register
PC97338_PTR		= 0x02		; PC97338 Power and Test Register

COM_BASE		= 0x3f8		; Our default COM1 base after SIO initialization
COM_RB			= 0x00		; Receive Buffer - Read_i
COM_TB			= 0x00		; Transmit Buffer - Write_o
COM_BRD_LO		= 0x00		; Baud Rate Divisor LSB
COM_BRD_HI		= 0x01		; Daud Rate Divisor MSB

COM_IER			= 0x01		; Interrupt Enable Register
COM_FCR			= 0x02		; 16650 FIFO Control Register - Write_o
COM_LCR			= 0x03		; Line Control Register
COM_MCR			= 0x04		; Modem Control Registrer
COM_LSR			= 0x05		; Line Status Register


 .section begin, "a"		; The 'ALLOC' flag, which is required in order to use objcopy
 