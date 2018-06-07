/* bootkernel.cpp - The Kernel, mark 3 */
/* This file requires the following changes to be of use:
 -linking to STLPort - STanDard C++ LIBrary
 -final call to VOS shell */
// details on STLPort linking found here:
// http://stackoverflow.com/questions/7470346/how-to-use-stlport-in-my-kernel


// Coming Soon:
// http://www.osdever.net/tutorials/view/interrupts-exceptions-and-idts-part-2-exceptions


// These are reserved for later use

#if !defined(__cplusplus)
#include <stdbool.h> 
// C doesn't have booleans by default
#if defined(__cplusplus)
extern "C" 
// Use C linkage for kmain
#endif

#include <string.h>
#include <stdio.h>
// A few platform - independent headers we can call into action 
#include <stddef.h>
// We can use it - it doesnt use any platform-related api functions
#include <stdint.h>
// Include it to get int16_t and some integer types

#include "idt.h"
#include "pic.h"
// For Interrupt and Error Handling Services

/* Check if the compiler thinks if we are targeting the wrong operating system. */
#if defined(__linux__)
#error "You are not using a cross-compiler, you will most certainly run into trouble"
#endif


// Some parameters for the vga video controllers implemented here

const char *firstWords = "Hello - Welcome to your New Operating System!";
char *vidptr = (char*)0xb8000; 	//video mem begins here.
unsigned int i = 0;
unsigned int j = 0;
static const int VGA_WIDTH = 80;
static const int VGA_HEIGHT = 25;
unsigned int current_loc = 0;


void kb_init(void)
{
	/* 0xFD is 11111101 - enables only IRQ1 (keyboard)*/
	write_port(0x21 , 0xFD);
}


void clr(void)
{	
	/* this loops clears the screen
	* there are 25 lines each of 80 columns; each element takes 2 bytes */
	while(j < VGA_WIDTH * VGA_HEIGHT * 2) {
		/* blank character */
		vidptr[j] = ' ';
		/* attribute-byte - light grey on black screen */
		vidptr[j+1] = 0x07; 		
		j = j + 2;
		j = 0;
}


void newline(void)
{
	unsigned int lineSize = 160;
	// There are 2 bytes for each element, and 80 columns in a line
	// Straight multiplication of these constants gives us the size of a line - 160
	current_loc = current_loc + (lineSize - current_loc % (lineSize));
}


void printraw(char str)
{
	/* this loop writes the string to video memory */
	while(str[j] != '\0') {
		/* the character's ascii */
		vidptr[i] = str[j];
		/* attribute-byte: give character black bg and light grey fg */
		vidptr[i+1] = 0x07;
		++j;
		i = i + 2;
	}
}


void kmain(void)
{
	clr();
	printraw(firstWords);
	newline();
	printraw("Calling local HardWare Interface Controllers...");
	newline();
	idt_init();
	pic_init(PIC1, 0x28);
	
	// kb_init();
	// Now handled by 
	
	keyboard_handler_init();
	
	// printraw("Calling User Managing Agent...");
	// Will not work until MemMan is completed - will have to call as a module, using GRUB/GRUB 2
	// system("/System/Tools/Sys_Tools/createAccount");
	return;
}