
include 'defs.asm' ; OpCodes implementations

org 0
;---------------- 	Header	----------------------------------------------------
	h_magic		dd	0x00000101
	h_code		dd	_code
	h_code_size	dd	_code_size
	h_data		dd	_data
	h_data_size	dd	_data_size
	h_exp		dd	_export
	h_exp_size	dd	_export_size
	h_stack		dd	0x00000040
	h_size		dd	size

;==================	CODE	====================================================

_code:
	_decode:
		mov_ri		REG_B,	message,		_DWORD ;Move address of the message into B register
		mov_rm		REG_C,	message_len,	_BYTE  ;Move byte at message_len into C register
	__loop:
		mov_rmi		REG_A,	REG_B,			_BYTE  ;Move byte from the memory specified by address in B register into A register
		xor_rm		REG_A,	key,			_BYTE  ;Xor byte in A register with content of key
		mov_mri		REG_B,	REG_A,			_BYTE  ;Move byte from A register to memory pointed by B register
		add_ri		REG_B,	1,				_BYTE  ;Increment the address in B register
		loop_m		__loop						   ;Keep going till REG_C != 0
		return									   ;End of the function

_code_size = $ - _code

;==================	DATA	====================================================

_data:
	message		rb	16		;Place holder for hello world
	message_len	db	$ - message
	key			db	?

_data_size = $ - _data

;================== EXPORT	====================================================

_export:
	dd		f1					;File offset of the "decode" export entry
	dd		m					;File offset of the "message" export entry
	dd		k					;File offset of the "key" export entry
	dd		0					;End of the list of offsets
	f1		dd	_decode			;"decode" export entry: file offset of the exported object
			db	'decode',0		;						exported name
	m		dd	message			;Same for "message"
			db	'message',0
	k		dd	key				;Same for "key"
			db	'key',0

_export_size = $ - _export
size = $ - h_magic
