#include "int_utils.h"

// int_utils.c

// Source contents pulled directly from:
// https://github.com/dweinstein/vmxos-osx/
// files used include
// console.c, console.h,
// interrupt_hw.asm,
// interrupt.c, interrupt.h

char hex2string_buffer[10];
// converts number into hex format string
char* hex2string(int val)
{
	int i;
	int b, sh=7;
	hex2string_buffer[0] = '0';
	hex2string_buffer[1] = 'x';
	for (i=2; i<10; i++) 
	{
		b = val >> (sh-- * 4) & 0xF;
		hex2string_buffer[i] = (b < 10 ? 48 : 55) + b;
	}
	hex2string_buffer[10] = 0;
	return hex2string_buffer;
}

char bin2string_buffer[32];
// converts number into binary format string
char* bin2string(int val)
{
	int i;
	int b, sh=31;
	for (i=0; i<32; i++) 
	{
		b = val >> (sh--) & 0x1;
		bin2string_buffer[i] = 48 + b;
	}
	bin2string_buffer[32] = 0;
	return bin2string_buffer;
}

char dec2string_buffer[32];
// converts number into decimal format string
char* dec2string(int val)
{
	int i = 1, b;
	char s[32];
	if (val==0)
	{
		dec2string_buffer[0] = '0';
		dec2string_buffer[1] = 0;
		return dec2string_buffer;
	}
	s[0] = 0;
	while(val)
	{
		b = val % 10;
		s[i++] = 48 + b;
		val /= 10;
	}
	b = 0;
	// reverse the string and copy to buffer
	while((dec2string_buffer[b++] = s[--i]) && b < 32);
	return dec2string_buffer;
}