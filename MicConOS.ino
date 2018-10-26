/*
 * MicConOS - virtual machine for Arduino Due and Mega
 * Copyright © 2018 Konstantin Kraynov
 * konstantin.kraynov@yandex.ru
 * License: GNU General Public License v3
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#define ps2key_int_pin 3
#define ps2key_data_pin 4
#define pin_BL 5//backlight
#define pin_SD 6
#define exe_ok 0
#define exe_end 1
#define stack_overflow 2
#define division_by_zero 3
#define end_of_file 4
#define file_not_exist 5
#define opening_error 6
#define rewrite_error 7
#define off_by_one 8
#include <Arduino.h>
#include <timer-api.h>
#include <SD.h>
#include <UTFT.h>
#include <PS2Keyboard.h>
#include <Wire.h>
#ifdef __arm__
extern "C" char* sbrk(int incr);
#else //!__arm__
extern char *__brkval;
#endif
char input_str[256];//current executable, 255 for name and path, and 1 for null terminator
char directory[256] = "/";//working directory
char path_buf[256];
char work_file_name[256];//file with which the program works
char comline[8];
int8_t exe_code;//code returned by exe()
int8_t delta = 4;//length of command, 1..4
int8_t data[4];//a place for pre-loading from a file operation code and the three following bytes
uint8_t BL = 51;//brightness of the backlight
int16_t registers[256];
uint8_t call = 0;//number of the next entry in the call stack
uint16_t call_stack[255];//because the "call" can not be more than 255
uint16_t cursorY;
uint16_t cursorX;
uint16_t cursorYbar;
File exe_file;
File work_file;
PS2Keyboard keyboard;
UTFT LCD(ILI9486, 38, 39, 40, 41);
extern uint8_t BigFont[];
const char hexnums[17] = "0123456789ABCDEF";
const unsigned int VGA_COLORS[16] = {VGA_BLACK, VGA_WHITE, VGA_RED, VGA_GREEN, VGA_BLUE, VGA_SILVER, VGA_GRAY, VGA_MAROON, VGA_YELLOW, VGA_OLIVE, VGA_LIME, VGA_AQUA, VGA_TEAL, VGA_NAVY, VGA_FUCHSIA, VGA_PURPLE};
uint16_t dbytes(const uint8_t &a, const uint8_t &b) { return ((uint16_t)a << 8) | b; }
uint8_t bcd2bin(uint8_t val) { return val - 6 * (val >> 4); }
uint8_t dec2bcd(uint8_t val) { return (val / 10 * 16) + (val%10); }
uint8_t getTemp() {
	Wire.beginTransmission(0x68);
	Wire.write(0x11);
	Wire.endTransmission();
	Wire.requestFrom(0x68, 2);
	return ((((short)Wire.read() << 8) | (short)Wire.read()) >> 6) / 4;
}
uint16_t now() {
	Wire.beginTransmission(0x68);
	Wire.write(0);
	Wire.endTransmission();
	Wire.requestFrom(0x68, 3);
	return bcd2bin(Wire.read() & 0x7F) + bcd2bin(Wire.read()) * 60 + bcd2bin(Wire.read()) * 60 * 17;
}
uint32_t free_memory() {
	volatile char top;
#ifdef __arm__
	return &top - reinterpret_cast<char*>(sbrk(0));
#else //!__arm__
	return __brkval ? &top - __brkval : &top - __malloc_heap_start;
#endif
}
bool cmp(char* c1, char* c2) {
	for (uint8_t i = 0; i < 255; ++i) {
		if (c1[i] != c2[i]) return false;
		if (c1[i] == 0) break;
	}
	return true;
}
void status() {
	char bar[32] = "", me[7], sTime[3];
	uint8_t mm, hh, d, m, y;
	Wire.beginTransmission(0x68);
	Wire.write(0x01);
	Wire.endTransmission();
	Wire.requestFrom(0x68, 6);
	mm += bcd2bin(Wire.read());
	hh += bcd2bin(Wire.read());
	Wire.read();
	d += bcd2bin(Wire.read());
	m += bcd2bin(Wire.read());
	y += bcd2bin(Wire.read());
	if (hh < 10) strcat(bar, "0");
	strcat(bar, itoa(hh, sTime, 10));
	strcat(bar, ":");
	if (mm < 10) strcat(bar, "0");
	strcat(bar, itoa(mm, sTime, 10));
	strcat(bar, " ");
	if (d < 10) strcat(bar, "0");
	strcat(bar, itoa(d, sTime, 10));
	strcat(bar, ".");
	if (m < 10) strcat(bar, "0");
	strcat(bar, itoa(m, sTime, 10));
	strcat(bar, ".");
	if (y < 10) strcat(bar, "0");
	strcat(bar, itoa(y, sTime, 10));
	strcat(bar, " T=");
	strcat(bar, itoa(getTemp(), sTime, 10));
	strcat(bar, " RAM=");
	strcat(bar, itoa(free_memory(), me, 10));
	LCD.print(bar, 0, cursorYbar);
}
void ClrScr() {
	LCD.fillScr(LCD.getBackColor());
	status();
	cursorX = 0;
	cursorY = 0;
}
void setc8(const char &k) {
	char c[2] = {(k == 0 || k == PS2_DELETE) ? ' ' : k, 0};
	if ((cursorX > LCD.getDisplayXSize() - LCD.getFontXsize()) || k == '\n') {
		cursorX = 0;
		cursorY += LCD.getFontYsize();
		if (cursorY > LCD.getDisplayYSize() - 2 * LCD.getFontYsize()) {
			while (keyboard.read() != PS2_ENTER) delay(16);
			ClrScr();
		}
	} else if (k == PS2_DELETE) {
		cursorX -= LCD.getFontXsize();
		LCD.print(c, cursorX, cursorY);
	} else {
		LCD.print(c, cursorX, cursorY);
		cursorX += LCD.getFontXsize();
	}
}
void echo(const char* s, const uint8_t &len = 255) {
	for (uint8_t i = 0; i < len; ++i)
		if (s[i] == 0)
			break;
		else
			setc8(s[i]);
}
inline void echoln(const char* s, const uint8_t &len = 255) {
	echo(s, len);
	setc8('\n');
}
inline void setc16(const int16_t &k) {
	char s[6];
	echo(itoa(k, s, 10));
}
uint8_t getstr(char* str, const uint8_t &len = 255) {
	setc8('>');
	uint8_t i = 0, buf = 0;
	while (buf != PS2_ENTER) {
		while (!keyboard.available()) delay(16);
		buf = keyboard.read();
		if (buf == PS2_DELETE && i > 0) {
			str[--i] = 0;
			setc8(buf);
		} else if (buf == PS2_ENTER) {
			str[i] = 0;
			setc8('\n');
		} else if (buf >= 32 && i < len && buf != PS2_DELETE) {
			str[i++] = buf;
			setc8(buf);
		}
	}
	return i;
}
inline void getc16(int16_t &u) {
	char s[6];
	getstr(s, 5);
	u = (int16_t)atoi(s);
}
uint8_t exe() {
	uint8_t opcode = data[0] & 127;
	int16_t swpbuf;
	int16_t arg2 = 0;
	if (data[0] & 128) {
		if (opcode >= 37 && opcode <= 43)
			arg2 = registers[data[1]];
		else
			arg2 = registers[data[2]];
		delta = 3;
	} else {
		if (opcode >= 37 && opcode <= 43)
			arg2 = dbytes(data[1], data[2]);
		else
			arg2 = dbytes(data[2], data[3]);
		delta = 4;
	}
	switch (opcode) {
		case 0:
			delta = 1;
			break;
		case 1:
			if (call == 0)
				return off_by_one;
			--call;
			exe_file.seek(call_stack[call]);
			break;
		case 2:
			work_file.close();
			delta = 1;
			break;
		case 3:
			return exe_end;
			break;
		case 4:
			ClrScr();
			delta = 1;
			break;
		case 5:
			swpbuf = registers[data[1]];
			registers[data[1]] = registers[data[2]];
			registers[data[2]] = swpbuf;
			delta = 3;
			break;
		case 6:
			registers[data[1]] += arg2;
			break;
		case 7:
			registers[data[1]] -= arg2;
			break;
		case 8:
			registers[data[1]] *= arg2;
			break;
		case 9:
			if (arg2 == 0) return division_by_zero;
			registers[data[1]] /= arg2;
			break;
		case 10:
			if (arg2 == 0) return division_by_zero;
			registers[data[1]] %= arg2;
			break;
		case 11:
			registers[data[1]] = pow(registers[data[1]], arg2);
			break;
		case 12:
			registers[data[1]] = arg2;
			break;
		case 13:
			registers[data[1]] |= arg2;
			break;
		case 14:
			registers[data[1]] &= arg2;
			break;
		case 15:
			registers[data[1]] = ~arg2;
			break;
		case 16:
			registers[data[1]] ^= arg2;
			break;
		case 17:
			registers[data[1]] = -arg2;
			break;
		case 18:
			registers[data[1]] = (registers[data[1]] << (arg2 % 16)) | (registers[data[1]] >> (16 - arg2 % 16));
			break;
		case 19:
			registers[data[1]] = (registers[data[1]] >> (arg2 % 16)) | (registers[data[1]] << (16 - arg2 % 16));
			break;
		case 20:
			registers[data[1]] = registers[data[1]] << arg2;
			break;
		case 21:
			registers[data[1]] = registers[data[1]] >> arg2;
			break;
		case 22:
			registers[data[1]] = rand() % arg2;
			break;
		case 23:
			for (int16_t i = 0; i < arg2; ++i)
				getc16(registers[data[1] + i]);
			break;
		case 24:
			for (int16_t i = 0; i < arg2; ++i)
				registers[data[1] + i] = work_file.available() ? work_file.read() : 0;
			break;
		case 25:
			for (int16_t i = 0; i < arg2; ++i)
				setc16(registers[data[1] + i]);
			break;
		case 26:
			for (int16_t i = 0; i < arg2; ++i)
				setc8(registers[data[1] + i]);
			break;
		case 27:
			if (work_file.available())
				for (int16_t i = 0; i < arg2; ++i)
					registers[data[1] + i] = dbytes(work_file.available() ? work_file.read() : 0, work_file.available() ? work_file.read() : 0);
			else
				return end_of_file;
			break;
		case 28:
			if (work_file.available())
				for (int16_t i = 0; i < arg2; ++i)
					registers[data[1] + i] = work_file.available() ? work_file.read() : 0;
			else
				return end_of_file;
			break;
		case 29:
			for (int16_t i = 0; i < arg2; ++i) {
				work_file.write(registers[data[1] + i] >> 8);
				work_file.write(registers[data[1] + i] & 255);
			}
			break;
		case 30:
			for (int16_t i = 0; i < arg2; ++i)
				work_file.write(registers[data[1] + i]);
			break;
		case 31:
			registers[data[1]] = digitalRead(arg2);
			break;
		case 32:
			registers[data[1]] = analogRead(arg2);
			break;
		case 33:
			digitalWrite(registers[data[1]], arg2);
			break;
		case 34:
			analogWrite(registers[data[1]], arg2);
			break;
		case 35:
			digitalWrite(arg2, registers[data[1]]);
			break;
		case 36:
			analogWrite(arg2, registers[data[1]]);
			break;
		case 37:
			cursorX = arg2 * LCD.getFontXsize();
			break;
		case 38:
			cursorY = arg2 * LCD.getFontYsize();
			break;
		case 39:
			LCD.setBackColor(VGA_COLORS[(arg2 >> 8) % 16]);
			LCD.setColor(VGA_COLORS[(arg2 & 255) % 16]);
			break;
		case 40:
			pinMode(arg2, OUTPUT);
			break;
		case 41:
			pinMode(arg2, INPUT);
			break;
		case 42:
			Serial.write(arg2);//not tested
			break;
		case 43:
			delay(arg2);
			break;
		case 44:
			registers[data[1]] = Serial.read();
			delta = 2;
			break;
		case 45:
			registers[data[1]] = registers[data[1]] + 1;
			delta = 2;
			break;
		case 46:
			registers[data[1]] = registers[data[1]] - 1;
			delta = 2;
			break;
		case 47:
			for (uint8_t i = 0; i < 255; ++i) {
				work_file_name[i] = registers[data[1] + i];
				if (registers[data[1] + i] == 0)
					break;
			}
			if (!SD.exists(work_file_name))
				return file_not_exist;
			work_file = SD.open(work_file_name, FILE_READ);
			if (!work_file)
				return opening_error;
			delta = 2;
			break;
		case 48:
			for (uint8_t i = 0; i < 255; ++i) {
				work_file_name[i] = registers[data[1] + i];
				if (registers[data[1] + i] == 0)
					break;
			}
			if (SD.exists(work_file_name) && !SD.remove(work_file_name))
				return rewrite_error;
			work_file = SD.open(work_file_name, FILE_WRITE);
			if (!work_file)
				return opening_error;
			delta = 2;
			break;
		case 49:
			for (uint8_t i = 0; i < 255; ++i) {
				work_file_name[i] = registers[data[1] + i];
				if (registers[data[1] + i] == 0)
					break;
			}
			work_file = SD.open(work_file_name, FILE_WRITE);
			if (!work_file)
				return opening_error;
			delta = 2;
			break;
		case 50:
			for (uint8_t i = 0; i < 255; ++i) {
				work_file_name[i] = registers[data[1] + i];
				if (registers[data[1] + i] == 0)
					break;
			}
			SD.mkdir(work_file_name);
			delta = 2;
			break;
		case 51:
			for (uint8_t i = 0; i < 255; ++i) {
				work_file_name[i] = registers[data[1] + i];
				if (registers[data[1] + i] == 0)
					break;
			}
			SD.rmdir(work_file_name);
			delta = 2;
			break;
		case 52:
			for (uint8_t i = 0; i < 255; ++i) {
				work_file_name[i] = registers[data[1] + i];
				if (registers[data[1] + i] == 0)
					break;
			}
			SD.remove(work_file_name);
			delta = 2;
			break;
		case 53:
			if (registers[data[1]] == 0)
				exe_file.seek(dbytes(data[2], data[3]));
			break;
		case 54:
			if (registers[data[1]] != 0)
				exe_file.seek(dbytes(data[2], data[3]));
			break;
		case 55:
			if (registers[data[1]] > 0)
				exe_file.seek(dbytes(data[2], data[3]));
			break;
		case 56:
			if (registers[data[1]] < 0)
				exe_file.seek(dbytes(data[2], data[3]));
			break;
		case 57:
			exe_file.seek(dbytes(data[1], data[2]));
			break;
		case 58:
			if (call == 255)
				return stack_overflow;
			call_stack[call] = exe_file.position();
			exe_file.seek(dbytes(data[1], data[2]));
			++call;
			break;
		default:
			return data[0];
			break;
	}
	return exe_ok;
}
void printDirectory(File dir, int numTabs) {
	while (true) {
		File entry = dir.openNextFile();
		if (!entry) break;
		for (int8_t i = 0; i < numTabs; ++i)
			setc8('-');
		echo(entry.name(), strlen(entry.name()));
		if (entry.isDirectory()) {
			echoln("/");
			printDirectory(entry, numTabs + 1);
		} else {
			cursorX = LCD.getFontXsize() * 24;
			setc16(entry.size());
			setc8('\n');
		}
		entry.close();
	}
}
void vi() {
	File edit;
	char text_buf[128][16], t;
	int8_t i = 0, k = 0, page = 0;//i-th string, k-th character
	if (SD.exists(input_str)) {
		edit = SD.open(input_str, FILE_READ);
		for (uint8_t ti = 0; ti <= 127; ++ti) {
			t = 1;
			for (uint8_t tk = 0; tk <= 15; ++tk)
				if (edit.available() && t != 0) {
					t = edit.read();
					if (t < 32 || t == 127 || tk == 15) {
						t = 0;
						while (edit.peek() < 32 && edit.available())
							edit.read();
					}
					text_buf[ti][tk] = t;
				} else
					text_buf[ti][tk] = 0;
		}
		edit.close();
	} else
		for (uint8_t ti = 0; ti <= 127; ++ti)
			for (uint8_t tk = 0; tk <= 15; ++tk)
				text_buf[ti][tk] = 0;
	bool refresh = true;
	while (t != PS2_ESC) {
		if (k > 15) {
			text_buf[page * 16 + i][15] = 0;
			k = 0;
			++i;
		} else if (k < 0) {
			if (i > 0) {
				--i;
				for (k = 0; k < 15 && text_buf[page * 16 + i][k] != 0; ++k);
			} else
				k = 0;
		}
		if (i > 15 || i < 0 || refresh) {
			refresh = false;
			if (i < 0) {
				if (page > 0) {
					--page;
					i = 15;
				} else
					i = 0;
			}
			else if (i > 15) {
				i = 15;
				if (page < 7) {
					++page;
					i = 0;
				} else
					i = 15;
			}
			ClrScr();
			echo("ESC-exit TAB-save PAGE=");
			setc16(page);
			setc8('\n');
			setc8('\n');
			for (uint8_t ti = 0; ti <= 15; ++ti) {
				setc16(page * 16 + ti);
				if (page * 16 + ti < 100)
					setc8(' ');
				if (page * 16 + ti < 10)
					setc8(' ');
				echo(": ");
				for (uint8_t tk = 0; tk <= 15; ++tk)
					if (text_buf[page * 16 + ti][tk] == 0) {
						setc8('\n');
						break;
					} else
						setc8(text_buf[page * 16 + ti][tk]);
			}
		}
		if (text_buf[page * 16 + i][k] == 0)
			for (; k > 0 && text_buf[page * 16 + i][k - 1] == 0; --k);
		cursorX = (k + 5) * LCD.getFontXsize();
		cursorY = (i + 2) * LCD.getFontYsize();
		setc8('_');
		cursorX -= LCD.getFontXsize();
		while (!keyboard.available()) delay(16);
		t = keyboard.read();
		if (t == PS2_TAB) {
			SD.remove(input_str);
			edit = SD.open(input_str, FILE_WRITE);
			for (uint8_t ti = 0; ti <= 127; ++ti)
				for (uint8_t tk = 0; tk <= 15; ++tk)
					if (text_buf[ti][tk] != 0)
						edit.write(text_buf[ti][tk]);
					else {
						if (tk != 0 || text_buf[ti - 1][0] != 0)
							edit.write('\n');
						tk = 16;
					}
			edit.close();
		} else if (t == PS2_ENTER) {
			setc8(text_buf[page * 16 + i][k]);
			++i;
			if (text_buf[127][0] == 0) {
				for (int8_t ti = 127; ti >= i + 1; --ti)
					for (int8_t tk = 15; tk >= 0; --tk)
						text_buf[ti][tk] = text_buf[ti - 1][tk];
				for (int8_t tk = 0; tk < 15 - k; ++tk) {
					text_buf[page * 16 + i][tk] = text_buf[page * 16 + i - 1][k + tk + 1];
					text_buf[page * 16 + i - 1][k + tk + 1] = 0;
				}
				for (int8_t tk = 15 - k; tk <= 15; ++tk)
					text_buf[page * 16 + i][tk] = 0;
				refresh = true;
			}
			k = 0;
		} else if (t == PS2_DELETE) {
			setc8(text_buf[page * 16 + i][k]);
			cursorX -= LCD.getFontXsize();
			if (text_buf[page * 16 + i][0] == 0) {
				for (int8_t ti = page * 16 + i; ti <= 126; ++ti)
					for (int8_t tk = 0; tk <= 15; ++tk)
						text_buf[ti][tk] = text_buf[ti + 1][tk];
				for (int8_t tk = 0; tk <= 15; ++tk)
					text_buf[127][tk] = 0;
				refresh = true;
			} else if (k > 0) {
				cursorX -= LCD.getFontXsize();
				for (int8_t tk = k; tk <= 15; ++tk) {
					text_buf[page * 16 + i][tk - 1] = text_buf[i][tk];
					setc8(text_buf[page * 16 + i][tk]);
				}
			}
			--k;
		} else if (t == PS2_RIGHTARROW) {
			setc8(text_buf[page * 16 + i][k]);
			if (text_buf[page * 16 + i][k] == 0) {
				if (page < 7 || i < 15) {
					k = 0;
					++i;
				}
			} else
				++k;
		} else if (t == PS2_LEFTARROW) {
			setc8(text_buf[page * 16 + i][k]);
			--k;
		} else if (t == PS2_DOWNARROW) {
			if (page < 7 || i < 15) {
				setc8(text_buf[page * 16 + i][k]);
				++i;
			}
		} else if (t == PS2_UPARROW) {
			if (page > 0 || i > 0) {
				setc8(text_buf[page * 16 + i][k]);
				--i;
			}
		} else if (t == PS2_HOME) {
			setc8(text_buf[page * 16 + i][k]);
			i = 0;
		} else if (t == PS2_END) {
			setc8(text_buf[page * 16 + i][k]);
			i = 15;
		} else if (t == PS2_PAGEDOWN) {
			setc8(text_buf[page * 16 + i][k]);
			if (page < 7) {
				++page;
				refresh = true;
			}
			else
				i = 15;
		} else if (t == PS2_PAGEUP) {
			setc8(text_buf[page * 16 + i][k]);
			if (page > 0) {
				--page;
				refresh = true;
			}
			else
				i = 0;
		} else if (text_buf[page * 16 + i][14] == 0) {
			for (int8_t tk = 14; tk > k; --tk)
				text_buf[page * 16 + i][tk] = text_buf[page * 16 + i][tk - 1];
			text_buf[page * 16 + i][k] = t;
			for (int8_t tk = k; tk < 15; ++tk)
				setc8(text_buf[page * 16 + i][tk]);
			++k;
		}
	}
	ClrScr();
}
void asm_lite() {
	File source, bin;
	char op[4], arg1[7], arg2[7], t;
	uint16_t labels[32], sz = 0;
	uint8_t opcode, i;
	if (SD.exists(input_str)) {
		source = SD.open(input_str, FILE_READ);
		input_str[0] = input_str[0] == '~' ? '_' : '~';
		SD.remove(input_str);
		bin = SD.open(input_str, FILE_WRITE);
		while (source.available()) {
			t = ' ';
			i = 0;
			while (isspace(t) && source.available())
				t = source.read();
			while (!isspace(t) && source.available()) {
				op[i] = t;
				t = source.read();
				++i;
			}
			op[i] = 0;
			while (isspace(t) && source.available() && t != '\n')
				t = source.read();
			i = 0;
			while (!isspace(t) && source.available()) {
				arg1[i] = t;
				t = source.read();
				++i;
			}
			arg1[i] = 0;
			while (isspace(t) && source.available() && t != '\n')
				t = source.read();
			i = 0;
			while (!isspace(t) && source.available()) {
				arg2[i] = t;
				t = source.read();
				++i;
			}
			arg2[i] = 0;
			if (strlen(op) == 3) {
				if (cmp(op, "nop"))
					opcode = 0;
				else if (cmp(op, "ret"))
					opcode = 1;
				else if (cmp(op, "clf"))
					opcode = 2;
				else if (cmp(op, "end"))
					opcode = 3;
				else if (cmp(op, "clr"))
					opcode = 4;
				else if (cmp(op, "xch"))
					opcode = 5;
				else if (cmp(op, "add"))
					opcode = 6;
				else if (cmp(op, "sub"))
					opcode = 7;
				else if (cmp(op, "mul"))
					opcode = 8;
				else if (cmp(op, "div"))
					opcode = 9;
				else if (cmp(op, "mod"))
					opcode = 10;
				else if (cmp(op, "pow"))
					opcode = 11;
				else if (cmp(op, "mov"))
					opcode = 12;
				else if (cmp(op, "orl"))
					opcode = 13;
				else if (cmp(op, "and"))
					opcode = 14;
				else if (cmp(op, "not"))
					opcode = 15;
				else if (cmp(op, "xor"))
					opcode = 16;
				else if (cmp(op, "neg"))
					opcode = 17;
				else if (cmp(op, "rol"))
					opcode = 18;
				else if (cmp(op, "ror"))
					opcode = 19;
				else if (cmp(op, "shl"))
					opcode = 20;
				else if (cmp(op, "shr"))
					opcode = 21;
				else if (cmp(op, "rnd"))
					opcode = 22;
				else if (cmp(op, "rci"))
					opcode = 23;
				else if (cmp(op, "rcc"))
					opcode = 24;
				else if (cmp(op, "wci"))
					opcode = 25;
				else if (cmp(op, "wcc"))
					opcode = 26;
				else if (cmp(op, "rfi"))
					opcode = 27;
				else if (cmp(op, "rfc"))
					opcode = 28;
				else if (cmp(op, "wfi"))
					opcode = 29;
				else if (cmp(op, "wfc"))
					opcode = 30;
				else if (cmp(op, "rdd"))
					opcode = 31;
				else if (cmp(op, "rda"))
					opcode = 32;
				else if (cmp(op, "wfd"))
					opcode = 33;
				else if (cmp(op, "wfa"))
					opcode = 34;
				else if (cmp(op, "wsd"))
					opcode = 35;
				else if (cmp(op, "wsa"))
					opcode = 36;
				else if (cmp(op, "stx"))
					opcode = 37;
				else if (cmp(op, "sty"))
					opcode = 38;
				else if (cmp(op, "stc"))
					opcode = 39;
				else if (cmp(op, "pni"))
					opcode = 40;
				else if (cmp(op, "pno"))
					opcode = 41;
				else if (cmp(op, "wsr"))
					opcode = 42;
				else if (cmp(op, "dly"))
					opcode = 43;
				else if (cmp(op, "rsr"))
					opcode = 44;
				else if (cmp(op, "inc"))
					opcode = 45;
				else if (cmp(op, "dec"))
					opcode = 46;
				else if (cmp(op, "ofr"))
					opcode = 47;
				else if (cmp(op, "ofw"))
					opcode = 48;
				else if (cmp(op, "ofa"))
					opcode = 49;
				else if (cmp(op, "mkd"))
					opcode = 50;
				else if (cmp(op, "rmd"))
					opcode = 51;
				else if (cmp(op, "rmf"))
					opcode = 52;
				else if (cmp(op, "ife"))
					opcode = 53;
				else if (cmp(op, "ifn"))
					opcode = 54;
				else if (cmp(op, "ifb"))
					opcode = 55;
				else if (cmp(op, "ifs"))
					opcode = 56;
				else if (cmp(op, "jmp"))
					opcode = 57;
				else if (cmp(op, "cal"))
					opcode = 58;
				else if (cmp(op, "lab"))
					opcode = 59;
				else if (cmp(op, "prс"))
					opcode = 60;
				//else if (cmp(op, "dat"))
				//	opcode = 61;
				if (opcode <= 4) {
					bin.write(opcode);
					sz += 1;
				} else if (opcode <= 5) {
					bin.write(opcode);
					for (uint8_t k = 0; k < 6; ++k) {
						arg1[k] = arg1[k + 1];
						arg2[k] = arg2[k + 1];
					}
					bin.write(atoi(arg1));
					bin.write(atoi(arg2));
					sz += 3;
				} else if (opcode <= 36) {
					if (isdigit(arg2[0])) {
						bin.write(opcode);
						for (uint8_t k = 0; k < 6; ++k)
							arg1[k] = arg1[k + 1];
						bin.write(atoi(arg1));
						bin.write(atoi(arg2) >> 8);
						bin.write(atoi(arg2) & 255);
						sz += 4;
					} else {
						bin.write(opcode + 128);
						for (uint8_t k = 0; k < 6; ++k) {
							arg1[k] = arg1[k + 1];
							arg2[k] = arg2[k + 1];
						}
						bin.write(atoi(arg1));
						bin.write(atoi(arg2));
						sz += 3;
					}
				} else if (opcode <= 43) {
					if (isdigit(arg1[0])) {
						bin.write(opcode);
						bin.write(atoi(arg1) >> 8);
						bin.write(atoi(arg1) & 255);
						sz += 3;
					} else {
						bin.write(opcode + 128);
						for (uint8_t k = 0; k < 6; ++k)
							arg1[k] = arg1[k + 1];
						bin.write(atoi(arg1));
						sz += 2;
					}
				} else if (opcode <= 52) {
					bin.write(opcode);
					for (uint8_t k = 0; k < 6; ++k)
						arg1[k] = arg1[k + 1];
					bin.write(atoi(arg1));
					sz += 2;
				} else if (opcode <= 56) {
					bin.write(opcode);
					for (uint8_t k = 0; k < 6; ++k) {
						arg1[k] = arg1[k + 1];
						arg2[k] = arg2[k + 1];
					}
					bin.write(atoi(arg1));
					bin.write(labels[atoi(arg2)] >> 8);
					bin.write(labels[atoi(arg2)] & 255);
					sz += 3;
				} else if (opcode <= 58) {
					bin.write(opcode);
					for (uint8_t k = 0; k < 6; ++k)
						arg1[k] = arg1[k + 1];
					bin.write(labels[atoi(arg1)] >> 8);
					bin.write(labels[atoi(arg1)] & 255);
					sz += 2;
				} else if (opcode <= 60) {
					for (uint8_t k = 0; k < 6; ++k)
						arg1[k] = arg1[k + 1];
					labels[atoi(arg1)] = sz;
				}
			}
		}
		source.close();
		bin.close();
	}
}
bool get_file_name(const bool &m = false) {
	if (input_str[0] != '/') {
		strcpy(path_buf, input_str);
		strcpy(input_str, directory);
		strcat(input_str, path_buf);
	}
	if (SD.exists(input_str) || !m || cmp(input_str, "/"))
		return true;
	echoln("File not exist");
	return false;
}
int16_t math(char* c) {
#define STACK_SIZE 32
	uint8_t i = 0, q, stsz = 0;
	char exp[8];
	uint16_t bstack[STACK_SIZE];
	while (c[i] != 0) {
		q = 0;
		while (c[i] == ' ') ++i;
		while (c[i] != 0 && c[i] != ' ')
			exp[q++] = c[i++];
		exp[q] = 0;
		if (isalpha(exp[0]) && exp[1] == 0) {
			bstack[stsz++] = registers[exp[0] - (exp[0] > 90 ? 97 : 65)];
		} else if (isdigit(exp[0])) {
			bstack[stsz++] = atoi(exp);
		} else if (cmp(exp, "rand")) {
			bstack[stsz++] = rand();
		} else if (cmp(exp, "time")) {
			bstack[stsz++] = now() % 0x8000;
		} else if (cmp(exp, "free")) {
			bstack[stsz++] = free_memory();
		} else if (stsz > 0) {
			if (cmp(exp, "~")) {
				bstack[stsz - 1] = ~bstack[stsz - 1];
			} else if (cmp(exp, "`")) {
				bstack[stsz - 1] = -bstack[stsz - 1];
			} else if (stsz > 1) {
				if (exp[1] == 0) {
					if (exp[0] == '+')
						bstack[stsz - 2] += bstack[stsz - 1];
					else if (exp[0] == '-')
						bstack[stsz - 2] -= bstack[stsz - 1];
					else if (exp[0] == '*')
						bstack[stsz - 2] *= bstack[stsz - 1];
					else if (exp[0] == '/')
						bstack[stsz - 2] /= bstack[stsz - 1];
					else if (exp[0] == '%')
						bstack[stsz - 2] %= bstack[stsz - 1];
					else if (exp[0] == '|')
						bstack[stsz - 2] |= bstack[stsz - 1];
					else if (exp[0] == '^')
						bstack[stsz - 2] ^= bstack[stsz - 1];
					else if (exp[0] == '&')
						bstack[stsz - 2] &= bstack[stsz - 1];
					else if (exp[0] == '=')
						bstack[stsz - 2] = (bstack[stsz - 2] == bstack[stsz - 1] ? 1 : 0);
					else if (exp[0] == '<')
						bstack[stsz - 2] = (bstack[stsz - 2] < bstack[stsz - 1] ? 1 : 0);
					else if (exp[0] == '>')
						bstack[stsz - 2] = (bstack[stsz - 2] > bstack[stsz - 1] ? 1 : 0);
					else if (exp[0] == '!')
						bstack[stsz - 2] = (bstack[stsz - 2] != bstack[stsz - 1] ? 1 : 0);
					else {
						echoln("er_op");
						return 0;
					}
					--stsz;
				} else if (cmp(exp, "<=")) {
					bstack[stsz - 2] = (bstack[stsz - 2] <= bstack[stsz - 1] ? 1 : 0);
					--stsz;
				} else if (cmp(exp, ">=")) {
					bstack[stsz - 2] = (bstack[stsz - 2] >= bstack[stsz - 1] ? 1 : 0);
					--stsz;
				} else if (cmp(exp, "<<")) {
					bstack[stsz - 2] = bstack[stsz - 2] << bstack[stsz - 1];
					--stsz;
				} else if (cmp(exp, ">>")) {
					bstack[stsz - 2] = bstack[stsz - 2] >> bstack[stsz - 1];
					--stsz;
				} else if (cmp(exp, "pow")) {
					bstack[stsz - 2] = pow(bstack[stsz - 2], bstack[stsz - 1]);
					--stsz;
				} else {
					echoln("er_ch");
					return 0;
				}
			} else {
				echoln("er_low_st_2");
				return 0;
			}
		} else {
			echoln("er_low_st_1");
			return 0;
		}
		if (stsz > STACK_SIZE) {
			echoln("stack_ower");
			return 0;
		}
	}
	if (stsz != 1) {
		echoln("er_mast_op");
		return 0;
	}
	return bstack[0];
}
void get_file_str(File &ofile, char* c) {
	uint8_t i = 0;
	while (ofile.peek() + 1 <= 32 && ofile.available()) ofile.read();
	for (; i < 255 && (ofile.peek() + 1) > 32; ++i)
		c[i] = ofile.read();
	c[i] = 0;
}
void shell() {
	File ofile;
	if (input_str[0] == 0)
		return;
	uint8_t i = 0, clnum;
	for (; i < 7; ++i)
		if (input_str[i] != ' ' && input_str[i] != 0)
			comline[i] = input_str[i];
		else
			break;
	comline[i] = 0;
	while (input_str[i] == ' ') ++i;
	clnum = i;
	while (input_str[i] != 0) {
		input_str[i - clnum] = input_str[i];
		++i;
	}
	input_str[i - clnum] = 0;
	if (cmp(comline, "ls")) {
		if (get_file_name(true)) {
			ofile = SD.open(input_str);
			printDirectory(ofile, 0);
			ofile.close();
		}
	} else if (cmp(comline, "uptime")) {
		setc16(millis() / 1000);
		echoln(" s");
	} else if (cmp(comline, "vi")) {
		get_file_name();
		vi();
	} else if (cmp(comline, "asm")) {
		if (get_file_name(true))
			asm_lite();
	} else if (cmp(comline, "hex")) {
		if (get_file_name(true)) {
			ofile = SD.open(input_str, FILE_READ);
			while (ofile.available()) {
				if (clnum % 8 == 0) {
					setc8(hexnums[clnum >> 4]);
					setc8(hexnums[clnum & 15]);
					echo(" | ");
				}
				i = ofile.read();
				setc8(hexnums[i >> 4]);
				setc8(hexnums[i & 15]);
				setc8(' ');
				if ((clnum + 1) % 8 == 0)
					setc8('\n');
				++clnum;
			}
			ofile.close();
			setc8('\n');
		}
	} else if (cmp(comline, "cat")) {
		if (get_file_name(true)) {
			ofile = SD.open(input_str, FILE_READ);
			while (ofile.available())
				setc8(ofile.read());
			ofile.close();
			setc8('\n');
		}
	} else if (cmp(comline, "mkdir")) {
		get_file_name();
		SD.mkdir(input_str);
	} else if (cmp(comline, "rmdir")) {
		get_file_name();
		SD.rmdir(input_str);
	} else if (cmp(comline, "mk")) {
		get_file_name();
		File temp = SD.open(input_str, FILE_WRITE);
		temp.close();
	} else if (cmp(comline, "rm")) {
		get_file_name();
		SD.remove(input_str);
	} else if (cmp(comline, "cd")) {
		strcpy(path_buf, "");
		if (strcmp(input_str, "/")) {
			if (input_str[strlen(input_str) - 1] != '/')
				strcat(input_str, "/");
			if (input_str[0] != '/')
				strcpy(path_buf, directory);
			strcat(path_buf, input_str);
			if (SD.exists(path_buf)) {
				File temp = SD.open(path_buf);
				if (temp.isDirectory())
					strcpy(directory, path_buf);
				else
					echoln("Isn't dir");
				temp.close();
			} else
				echoln("Isn't exist");
		} else
			strcpy(directory, input_str);
	} else if (cmp(comline, "b+")) {
		if (BL < 255) {
			BL += 51;
			analogWrite(pin_BL, BL);
		}
	} else if (cmp(comline, "b-")) {
		if (BL > 0) {
			BL -= 51;
			analogWrite(pin_BL, BL);
		}
	} else if (cmp(comline, "clear")) {
		ClrScr();
	} else if (cmp(comline, "pwd")) {
		echoln(directory);
	} else if (cmp(comline, "'")) {
		;
	} else if (cmp(comline, "delay")) {
		delay(math(input_str));
	} else if (cmp(comline, "let")) {
		i = input_str[0] - (input_str[0] > 90 ? 97 : 65);
		input_str[0] = ' ';
		registers[i] = math(input_str);
	} else if (cmp(comline, "echo")) {
		if (input_str[0] == '"') {
			for (i = 1; i < 255 && input_str[i] != '"'; ++i)
				input_str[i - 1] = input_str[i];
			input_str[i - 1] = 0;
		} else if (isalpha(input_str[0]) && input_str[1] == 0) {
			itoa(registers[input_str[0] - (input_str[0] > 90 ? 97 : 65)], input_str, 10);
		} else {
			itoa(math(input_str), input_str, 10);
		}
		echoln(input_str);
	} else if (cmp(comline, "input")) {
		if (isalpha(input_str[0]) && input_str[1] == 0)
			getc16(registers[input_str[0] - (input_str[0] > 90 ? 97 : 65)]);
		else
			echoln("not a var");
	} else if (cmp(comline, "sh")) {
		if (get_file_name(true)) {
			ofile = SD.open(input_str, FILE_READ);
			while (ofile.available()) {
				i = 0;
				get_file_str(ofile, input_str);
				if (cmp(input_str, "end")) break;
				if (cmp(input_str, "goto")) {
					get_file_str(ofile, input_str);
					ofile.seek(0);
					while (i != math(input_str) && ofile.available())
						if (ofile.read() == '\n') ++i;
				} else if (cmp(input_str, "if")) {
					get_file_str(ofile, input_str);
					if (math(input_str)) {
						get_file_str(ofile, input_str);
						ofile.seek(0);
						while (i != math(input_str) && ofile.available())
							if (ofile.read() == '\n') ++i;
					} else
						get_file_str(ofile, input_str);
				} else
					shell();
			}
			ofile.close();
		}
	} else if (cmp(comline, "man") || cmp(comline, "help") || cmp(comline, "?")) {
		echoln("asm F    compile file");
		echoln("b+       increase backlight");
		echoln("b-       decrease backlight");
		echoln("cat F    show file");
		echoln("cd D     change working dir");
		echoln("clear    clear the display");
		echoln("color FB set font/back colors");
		echoln("date M   set M = DDMMYYHHMM");
		echoln("date     echo datetime");
		echoln("echo E   print expression");
		echoln("input V  set var = input");
		echoln("hex F    show file in hex");
		echoln("let V E  set var = expression");
		echoln("ls D     file listing");
		echoln("mk F     make file");
		echoln("mkdir D  make directory");
		echoln("pwd      print current dir");
		echoln("rm F     remove file");
		echoln("rmdir D  remove directory");
		echoln("run F    file execution");
		echoln("sh F     start script");
		echoln("-goto/S  go to string (/=NL)");
		echoln("-if/E/S  if E go to S");
		echoln("-end     end script");
		echoln("uptime   display uptime");
		echoln("vi F     text editor");
		echoln("' stuff   remark/comment");
	} else if (cmp(comline, "date")) {
		Wire.beginTransmission(0x68);
		Wire.write(0x01);
		Wire.write(dec2bcd((input_str[8] -48) * 10 + input_str[9] -48));
		Wire.write(dec2bcd((input_str[6] -48) * 10 + input_str[7] -48) & 0b10111111);
		Wire.write(0x00);
		Wire.write(dec2bcd((input_str[0] -48) * 10 + input_str[1] -48));
		Wire.write(dec2bcd((input_str[2] -48) * 10 + input_str[3] -48));
		Wire.write(dec2bcd((input_str[4] -48) * 10 + input_str[5] -48));
		Wire.endTransmission();
		status();
	} else if (cmp(comline, "color")) {
		LCD.setColor(VGA_COLORS[input_str[0] % 16]);
		LCD.setBackColor(VGA_COLORS[input_str[1] % 16]);
	} else if (cmp(comline, "run")) {
		if (get_file_name(true)) {
			exe_file = SD.open(input_str, FILE_READ);
			if (exe_file) {
				while (exe_file.available()) {
					for (i = 0; i < 4; ++i)
						data[i] = exe_file.available() ? exe_file.read() : 0;
					exe_code = exe();
					if (exe_code == exe_end)
						break;
					else if (exe_code != exe_ok) {
						setc16(exe_code);
						echoln(" runtime error");
						break;
					}
					if (delta != 0 && delta != 4)
						exe_file.seek(exe_file.position() - (4 - delta));
				}
				exe_file.close();
			} else
				echoln("Opening error");
		}
	} else {
		echo("Bad command: ");
		echoln(comline);
	}
}
void setup() {
	pinMode(pin_SD, OUTPUT);
	pinMode(pin_BL, OUTPUT);
	LCD.InitLCD();
	LCD.setFont(BigFont);
	LCD.setColor(VGA_LIME);
	analogWrite(pin_BL, BL);
	keyboard.begin(ps2key_data_pin, ps2key_int_pin);
	Wire.begin();
	cursorYbar = LCD.getDisplayYSize() - LCD.getFontYsize();
	ClrScr();
	timer_init_ISR_1Hz(TIMER_DEFAULT);
	echoln("MicConOS 0.9b");
	if (!SD.begin(pin_SD))
		echoln("SD fail");
}
void loop() {
	echo(directory);
	getstr(input_str);
	shell();
}
void timer_handle_interrupts(int timer) {
	static uint8_t count = 0;
	if (!(++count % 8)) status();
}
