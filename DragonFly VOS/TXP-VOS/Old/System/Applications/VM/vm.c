#include "vm.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


VCPU*	vcpu_alloc(void)
{
	VCPU*	retval = (VCPU*)malloc(sizeof(VCPU));
	if(NULL != retval)
		memset(retval, 0, sizeof(VCPU));
	return retval;
}

void	vcpu_delete(VCPU** v)
{
	if(NULL != v && NULL != *v)
	{
		if(NULL != (*v)->stackBase)
			free((*v)->stackBase);
		if(NULL != (*v)->base)
			free((*v)->base);
		free(*v);
		*v = NULL;
	}
}

int		vcpu_load(VCPU* v, char* vm_file)
{
	FILE*			src;
	VM_HEADER		vmh;
	
	if(NULL == v || NULL == vm_file)
		return -1;
	
	if(NULL == (src = fopen(vm_file, "rb")))
		return -1;
	
	if(1 != fread(&vmh, sizeof(VM_HEADER), 1, src))
		return -1;
	
	//printf("%s: file size %d bytes\n", __FUNCTION__, vmh.fileSize);
	
	if(NULL == (v->base = (unsigned char*) malloc(sizeof(unsigned char) * (size_t)vmh.fileSize)))
	{
		fclose(src);
		return -1;
	}
	
	fseek(src, 0, SEEK_SET);
	fread(v->base, sizeof(unsigned char), (size_t)vmh.fileSize, src);
	
	if(0 < vmh.requestedStack)
	{
		v->stackBase = (unsigned int*)malloc(sizeof(unsigned int) * vmh.requestedStack);
		v->stackPtr = v->stackBase + vmh.requestedStack - 1;
	}
	else
	{
		fclose(src);
		return -1;
	}
	
	fclose(src);
	return 0;
}


unsigned long	vcpu_get_exported_entry(VCPU* v, char* name)
{
	VM_HEADER*			hdr;
	unsigned long		retval = 0;
	unsigned int*		exportPtr;
	char*				enamePtr;
	
	if(NULL == v || NULL == name)
		return retval;
	
	hdr = (VM_HEADER*)v->base;
	
	exportPtr = (unsigned int*)((unsigned long)v->base + (unsigned long)hdr->exportOffset);
	
	while(0 != *exportPtr)
	{
		enamePtr = (char*)((unsigned long)(*exportPtr + sizeof(unsigned int)) + (unsigned long)v->base);
		if(0 == strcmp(enamePtr, name))
		{
			retval = (unsigned long)*exportPtr;
			retval = (unsigned long)*((unsigned int*)(retval + (unsigned long)v->base));
			retval += (unsigned long)v->base;
			break;
		}
		exportPtr++;
	}
	
	return retval;
}


void	vcpu_set_ip(VCPU* v, unsigned long ip)
{
	if(NULL == v)
		return;
	
	v->ip = ip - (unsigned long)v->base;
}


