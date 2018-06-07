
#ifndef _VM_
#define _VM_

/*	Header	*/
typedef struct _VM_HEADER
{
	unsigned int			version;
	unsigned int			codeOffset;
	unsigned int			codeSize;
	unsigned int			dataOffset;
	unsigned int			dataSize;
	unsigned int			exportOffset;
	unsigned int			exportSize;
	unsigned int			requestedStack;
	unsigned int			fileSize;
}VM_HEADER;


/*	Our virtual CPU	*/
typedef struct _VCPU
{
	unsigned int		regs[4];					/*	Registers	*/
	unsigned int		*stackBase, *stackPtr;		/*	Stack pointer and stack base	*/
	unsigned int		ip;							/*	Instruction pointer	*/
	unsigned char		*base;						/*	Base address of the image (where the pseudo executable is loaded to). Actually, the pointer to the buffer	*/
}VCPU;

/*	Registers	*/
#define	REG_A			0
#define	REG_B			1
#define REG_C			2
#define REG_D 			3

/*	Instruction encoding	*/
typedef struct _INSTRUCTION
{
	unsigned short	opCode:5;
	unsigned short	opType1:2;
	unsigned short	opType2:2;
	unsigned short	opSize:2;
	unsigned short	reg1:2;
	unsigned short	reg2:2;
	unsigned short	direction:1;
}INSTRUCTION;

/*	Operand types	*/
#define OP_REG			0	/*	Register operand	*/
#define OP_IMM			1	/*	Immediate operand	*/
#define OP_MEM			2	/*	Memory reference	*/
#define OP_NONE			3	/*	No operand (optional)	*/

/*	Operand sizes	*/
#define OP_SIZE_BYTE	0
#define OP_SIZE_WORD	1
#define OP_SIZE_DWORD	2
#define _BYTE			OP_SIZE_BYTE
#define _WORD			OP_SIZE_WORD
#define _DWORD			OP_SIZE_DWORD

/*	Operation direction	*/
#define DIR_LEFT		0
#define DIR_RIGHT		1

/*	Instructions (OpCodes)	*/
#define MOV				1
#define MOVI			7
#define ADD				2
#define SUB				3
#define XOR				4
#define LOOP			5
#define RET				6

#define END_MARKER		0xDEADC0DE

/*	Macros	*/
#define MPUSH(cpu, val)	(*(--(cpu)->stackPtr) = (val))
#define MPOP(cpu, dest)	((dest) = *((cpu)->stackPtr++))

/*	Functions	*/
extern	VCPU*	vcpu_alloc(void);										/* Allocation of Virtual CPU */
extern	void	vcpu_delete(VCPU** v);									/* Deallocation of Virtual CPU and whatever it has pointers to */
extern	int		vcpu_load(VCPU* v, char* vm_file);						/* Loading pseudo executable and stack allocation */
extern	unsigned long	vcpu_get_exported_entry(VCPU* v, char* name);	/* Returns a pointer to the exported entry or NULL */
extern 	void	vcpu_set_ip(VCPU* v, unsigned long ip);					/* Sets instruction pointer (you pass actual pointer, not file offset!)*/
extern	int		vcpu_run(VCPU* v);										/* Runs VM starting from current IP */

#endif	/*	_VM_	*/
