/* pic.c - PIC Controller and setter */
// Complete - Derived from:
// http://www.osdever.net/tutorials/view/programming-the-pic
// and
// http://wiki.osdev.org/8259_PIC
// 
// This code module is not my own - rights to original authors reserved
// Thank you, Alexander Blessing and OsDev Wiki contributors


#include "pic.h"
#include "int_utils.h"


// initializes the PICs and remaps them
void pic_init(int pic1, int pic2)
{
	unsigned char d1, d2;
	
	/* Save the previous states of PIC Master and Slave */
	d1 = inb(PIC1_DAT);
	d2 = inb(PIC2_DAT);
	
	/* If an IRQ came only from PIC Master, one needs only to send an EOI to the Master */
	/* However, if the PIC Slave is the source, both devices must be sent an EOI */
	/* For simplicity, all IRQs will result in an EOI being sent to both devices */
	/* Also resets the chip */
	outb(PIC1, PIC_EOI);
	io_wait();
	outb(PIC2, PIC_EOI);
	
	io_wait();
	
	// ICW1 - begin initialization
	outb(PIC1, ICW1);
	io_wait();
	outb(PIC2, ICW1);
	
	io_wait();
	
	// ICW2 - remap offset address of IDT
	// In x86 protected mode, we have to remap the PICs beyond 0x20 because
	// Intel has designated the first 32 interrupts as "reserved" for CPU exceptions
	outb(PIC1 + 1, PIC1);	/* remap */
	io_wait();
	outb(PIC2 + 1, 0x28);	/*  pics */
	
	io_wait();
	
	// ICW3 - setup cascading
	outb(PIC1 + 1, 0x00);
	// outb(PIC1 + 1, 4)	=>	IRQ2 -> connection to slave
	io_wait();
	outb(PIC2 + 1, 0x00);
	// outb(PIC2 + 1, 2);
	
	io_wait();
	
	// ICW4 - environment info
	outb(PIC1 + 1, ICW4);
	io_wait();
	outb(PIC2 + 1, ICW4);
	/* Initialization finished */
	
	io_wait();
	
	// Mask the Interrupts
	maskIRQ(0x01);
	
	/* Restore saved values or 'masks' */
	outb(PIC1_DAT, d1);
	outb(PIC2_DAT, d2);
	
	/* disable all IRQs */
	outb(PIC1 + 1, 0xFF);
	// This now itializes interrupt service for the Keyboard
	outb(PIC+1, 0xFD);
}


// sends EOI to the PICs, based upon standard Master-Slave IRQ source rules
// not used here, but available for those who want to take this down a different path
void send_EOI(unsigned char irq)
{
	if(irq >= 8){
		outb(PIC2_COMM, PIC_EOI);
	//	outb(PIC2, PIC_EOI);
	}
	outb(PIC1_COMM, PIC_EOI);
//	outb(PIC1, PIC_EOI);
}


void maskIRQ(unsigned int irq)
{
	if(irq == IRQ_default) {
		outb(PIC1_DAT, 0xFF);
		outb(PIC2_DAT, 0xFF);
	}
	else {
		irq = irq | (1<<irq);
		
		if(irq >= 8) {
			outb(PIC2_DAT, irq >> 8);
		}
		else {
			outb(PIC1_DAT, irq & 0xFF);
		}
	}
}

void unmaskIRQ(unsigned int irq)
{
	if(irq == IRQ_DEF) {
		outb(PIC1_DAT, 0x00);
		outb(PIC2_DAT, 0x00);
	}
	else {
		irq = irq & (1<<irq);
		
		if(irq >= 8) {
			outb(PIC2_DAT, irq >> 8);
		}
		else {
			outb(PIC1_DAT, irq & 0xFF);
		}
	}
}