int		vcpu_run(VCPU* v)
{
	int					counter = 0;
	int __volatile__ 	do_run = 1;
	INSTRUCTION*		instr;
	
	if(NULL == v || NULL == v->base)
		return 0;
	
	MPUSH(v, END_MARKER);	/*	set end marker	*/
	
	while(1 == do_run)
	{
		instr = (INSTRUCTION*)((unsigned long)v->base + (unsigned long)v->ip);
		switch(instr->opCode)
		{
			case MOV:	/*	MOV instruction	*/
				/*	reg, reg	*/
				if(instr->opType1 == OP_REG && instr->opType2 == OP_REG)
				{
					if(instr->opSize == _BYTE)
						v->regs[instr->reg1] = (v->regs[instr->reg1] & 0xFFFFFF00) | (v->regs[instr->reg2] & 0x000000FF);
					else if(instr->opSize == _WORD)
						v->regs[instr->reg1] = (v->regs[instr->reg1] & 0xFFFF0000) | (v->regs[instr->reg2] & 0x0000FFFF);
					else
						v->regs[instr->reg1] = v->regs[instr->reg2];
					v->ip += 2;
				}
				/*	reg, mem	*/
				else if(instr->opType1 == OP_REG && instr->opType2 == OP_MEM && instr->direction == DIR_LEFT)
				{
					unsigned int	tmp = (unsigned int)*((unsigned int*)((unsigned long)v->base + (unsigned long)v->ip + 2));
					tmp = (unsigned int) *((unsigned int*)((unsigned long)tmp + (unsigned long)v->base));
					if(instr->opSize == _BYTE)
						v->regs[instr->reg1] = (v->regs[instr->reg1] & 0xFFFFFF00) | (tmp & 0x000000FF);
					else if(instr->opSize == _WORD)
						v->regs[instr->reg1] = (v->regs[instr->reg1] & 0xFFFF0000) | (tmp & 0x0000FFFF);
					else
						v->regs[instr->reg1] = tmp;
					v->ip += 6;
				}
				/*	reg, imm	*/
				else if(instr->opType1 == OP_REG && instr->opType2 == OP_IMM)
				{
					unsigned int	tmp = (unsigned int)*((unsigned int*)((unsigned long)v->base + (unsigned long)v->ip + 2));
					if(instr->opSize == _BYTE)
					{
						v->regs[instr->reg1] = (v->regs[instr->reg1] & 0xFFFFFF00) | (tmp & 0x000000FF);
						v->ip += 3;
					}
					else if(instr->opSize == _WORD)
					{
						v->regs[instr->reg1] = (v->regs[instr->reg1] & 0xFFFF0000) | (tmp & 0x0000FFFF);
						v->ip += 4;
					}
					else
					{
						v->regs[instr->reg1] = tmp;
						v->ip += 6;
					}
				}
				/*	mem, reg	*/
				else if(instr->opType1 == OP_REG && instr->opType2 == OP_MEM && instr->direction == DIR_RIGHT)
				{
					unsigned int	tmp = (unsigned int)*((unsigned int*)((unsigned long)v->base + (unsigned long)v->ip + 2));
					if(instr->opSize == _BYTE)
					{
						*((unsigned char*)(tmp + (unsigned long)v->base)) = (unsigned char)(v->regs[instr->reg1] & 0x000000FF);
					}
					else if(instr->opSize == _WORD)
					{
						*((unsigned short*)(tmp + (unsigned long)v->base)) = (unsigned short)(v->regs[instr->reg1] & 0x0000FFFF);
					}
					else
					{
						*((unsigned int*)(tmp + (unsigned long)v->base)) = v->regs[instr->reg1];
					}
					v->ip += 6;
				}
				break;
			
			case MOVI:	/*	MOV Indirect. Address in register	*/
				/*	reg, [reg]	*/
				if(instr->opType1 == OP_REG && instr->opType2 == OP_REG && instr->direction == DIR_LEFT)
				{
					unsigned int	tmp = (unsigned int)*((unsigned int*)((unsigned long)v->regs[instr->reg2] + (unsigned long)v->base));
					
					if(instr->opSize == _BYTE)
						v->regs[instr->reg1] = (v->regs[instr->reg1] & 0xFFFFFF00) | (tmp & 0x000000FF);
					else if(instr->opSize == _WORD)
						v->regs[instr->reg1] = (v->regs[instr->reg1] & 0xFFFF0000) | (tmp & 0x0000FFFF);
					else
						v->regs[instr->reg1] = tmp;
				}
				/*	[reg], reg	*/
				else if(instr->opType1 == OP_REG && instr->opType2 == OP_REG && instr->direction == DIR_RIGHT)
				{
					unsigned int	tmp = ((unsigned int)((unsigned long)v->regs[instr->reg1] + (unsigned long)v->base));
					
					if(instr->opSize == _BYTE)
					{
						*((unsigned char*)(unsigned long)tmp) = (unsigned char)(v->regs[instr->reg2] & 0x000000FF);
					}
					else if(instr->opSize == _WORD)
					{
						*((unsigned short*)(unsigned long)tmp) = (unsigned short)(v->regs[instr->reg2] & 0x0000FFFF);
					}
					else
					{
						*((unsigned int*)(unsigned long)tmp) = v->regs[instr->reg2];
					}
				}
				v->ip += 2;
				break;
			
			case ADD:	/*	ADD instruction	*/
				/*	reg, reg	*/
				if(instr->opType1 == OP_REG && instr->opType2 == OP_REG)
				{
					if(instr->opSize == _BYTE)
						v->regs[instr->reg1] = (v->regs[instr->reg1] & 0xFFFFFF00) | (((v->regs[instr->reg2] & 0x000000FF) + (v->regs[instr->reg1] & 0x000000FF)) & 0x000000FF);
					else if(instr->opSize == _WORD)
						v->regs[instr->reg1] = (v->regs[instr->reg1] & 0xFFFF0000) | (((v->regs[instr->reg2] & 0x0000FFFF) + (v->regs[instr->reg1] & 0x0000FFFF)) & 0x0000FFFF);
					else
						v->regs[instr->reg1] += v->regs[instr->reg2];
					v->ip += 2;
				}
				/*	reg, mem	*/
				else if(instr->opType1 == OP_REG && instr->opType2 == OP_MEM && instr->direction == DIR_LEFT)
				{
					unsigned int	tmp = (unsigned int)*((unsigned int*)((unsigned long)v->base + (unsigned long)v->ip + 2));
					tmp = (unsigned int) *((unsigned int*)((unsigned long)tmp + (unsigned long)v->base));
					if(instr->opSize == _BYTE)
						v->regs[instr->reg1] = (v->regs[instr->reg1] & 0xFFFFFF00) | (((tmp & 0x000000FF) + (v->regs[instr->reg1] & 0x000000FF)) & 0x000000FF);
					else if(instr->opSize == _WORD)
						v->regs[instr->reg1] = (v->regs[instr->reg1] & 0xFFFF0000) | (((tmp & 0x0000FFFF) + (v->regs[instr->reg1] & 0x0000FFFF)) & 0x0000FFFF);
					else
						v->regs[instr->reg1] += tmp;
					v->ip += 6;
				}
				/*	reg, imm	*/
				else if(instr->opType1 == OP_REG && instr->opType2 == OP_IMM)
				{
					unsigned int	tmp = (unsigned int)*((unsigned int*)((unsigned long)v->base + (unsigned long)v->ip + 2));
					if(instr->opSize == _BYTE)
					{
						v->regs[instr->reg1] = (v->regs[instr->reg1] & 0xFFFFFF00) | (((tmp & 0x000000FF) + (v->regs[instr->reg1] & 0x000000FF)) & 0x000000FF);
						v->ip += 3;
					}
					else if(instr->opSize == _WORD)
					{
						v->regs[instr->reg1] = (v->regs[instr->reg1] & 0xFFFF0000) | (((tmp & 0x0000FFFF) + (v->regs[instr->reg1] & 0x0000FFFF)) & 0x0000FFFF);
						v->ip += 4;
					}
					else
					{
						v->regs[instr->reg1] += tmp;
						v->ip += 6;
					}
				}
				/*	mem, reg	* /
				else if(instr->opType1 == OP_REG && instr->opType2 == OP_MEM && instr->direction == DIR_RIGHT)
				{
					unsigned int	tmp = (unsigned int)*((unsigned int*)((unsigned long)v->base + (unsigned long)v->ip + 2));
					if(instr->opSize == _BYTE)
					{
						*((unsigned char*)(tmp + (unsigned long)v->base)) = (unsigned char)(v->regs[instr->reg1] & 0x000000FF);
					}
					else if(instr->opSize == _WORD)
					{
						*((unsigned short*)(tmp + (unsigned long)v->base)) = (unsigned short)(v->regs[instr->reg1] & 0x0000FFFF);
					}
					else
					{
						*((unsigned int*)(tmp + (unsigned long)v->base)) = v->regs[instr->reg1];
					}
					v->ip += 6;
				}*/
				break;
			/*
			case SUB:	/*	SUB instruction	* /
				break;*/
				
			case XOR:	/*	XOR instruction	*/
				/*	reg, reg	*/
				if(instr->opType1 == OP_REG && instr->opType2 == OP_REG)
				{
					if(instr->opSize == _BYTE)
						v->regs[instr->reg1] ^= (v->regs[instr->reg2] & 0x000000FF);
					else if(instr->opSize == _WORD)
						v->regs[instr->reg1] ^= (v->regs[instr->reg2] & 0x0000FFFF);
					else
						v->regs[instr->reg1] ^= v->regs[instr->reg2];
					v->ip += 2;
				}
				/*	reg, mem	*/
				else if(instr->opType1 == OP_REG && instr->opType2 == OP_MEM && instr->direction == DIR_LEFT)
				{
					unsigned int	tmp = (unsigned int)*((unsigned int*)((unsigned long)v->base + (unsigned long)v->ip + 2));
					tmp = (unsigned int) *((unsigned int*)((unsigned long)tmp + (unsigned long)v->base));
					if(instr->opSize == _BYTE)
						v->regs[instr->reg1] ^= (tmp & 0x000000FF);
					else if(instr->opSize == _WORD)
						v->regs[instr->reg1] ^= (tmp & 0x0000FFFF);
					else 
						v->regs[instr->reg1] ^= tmp;
					v->ip += 6;
				}
				/*	mem, reg	*/
				else if(instr->opType1 == OP_REG && instr->opType2 == OP_MEM && instr->direction == DIR_RIGHT)
				{
					unsigned int	tmp = (unsigned int)*((unsigned int*)((unsigned long)v->base + (unsigned long)v->ip + 2));
					
					if(instr->opSize == _BYTE)
					{
						*((unsigned char*)(tmp + (unsigned long)v->base)) ^= (unsigned char)(v->regs[instr->reg2] & 0x000000FF);
					}
					else if(instr->opSize == _WORD)
					{
						*((unsigned short*)(tmp + (unsigned long)v->base)) ^= (unsigned short)(v->regs[instr->reg2] & 0x0000FFFF);
					}
					else
					{
						*((unsigned int*)(tmp + (unsigned long)v->base)) ^= v->regs[instr->reg2];
					}
					v->ip += 6;
				}
				/*	reg, imm	*/
				else if(instr->opType1 == OP_REG && instr->opType2 == OP_IMM)
				{
					unsigned int	tmp = (unsigned int)*((unsigned int*)((unsigned long)v->base + (unsigned long)v->ip + 2));
					if(instr->opSize == _BYTE)
					{
						v->regs[instr->reg1] ^= (tmp & 0x000000FF);
						v->ip += 3;
					}
					else if(instr->opSize == _WORD)
					{
						v->regs[instr->reg1] ^= (tmp & 0x0000FFFF);
						v->ip += 4;
					}
					else 
					{
						v->regs[instr->reg1] ^= tmp;
						v->ip == 6;
					}
				}
				break;
				
			case LOOP:	/*	LOOP instruction	*/
				v->regs[REG_C]--;
				if(0 == v->regs[REG_C])
					v->ip += 6;
				else
				{
					unsigned int	tmp = (unsigned int)*((unsigned int*)((unsigned long)v->base + (unsigned long)v->ip + 2));
					v->ip = tmp;
				}
				break;
			
			case RET:	/*	RET instruction	*/
				if(END_MARKER == *v->stackPtr)
				{
					v->stackPtr++;
					do_run = 0;
				}
				else if(instr->opType1 == OP_IMM)
				{
					unsigned int	tmp = (unsigned int)*((unsigned int*)((unsigned long)v->base + (unsigned long)v->ip + 2));
					v->ip = *(v->stackPtr++);
					v->stackPtr += (int)tmp;
				}
				else
				{
					v->ip = *(v->stackPtr++);
				}
				break;
			
			default:	/*	Invalid instruction	*/
				printf("Invalid instruction at %08X\n", v->ip);
				do_run = 0;
				break;
		}
		counter++;
	}
	
	
	return counter;
}


