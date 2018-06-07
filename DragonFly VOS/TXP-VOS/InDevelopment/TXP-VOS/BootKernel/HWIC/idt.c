/* IDT.c - partial Interrupt Descriptor Table module */
// Borrowed from:
// http://arjunsreedharan.org/post/99370248137/kernel-201-lets-write-a-kernel-with-keyboard
// This IDT and Keyboard setup routine is a re-organized version of the original source, and is meant to act as a kernel module

// Partially derived from:
// https://github.com/dweinstein/vmxos-osx/blob/master/src/kernel/interrupt/interrupt.h

// Could have been done this way:
// http://www.osdever.net/tutorials/view/interrupts-exceptions-and-idts-part-3-idts
// But then, would have had to manually editted the IDT entries for the Keyboard that I needed
// May try an alternative implementation later in time...


#include "idt.h"
#include "int_utils.h"


struct IDT_entry{
	unsigned short int offset_lowerbits;
	unsigned short int selector;
	unsigned char zero;
	unsigned char type_attr;
	unsigned short int offset_higherbits;
};
struct IDT_entry IDT[IDT_SIZE];


void idt_init(void)
{
	unsigned long idt_address;
	unsigned long idt_ptr[IDT_SIZE];
	
	unsigned long keyboard_address;
	
	/* populate IDT entry of keyboard's interrupt */
	keyboard_address = (unsigned long)keyboard_handler;
	
	IDT[0x21].offset_lowerbits = keyboard_address & 0xffff;
	IDT[0x21].selector = KERNEL_CODE_SEGMENT_OFFSET;
	IDT[0x21].zero = 0;					/* PRESENT BIT */
	IDT[0x21].type_attr = 0x8e;			/* INTERRUPT_GATE */
	IDT[0x21].offset_higherbits = (keyboard_address & 0xffff0000) >> 16;
	
	// Fill out the IDT
	// https://github.com/dweinstein/vmxos-osx/blob/master/src/kernel/interrupt/interrupt.c
	
	set_gate(0x00, int0);
	set_gate(0x01, int1);
	set_gate(0x02, int2);
	set_gate(0x03, int3);
	set_gate(0x04, int4);
	set_gate(0x05, int5);
	set_gate(0x06, int6);
	set_gate(0x07, int7);
	set_gate(0x08, int8);
	set_gate(0x09, int9);
	set_gate(0x0a, int10);
	set_gate(0x0b, int11);
	set_gate(0x0c, int12);
	set_gate(0x0d, int13);
	set_gate(0x0e, int14);
	set_gate(0x0f, int15);
	set_gate(0x10, int16);
	set_gate(0x11, int17);
	set_gate(0x12, int18);
	set_gate(0x13, int19);
	set_gate(0x14, int20);
	set_gate(0x15, int21);
	set_gate(0x16, int22);
	set_gate(0x17, int23);
	set_gate(0x18, int24);
	set_gate(0x19, int25);
	set_gate(0x1a, int26);
	set_gate(0x1b, int27);
	set_gate(0x1c, int28);
	set_gate(0x1d, int29);
	set_gate(0x1e, int30);
	set_gate(0x1f, int31);
	
	unsigned int i;

	for(i = 0x20; i < 0x31; i++) {
		if(i != 0x21) {
			set_IDT_gate(i, 1);
		}
		if(i == 0x30) {
			set_IDT_gate(i, 48);
		}
	}
	
	/* fill the IDT descriptor */
	idt_address = (unsigned long)IDT ;
	idt_ptr[0] = (sizeof (struct IDT_entry) * IDT_SIZE) + ((idt_address & 0xffff) << 16);
	idt_ptr[1] = idt_address >> 16 ;

	load_idt(idt_ptr);
}


void set_gate(int index, void (*handler)())
{
	unsigned int offset = (unsigned int)handler;
		IDT[index].offset_lowerbits = (offset & 0xFFFF);
		IDT[index].selector = 0x08;				// KERNEL_CODE_SEGMENT_OFFSET
		IDT[index].zero = INTERRUPT_GATE;		// PRESENT BIT
		IDT[index].type_attr = 0x8E00;			// INTERRUPT_GATE
		IDT[index].offset_higherbits = (offset >> 16);
}

void set_IDT_gate(int index, int type)
{
	
	void (*INThandle)(void) = &intHW;

	if(type == 1) {
		unsigned int offset = (unsigned int)INThandle;
	}
	else if(type == 48) {
		unsigned int offset = (unsigned int)intHW;
	}
	
	// Should I remove the (index) variable?
		IDT[index].offset_lowerbits = (offset & 0xFFFF);
		IDT[index].selector = 0x08;				// KERNEL_CODE_SEGMENT_OFFSET
		IDT[index].zero = INTERRUPT_GATE;		// PRESENT BIT
		IDT[index].type_attr = 0x8E00;			// INTERRUPT_GATE
		IDT[index].offset_higherbits = (offset >> 16);
}


char notification[50];
	
void int_syscall(unsigned int code)
{
	notification = "System call detected: ";
	newline();
	printraw(notification);
	notification = hex2string(code);
	printraw(notification);
	newline();
}

void int_exception(unsigned int code, unsigned int err)
{
	notification = "Exception detected amidst runtime: ";
	newline();
	printraw(notification);
	notification = dec2string(code);
	printraw(notification);
	newline();
	notification = "Exception code: ";
	printraw(notification);
	notification = exceptions[code];
	printraw(notification);
	newline();
	hang();
}

void int_error(unsigned int code, unsigned int err)
{
	notification = "Exception detected amidst runtime: ";
	newline();
	printraw(notification);
	notification = dec2string(code);
	printraw(notification);
	newline();
	notification = "Error code: ";
	printraw(notification);
	notification = bin2string(err);
	printraw(notification);
	newline();
	notification = "Page fault detected: ";
	printraw(notification);
	if(code == 14) {
		notification = "true";
	}
	else {
		notification = "false";
	}
	printraw(notification);
	hang();
}


// Initializes interrupt service for the Keyboard - no longer...
void kb_init(void)
{
	/* 0xFD is 11111101 - enables only IRQ1 (keyboard)*/
	write_port(0x21 , 0xFD);
}

void keyboard_handler_init(void)
{
	unsigned char status;
	char keycode;

	/* write EOI */
	send_EOI(0x01);
	// write_port(0x20, 0x20);

	status = read_port(KEYBOARD_STATUS_PORT);
	/* Lowest bit of status will be set if buffer is not empty */
	if (status & 0x01) {
		keycode = read_port(KEYBOARD_DATA_PORT);
		if(keycode < 0)
			return;

		if(keycode == ENTER_KEY_CODE) {
			kprint_newline();
			return;
		}

		vidptr[current_loc++] = keyboard_map[(unsigned char) keycode];
		vidptr[current_loc++] = 0x07;
	}
}


void keyboard_handler(void)
{
	unsigned char status, scancode;
	char keycode;

	/* write EOI */
	send_EOI(0x01);
	// write_port(0x20, 0x20);
	
	scancode = inb(0x60);
	
	status = read_port(KEYBOARD_STATUS_PORT);
	/* Lowest bit of status will be set if buffer is not empty */
	if (status & 0x01) {
		keycode = read_port(KEYBOARD_DATA_PORT);
		if(keycode < 0)
			return;
		vidptr[current_loc++] = keyboard_map[keycode];
		vidptr[current_loc++] = 0x07;	
	}
	
	if (scancode & 0x80) {
		// Shift, Alt, or Ctrl?
	}
}