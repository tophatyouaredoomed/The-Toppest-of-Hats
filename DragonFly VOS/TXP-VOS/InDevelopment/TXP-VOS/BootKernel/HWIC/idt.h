/* IDT.h - partial Interrupt Descriptor Table module */
// Borrowed from:
// http://arjunsreedharan.org/post/99370248137/kernel-201-lets-write-a-kernel-with-keyboard
// This IDT and Keyboard setup routine is a re-organized version of the original source, and is meant to act as a kernel module

// Partially derived from:
// https://github.com/dweinstein/vmxos-osx/blob/master/src/kernel/interrupt/interrupt.h
// as well

#ifndef _HWIC_IDT_H
#define _HWIC_IDT_H

#include "keyboard_map.h"

#define KEYBOARD_DATA_PORT			0x60
#define KEYBOARD_STATUS_PORT		0x64
#define IDT_SIZE					256
#define INTERRUPT_GATE				0x8e		// ring 0 gate
#define INTERRUPT_gate				0xee		// ring 3 gate
#define KERNEL_CODE_SEGMENT_OFFSET	0x08

#define ENTER_KEY_CODE				0x1C


extern unsigned char keyboard_map[128];


void idt_init(void);
void kb_init(void);
void keyboard_handler_init(void);
void keyboard_handler(void);


extern void keyboard_handler(void);
extern char read_port(unsigned short port);
extern void write_port(unsigned short port, unsigned char data);
extern void load_idt(unsigned long *idt_ptr);

#endif