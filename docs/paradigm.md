﻿# PARADIGM

- list of shell commands in the shell.md
- shell can interpret scripts (see shell.md for details)
- MocConOS can execute programs written and compiled for it
- source code of the program is written in assembly-like language and translated (compiled by assembler, macro processor) into an object file containing interpreted bytecode
- bytecode execution (interpretation) environment is a register-based virtual machine (similar to Java, Dalvik, .Net)
- VM uses an array of variables (registers in their essence) and a call stack
- call stack size is 254
- variable and label names must start with a letter or "_" and consist of letters, numbers, and a character "_"
- all variables are global, total uint8_max = 256 variables (registers)
- all variables has type int16, declared as an array[uint8 > 0] with garbage initial values
- array indexing from 0
- access to "array" is equivalent "array[0]"
- one-line comment after ';'
- length of "word" is 8 bits
- there can be 2^7 = 128 operations (RISC), 58 operation are used
- first bit in operation is the argument type, the remaining 7 is the opcode of the operation
- types of the arguments: 
	0 - int16 / label / null (2 or 0 byte)
	1 - variable (1 byte) 
- int16 transmitted and stored in big-endian
- ASCII encoding is used for character output, big byte of the int16 number is discarded
- file system is fat16 (or fat32, with the same restrictions as fat16)
- file names are written in an array (with a mandatory null terminator) in notation 8.3, directories and files are delimited by the character '/'
- working directory is always the root directory of the SD card, it can be omitted when accessing the file
- as the newline character used '\n', all strings necessarily end with a null terminator
- subroutine starts with (prc) and ends with (ret)
- conditional (if*) and unconditional (jmp) label navigation differs from subroutine (prc) calls in that it does not add the current address to the call stack
- there is no validation subroutine label, i.e., "unconditional jump at subroutine", and "call label" will not cause any errors
- interpreter uses 16-bit addressing, the size of the resulting object file should not exceed 2^16=65536 bytes (may be larger than this size, but it will be impossible to navigate through the labels and call subroutines from/to beyond this size)
- commands in the source code are written as
	- OPERATION {ARG_1{\[X]} {ARG_2{[Y]}}} {;COMMENT}
	- OPERATION - operation name
	- ARG_1 - first argument
	- ARG_2 - second argument
	- X, Y - index, if ARG is array
	- COMMENT - comment
- free pins list
	- Digital: 0 1 14 15 16 17 18 19 42 43 44 45 46 47 48 49 53
	- PWM: 8 9 10 11 12 13
	- Analog in: 54 55 56 57 58 59 60 61 62 63 64 65
	- I2C: 20 21
	- Other: 66 67 68 69
	- Due only: 50 51 52