
#include "vm.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
	VCPU*			cpu = vcpu_alloc();
	char*			string = "Hello, World!";
	char*			message;
	char*			key;
	unsigned long	decode;
	int 			i;
	
	vcpu_load(cpu, "vm_exe.bin");
	
	printf("Base    @ %lX\n", (unsigned long)cpu->base);
	printf("Stack 	@ %lX\n", (unsigned long)cpu->stackBase);
	printf("SP      @ %lX\n", (unsigned long)cpu->stackPtr);
	message = (char*)vcpu_get_exported_entry(cpu, "message");
	decode = vcpu_get_exported_entry(cpu, "decode");
	key = (char*)vcpu_get_exported_entry(cpu, "key");
	
	memcpy(message, string, strlen(string));
	
	*key = 0x1E;
	vcpu_set_ip(cpu, decode);
	vcpu_run(cpu);
	
	for(i = 0; i < strlen(string); i ++)
	{
		printf("%c", *(string + i));
	}
	printf("\n");
	
	for(i = 0; i < strlen(string); i ++)
	{
		printf("%c", *(message + i));
	}
	printf("\n");
	
	
	vcpu_set_ip(cpu, decode);
	vcpu_run(cpu);
	
	for(i = 0; i < strlen(string); i ++)
	{
		printf("%c", *(message + i));
	}
	printf("\n");
	
	
	return 0;
}
