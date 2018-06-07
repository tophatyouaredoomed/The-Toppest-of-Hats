/* pic.h - PIC Controller and setter resource header */
// Not developed on my own. Re-distro'd from other sources, mentioned in pic.c
// Please see the original sources for more information...
// 
// Note - if you want to use the local APIC and IOAPIC instead, please make sure to disable the PIC first
// mov a1, 0xff
// out 0xa1, al
// out 0x21, al
// Then, disable this PIC subsystem and remove it from your source

#ifndef _HWIC_PIC_H
#define _HWIC_PIC_H

#define PIC1		0x20
#define PIC1_COMM	PIC1
#define PIC1_DAT	0x21
// or could it be (PIC1+1) ?

#define PIC2		0xA0
#define PIC2_COMM	PIC2
#define PIC2_DAT	0xA1
// or could it be (PIC2+1) ?

#define PIC_EOI		0x20

#define ICW1 0x11
#define ICW4 0x01


void pic_init(int pic1, int pic2);
void send_EOI(unsigned char irq);

void maskIRQ(unsigned int irq);
void unmaskIRQ(unsigned int irq);

#endif