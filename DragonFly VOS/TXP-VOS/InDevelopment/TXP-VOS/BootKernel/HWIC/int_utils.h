#ifndef _HWIC_INTMAP_H
#define _HWIC_INTMAP_H

// int_utils.h

// Source contents pulled directly from:
// https://github.com/dweinstein/vmxos-osx/
// files used include
// console.c, console.h,
// interrupt_hw.asm,
// interrupt.c, interrupt.h

// IRQ Macros
#define IRQ_default		0xFF
#define IRQ_timer		0x00
#define IRQ_keyboard	0x01
#define IRQ_cascade		0x02
#define IRQ_com2_4		0x03
#define IRQ_com1_3		0x04
#define IRQ_lpt			0x05
#define IRQ_floppy		0x06
#define IRQ_free7		0x07
#define IRQ_clock		0x08
#define IRQ_free9		0x09
#define IRQ_free10		0x0a
#define IRQ_free11		0x0b
#define IRQ_ps2mouse	0x0c
#define IRQ_coproc		0x0d
#define IRQ_ide1		0x0e
#define IRQ_ide2		0x0f


char* exceptions[] = 
{
	"0x00 #00 Divide By Zero Error",
	"0x01 #DB Debug Error",
	"0x02 #-- NMI Interrupt",
	"0x03 #BP Breakpoint",
	"0x04 #OF Overflow",
	"0x05 #BR BOUND Range Exceeded",
	"0x06 #UD Invalid Opcode",
	"0x07 #NM Device Not Available",
	"0x08 #DF Double Fault",
	"0x09 #-- Coprocessor Segment Overrun",
	"0x0a #TS Invalid TSS",
	"0x0b #NP Segment Not Present",
	"0x0c #SS Stack Segment Fault",
	"0x0d #GP Gneral Protection Fault",
	"0x0e #PF Page Fault",
	"0x0f #15 reserved",
	"0x10 #MF FPU Floating-Point Exception",
	"0x11 #AC Alignment Check",
	"0x12 #MC Machine Check",
	"0x13 #XF SIMD Floating-Point Exception",
	"0x14 #20 reserved",
	"0x15 #21 reserved",
	"0x16 #22 reserved",
	"0x17 #23 reserved",
	"0x18 #24 reserved",
	"0x19 #25 reserved",
	"0x1a #26 reserved",
	"0x1b #27 reserved",
	"0x1c #28 reserved",
	"0x1d #29 reserved",
	"0x1e #SX Security Exception",
	"0x1f #31 reserved",
};


// converts number into hex format string
char* hex2string(int val);

// converts number into binary format string
char* bin2string(int val);

// converts number into decimal format string
char* dec2string(int val);

extern void hang(void);

extern void int0(void);
extern void int1(void);
extern void int2(void);
extern void int3(void);
extern void int4(void);
extern void int5(void);
extern void int6(void);
extern void int7(void);
extern void int8(void);
extern void int9(void);
extern void int10(void);
extern void int11(void);
extern void int12(void);
extern void int13(void);
extern void int14(void);
extern void int15(void);
extern void int16(void);
extern void int17(void);
extern void int18(void);
extern void int19(void);
extern void int20(void);
extern void int21(void);
extern void int22(void);
extern void int23(void);
extern void int24(void);
extern void int25(void);
extern void int26(void);
extern void int27(void);
extern void int28(void);
extern void int29(void);
extern void int30(void);
extern void int31(void);

void int48(void);
void intHW(void);

#endif