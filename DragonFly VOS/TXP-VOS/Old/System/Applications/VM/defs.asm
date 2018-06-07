
; Registers

	REG_A			=0			;00
	REG_B			=1			;01
	REG_C			=2			;10
	REG_D			=3			;11
	
; Register shifts
	REG1_SHIFT		=11
	REG2_SHIFT		=13

; Operand types
	OP_TYPE_SHIFT1	=5
	OP_TYPE_SHIFT2	=7
	
	OP_REG1			=0
	OP_REG2			=0
	
	OP_IMM1			=1 shl OP_TYPE_SHIFT1
	OP_IMM2			=1 shl OP_TYPE_SHIFT2
	
	OP_MEM1			=2 shl OP_TYPE_SHIFT1
	OP_MEM2			=2 shl OP_TYPE_SHIFT2
	
	OP_NONE1		=3 shl OP_TYPE_SHIFT1
	OP_NONE2		=3 shl OP_TYPE_SHIFT2

; Operand size
	_BYTE	=0
	_WORD	=1 shl 9
	_DWORD	=2 shl 9

; Direction
	DIR_LEFT		=0 
	DIR_RIGHT		=1 shl 15


; Instructions
; -- MOV -----------------------------------------------------------------------
	_MOV			=1
	
	; mov from register to register
	macro	mov_rr	dst_reg, src_reg, size
	{
		dw	_MOV or OP_REG1 or OP_REG2 or size or (dst_reg shl REG1_SHIFT) or (src_reg shl REG2_SHIFT)
	}
	
	; mov from memory to register
	macro	mov_rm	reg, pointer, size
	{
		dw	_MOV or OP_REG1 or OP_MEM2 or size or (reg shl REG1_SHIFT)
		dd	pointer
	}
	
	; mov immediate to register
	macro	mov_ri reg, imm, size
	{
		dw	_MOV or OP_REG1 or OP_IMM2 or size or (reg shl REG1_SHIFT) 
		if size = _BYTE
			db	imm
		else if size = _WORD
			dw	imm
		else
			dd	imm
		end if
	}
	
	; mov from register to memory
	macro	mov_mr	pointer, reg, size
	{
		dw	_MOV or OP_REG1 or OP_MEM2 or size or (reg shl REG1_SHIFT) or DIR_RIGHT
		dd	pointer
	}
	
	
	;-----------------------------------------------------
	; mov from memory to register by a pointer in register
	; i stands for indirect
	_MOVI			=7
	macro	mov_rmi reg, mem_reg, size
	{
		dw	_MOVI or OP_REG1 or OP_REG2 or size or (reg shl REG1_SHIFT) or (mem_reg shl REG2_SHIFT)
	}
	
	; mov from register to memory pointed by another register
	macro	mov_mri mem_reg, reg, size
	{
		dw	_MOVI or OP_REG1 or OP_REG2 or size or (mem_reg shl REG1_SHIFT) or (reg shl REG2_SHIFT) or DIR_RIGHT
	}
	
; -- ADD -----------------------------------------------------------------------
	_ADD			=2
	
	; add register to register
	macro	add_rr reg1, reg2, size
	{
		dw	_ADD or OP_REG1 or OP_REG2 or size or (reg1 shl REG1_SHIFT) or (reg2 shl REG2_SHIFT)
	}
	
	; add memory to register
	macro	add_rm reg, mem, size
	{
		dw	_ADD or OP_REG1 or OP_MEM2 or size or (reg shl REG1_SHIFT) 
		dd	mem
	}
	
	; add immediate to register
	macro	add_ri reg, imm, size
	{
		dw	_ADD or OP_REG1 or OP_IMM2 or size or (reg shl REG1_SHIFT)
		if size = _BYTE
			db	imm
		else if size = _WORD
			dw	imm
		else
			dd	imm
		end if
	}
	
	; add register to memory
	macro 	add_mr mem, reg, size
	{
		dw	_ADD or OP_REG1 or OP_MEM2 or size or (reg shl REG1_SHIFT) or DIR_RIGHT
		dd	mem
	}
	
	
; -- SUB -----------------------------------------------------------------------
	_SUB			=3
	
	; sub register to register
	macro	sub_rr reg1, reg2, size
	{
		dw	_SUB or OP_REG1 or OP_REG2 or size or (reg1 shl REG1_SHIFT) or (reg2 shl REG2_SHIFT)
	}
	
	; sub memory to register
	macro	sub_rm reg, mem, size
	{
		dw	_SUB or OP_REG1 or OP_MEM2 or size or (reg shl REG1_SHIFT) 
		dd	mem
	}
	
	; sub immediate to register
	macro	sub_ri reg, imm, size
	{
		dw	_SUB or OP_REG1 or OP_IMM2 or size or (reg shl REG1_SHIFT)
		if size = _BYTE
			db	imm
		else if size = _WORD
			dw	imm
		else
			dd	imm
		end if
	}
	
	; sub register to memory
	macro 	sub_mr mem, reg, size
	{
		dw	_SUB or OP_REG1 or OP_MEM2 or size or (reg shl REG1_SHIFT) or DIR_RIGHT
		dd	mem
	}
	
	
; -- XOR -----------------------------------------------------------------------
	_XOR			=4
	
	; xor register to register
	macro	xor_rr reg1, reg2, size
	{
		dw	_XOR or OP_REG1 or OP_REG2 or size or (reg1 shl REG1_SHIFT) or (reg2 shl REG2_SHIFT)
	}
	
	; xor memory to register
	macro	xor_rm reg, mem, size
	{
		dw	_XOR or OP_REG1 or OP_MEM2 or size or (reg shl REG1_SHIFT) 
		dd	mem
	}
	
	; xor immediate to register
	macro	xor_ri reg, imm, size
	{
		dw	_XOR or OP_REG1 or OP_IMM2 or size or (reg shl REG1_SHIFT)
		if size = _BYTE
			db	imm
		else if size = _WORD
			dw	imm
		else
			dd	imm
		end if
	}
	
	; xor register to memory
	macro 	xor_mr mem, reg, size
	{
		dw	_XOR or OP_REG1 or OP_MEM2 or size or (reg shl REG1_SHIFT) or DIR_RIGHT
		dd	mem
	}
	
	
; -- LOOP ----------------------------------------------------------------------
	_LOOP			=5
	
	; loop uses C register as counter
	; we have to give it a name different from "loop" in order to not confuse 
	; the assembler
	macro 	loop_m addr
	{
		dw	_LOOP
		dd	addr
	}
	
; -- RET -----------------------------------------------------------------------
	_RET			=6
	; return from function call
	macro	return [num]
	{
		if num eq
			dw	_RET or OP_NONE1 or OP_NONE2
		else
			dw	_RET or OP_IMM1
			dd	num
		end if
	}
	
	
	
	
	
	
	
	
	
	
	
	
	
