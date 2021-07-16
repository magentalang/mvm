#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif
#ifndef EXIT_FAILURE
#define EXIT_FAILURE 1
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stack.h"
#include "util.h"

void usage() {
	printf("usage: mvm [-h] file\n");
	printf("\t-h --help\tprints usage information\n");
	exit(EXIT_SUCCESS);
}

enum mode {
	normal,
	push,
};

int main(int argc, char *argv[]) {
	// print usage if...
	// - not enough arguments are provided
	if (argc <= 1)
		usage();
	// - if argument provided is asking for usage
	if (argv[1] == "-h" || argv[1] == "--help")
		usage();

	// other wise continue running...

	FILE *f = fopen(argv[1], "rb");

	if (f == NULL) {
		err("fatal: error opening file");
		exit(EXIT_FAILURE);
	}

	// get file size
	fseek(f, 0, SEEK_END);
	unsigned long size = ftell(f);
	fseek(f, 0, SEEK_SET);
	// read program
	unsigned long counter = 4; // set to start after the file header
	unsigned char bytecode[maxul(size, 4)]; // bytecode, at least 4 bytes long
	size_t ret_code = fread(bytecode, sizeof(char), size, f);
	fclose(f); // close file after reading
	if (ret_code == size) {
		// good :)
	} else {
		if (feof(f)) {
			err("fatal: unexpected end of file\n");
			exit(EXIT_FAILURE);
		} else if (ferror(f)) {
			err("fatal: error reading file\n");
			exit(EXIT_FAILURE);
		}
	}

	// call stack
	stack *cstack = initstack(1 << 16);
	// system stack
	stack *sstack = initstack(1 << 16);
	// create memory
	unsigned char mmemory[1 << 8]; // TODO: dynamic memory array
	unsigned int mpointer = 0x01; // memory pointer
	// create registers
	unsigned long registerA = 0x01;
	unsigned long registerB = 0x01;
	unsigned long registerC = 0x01;
	unsigned long registerD = 0x01;
	unsigned long registerE = 0x01;
	unsigned long registerF = 0x01;
	unsigned long registerG = 0x01;
	unsigned long registerH = 0x01;
	int width = 1; // byte width

	// verify file type
	if (bytecode[0] != '\n' || bytecode[1] != 'M' || bytecode[2] != 'V' || bytecode[3] != 'M') {
		err("fatal: file is not magenta bytecode\n");
		exit(EXIT_FAILURE);
	}

	// execute program
	enum mode mode = normal;
	int currentindex = 0;
	while (counter < size) {
		unsigned char c = bytecode[counter];
		currentindex++;

		switch (mode) {
			case normal: switch (c) {
				// noops
				case 0x05:
				case 0x0A: case 0x0B:
				case 0x11: case 0x13:
				case 0x1F: break;

				// enter push mode
				case 0x00: {
					mode = push;
				} break;

				// push bytes
				case 0x01: {
					for (int i = 0; i < width; i++)
						stack_push(sstack, bytecode[counter + i + 1]);

					counter += width;
				} break;

				// destructive pop
				case 0x02: stack_pop(sstack); break;

				// convert
				case 0x03: {
					unsigned long byte;
					stack_pop_width(sstack, width, &byte);
					char str[20];

					sprintf(str, "%lu", byte);
					strrev(str);

					for (int i = 0; str[i] != 0x00; i++)
						stack_push(sstack, str[i]);
				} break;

				// math
				case 0x06: case 0x07: {
					// width is at least one
					unsigned long a = nth_byte(0, registerA);
					unsigned long b = nth_byte(0, registerB);

					if (width == 1) {
						stack_push(sstack, c == 0x08 ? (unsigned char)a + (unsigned char)b : (unsigned char)a - (unsigned char)b);
						break;
					}

					// width is at least two
					a = a | nth_byte(1, registerA) << 8;
					b = b | nth_byte(1, registerB) << 8;

					if (width == 2) {
						unsigned short sum = c == 0x08 ? (unsigned short)a + (unsigned short)b : (unsigned short)a - (unsigned short)b;
						stack_push(sstack, nth_byte(1, sum));
						stack_push(sstack, nth_byte(0, sum));
						break;
					}

					// width is at least four
					a = a | nth_byte(2, registerA) << 16 | nth_byte(3, registerA) << 24;
					b = b | nth_byte(2, registerB) << 16 | nth_byte(3, registerB) << 24;

					if (width == 4) {
						unsigned int sum = c == 0x08 ? (unsigned int)a + (unsigned int)b : (unsigned int)a - (unsigned int)b;
						stack_push(sstack, nth_byte(3, sum));
						stack_push(sstack, nth_byte(2, sum));
						stack_push(sstack, nth_byte(1, sum));
						stack_push(sstack, nth_byte(0, sum));
						break;
					}

					// width is at least eight
					a = a | nth_byte(4, registerA) << 32 | nth_byte(5, registerA) << 40 | nth_byte(6, registerA) << 48 | nth_byte(7, registerA) << 56;
					b = b | nth_byte(4, registerB) << 32 | nth_byte(5, registerB) << 40 | nth_byte(6, registerB) << 48 | nth_byte(7, registerB) << 56;

					if (width == 8) {
						unsigned long sum = c == 0x08 ? (unsigned long)a + (unsigned long)b : (unsigned long)a - (unsigned long)b;
						stack_push(sstack, nth_byte(7, sum));
						stack_push(sstack, nth_byte(6, sum));
						stack_push(sstack, nth_byte(5, sum));
						stack_push(sstack, nth_byte(4, sum));
						stack_push(sstack, nth_byte(3, sum));
						stack_push(sstack, nth_byte(2, sum));
						stack_push(sstack, nth_byte(1, sum));
						stack_push(sstack, nth_byte(0, sum));
						break;
					}
				} break;

				// conditionals
				case 0x08: case 0x09: {
					int condition = 0;
					unsigned long newcounter;
					stack_pop_width(sstack, width, &newcounter);

					if (c == 0x08) {
						condition = registerA > registerB;
					} else if (c == 0x09) {
						condition = registerA == registerB;
					}

					if (condition == 0) {
						counter = newcounter - 1;
					}
				} break;

				// jump (to sub)
				case 0x0C: case 0x0D: {
					if (c == 0xD)
						stack_push(cstack, counter);

					unsigned long newcounter;
					stack_pop_width(sstack, width, &newcounter);
					counter = newcounter - 1;
				} break;

				// exit
				case 0x0E: counter = size; break;

				// exit sub
				case 0x0F: {
					unsigned long newcounter;
					stack_pop_width(cstack, width, &newcounter);
					counter = newcounter;
				} break;

				// memory set
				case 0x10: {
					unsigned long newpointer;
					stack_pop_width(sstack, width, &newpointer);
					mpointer = newpointer;
				} break;

				// memory location
				case 0x12: {
					if (width == 1) {
						stack_push(sstack, nth_byte(0, mpointer));
					} else if (width == 2) {
						stack_push(sstack, nth_byte(1, mpointer));
						stack_push(sstack, nth_byte(0, mpointer));
					} else if (width == 4) {
						stack_push(sstack, nth_byte(3, mpointer));
						stack_push(sstack, nth_byte(2, mpointer));
						stack_push(sstack, nth_byte(1, mpointer));
						stack_push(sstack, nth_byte(0, mpointer));
					} else if (width == 8) {
						stack_push(sstack, nth_byte(7, mpointer));
						stack_push(sstack, nth_byte(6, mpointer));
						stack_push(sstack, nth_byte(5, mpointer));
						stack_push(sstack, nth_byte(4, mpointer));
						stack_push(sstack, nth_byte(3, mpointer));
						stack_push(sstack, nth_byte(2, mpointer));
						stack_push(sstack, nth_byte(1, mpointer));
						stack_push(sstack, nth_byte(0, mpointer));
					}
				} break;

				// memory read block
				case 0x14: {
					if (width == 1) {
						stack_push(sstack, mmemory[mpointer]);
					} else if (width == 2) {
						stack_push(sstack, mmemory[mpointer + 1]);
						stack_push(sstack, mmemory[mpointer]);
					} else if (width == 4) {
						stack_push(sstack, mmemory[mpointer + 3]);
						stack_push(sstack, mmemory[mpointer + 2]);
						stack_push(sstack, mmemory[mpointer + 1]);
						stack_push(sstack, mmemory[mpointer]);
					} else if (width == 8) {
						stack_push(sstack, mmemory[mpointer + 7]);
						stack_push(sstack, mmemory[mpointer + 6]);
						stack_push(sstack, mmemory[mpointer + 5]);
						stack_push(sstack, mmemory[mpointer + 4]);
						stack_push(sstack, mmemory[mpointer + 3]);
						stack_push(sstack, mmemory[mpointer + 2]);
						stack_push(sstack, mmemory[mpointer + 1]);
						stack_push(sstack, mmemory[mpointer]);
					}
				} break;

				// memory read string
				case 0x15: {
					int strend = 0; // string end
					int strsrt = mpointer; // string start
					char* string;

					for (unsigned long i = mpointer; mmemory[i] != 0x00; i++)
						strend = i;

					for (unsigned long i = strend; i >= strsrt; i--)
						stack_push(sstack, mmemory[i]);
				} break;

				// memory write block
				case 0x16: {
					if (width == 1) {
						mmemory[mpointer] = stack_pop(sstack);
					} else if (width == 2) {
						mmemory[mpointer + 1] = stack_pop(sstack);
						mmemory[mpointer] = stack_pop(sstack);
					} else if (width == 4) {
						mmemory[mpointer + 3] = stack_pop(sstack);
						mmemory[mpointer + 2] = stack_pop(sstack);
						mmemory[mpointer + 1] = stack_pop(sstack);
						mmemory[mpointer] = stack_pop(sstack);
					} else if (width == 8) {
						mmemory[mpointer + 7] = stack_pop(sstack);
						mmemory[mpointer + 6] = stack_pop(sstack);
						mmemory[mpointer + 5] = stack_pop(sstack);
						mmemory[mpointer + 4] = stack_pop(sstack);
						mmemory[mpointer + 3] = stack_pop(sstack);
						mmemory[mpointer + 2] = stack_pop(sstack);
						mmemory[mpointer + 1] = stack_pop(sstack);
						mmemory[mpointer] = stack_pop(sstack);
					}
				} break;

				// byte width flags
				case 0x1A: width = 1; break;
				case 0x1B: width = 2; break;
				case 0x1C: width = 4; break;
				case 0x1D: width = 8; break;

				// debugging log command
				case 0x1E: {
					for (int i = 0; stack_peek(sstack) != 0x00; i++)
						putchar(stack_pop(sstack));

					stack_pop(sstack);
				} break;

				// pop to register
				case 0x20: case 0x22: case 0x24: case 0x26:
				case 0x28: case 0x2A: case 0x2C: case 0x2E: {
					unsigned long bytes = 0;
					stack_pop_width(sstack, width, &bytes);

					if (c == 0x20) registerA = bytes;
					else if (c == 0x22) registerB = bytes;
					else if (c == 0x24) registerC = bytes;
					else if (c == 0x26) registerD = bytes;
					else if (c == 0x28) registerE = bytes;
					else if (c == 0x2A) registerF = bytes;
					else if (c == 0x2C) registerG = bytes;
					else if (c == 0x2E) registerH = bytes;
				} break;

				// depop from register
				case 0x21: case 0x23: case 0x25: case 0x27:
				case 0x29: case 0x2B: case 0x2D: case 0x2F: {
					unsigned long reg;

					if (c == 0x21) reg = registerA;
					else if (c == 0x23) reg = registerB;
					else if (c == 0x25) reg = registerC;
					else if (c == 0x27) reg = registerD;
					else if (c == 0x29) reg = registerE;
					else if (c == 0x2B) reg = registerF;
					else if (c == 0x2D) reg = registerG;
					else if (c == 0x2F) reg = registerH;

					if (width == 1) {
						stack_push(sstack, nth_byte(0, reg));
					} else if (width == 2) {
						stack_push(sstack, nth_byte(1, reg));
						stack_push(sstack, nth_byte(0, reg));
					} else if (width == 4) {
						stack_push(sstack, nth_byte(3, reg));
						stack_push(sstack, nth_byte(2, reg));
						stack_push(sstack, nth_byte(1, reg));
						stack_push(sstack, nth_byte(0, reg));
					} else if (width == 8) {
						stack_push(sstack, nth_byte(7, reg));
						stack_push(sstack, nth_byte(6, reg));
						stack_push(sstack, nth_byte(5, reg));
						stack_push(sstack, nth_byte(4, reg));
						stack_push(sstack, nth_byte(3, reg));
						stack_push(sstack, nth_byte(2, reg));
						stack_push(sstack, nth_byte(1, reg));
						stack_push(sstack, nth_byte(0, reg));
					}
				} break;

				default: {
					err("fatal: invalid opcode");
					printf("\n          [%ld] 0x%x <--\n", counter + 1, c);
					exit(EXIT_FAILURE);
				}
			} break;

			case push:
				if (c == 0x00) {
					mode = normal;
				} else if (c >= 0xFF) {
					stack_push(sstack, 0x00);
				} else {
					stack_push(sstack, c);
				}
				break;
		}

		counter++;
	}

	// clean up
	free(sstack);
	free(cstack);
	//free(mmemory);

	return 0;
}
