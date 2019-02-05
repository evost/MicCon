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
#define lcd_font SmallFont
#include <Arduino.h>
#include <Wire.h>
#include <timer-api.h>
#include <SD.h>
#include <UTFT.h>
#include <PS2Keyboard.h>
#ifdef __arm__
	extern "C" char* sbrk(int incr);
#else //!__arm__
	extern char *__brkval;
#endif
char input_str[256];//current executable, 255 for name and path, and 1 for null terminator
char directory[256] = "/";//working directory
char path_buf[256];
char work_file_name[13];//file with which the program works
char comline[8];
uint8_t BL = 255;//brightness of the backlight
int16_t vars[26];
int16_t cursorY;
int16_t cursorX;
uint16_t cursorYbar;
PS2Keyboard keyboard;
UTFT LCD(ILI9486, 38, 39, 40, 41);
extern uint8_t lcd_font[];
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
bool cmp(const char* c1, const char* c2, const uint8_t &len = 255) {
	for (uint8_t i = 0; i < len; ++i) {
		if (c1[i] != c2[i]) return false;
		if (c1[i] == 0) break;
	}
	return true;
}
void swapcolors() {
	uint32_t col = LCD.getColor();
	LCD.setColor(LCD.getBackColor());
	LCD.setBackColor(col);
}
void status() {
	char bar[32] = "", me[7], sTime[3];
	uint8_t mm, hh, d, m, y;
	Wire.beginTransmission(0x68);
	Wire.write(0x01);
	Wire.endTransmission();
	Wire.requestFrom(0x68, 6);
	mm = bcd2bin(Wire.read());
	hh = bcd2bin(Wire.read());
	Wire.read();
	d = bcd2bin(Wire.read());
	m = bcd2bin(Wire.read());
	y = bcd2bin(Wire.read());
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
	if (k == PS2_BACKSPACE) {
		if (cursorX == 0) {
			cursorX = LCD.getDisplayXSize() - LCD.getFontXsize();
			cursorY -= LCD.getFontYsize();
		} else
			cursorX -= LCD.getFontXsize();
		LCD.printChar(' ', cursorX, cursorY);
	} else {
		if ((cursorX > LCD.getDisplayXSize() - LCD.getFontXsize()) || k == '\n') {
			cursorX = 0;
			cursorY += LCD.getFontYsize();
			if (cursorY > LCD.getDisplayYSize() - 2 * LCD.getFontYsize()) {
				while (keyboard.read() != PS2_ENTER) delay(16);
				ClrScr();
			}
		}
		if (k != '\n') {
			LCD.printChar(k == 0 ? ' ' : k, cursorX, cursorY);
			cursorX += LCD.getFontXsize();
		}
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
uint8_t getstr(char* str, const uint8_t &len = 255, const uint8_t &type = 1) {
	setc8('>');
	uint8_t i = type - 1, buf = 0;
	str[i] = 0;
	while (buf != PS2_ENTER) {
		swapcolors();
		setc8(str[i]);
		cursorX -= LCD.getFontXsize();
		swapcolors();
		while (!keyboard.available()) delay(16);
		buf = keyboard.read();
		if (buf == PS2_BACKSPACE) {
			if (i > 0 && str[i] == 0) {
				setc8(' ');
				i -= type;
				str[i] = 0;
				setc8(PS2_BACKSPACE);
				setc8(PS2_BACKSPACE);
			}
		} else if (buf == PS2_LEFTARROW) {
			if (i > 0) {
				setc8(str[i]);
				cursorX -= 2 * LCD.getFontXsize();
				i -= type;
			}
		} else if (buf == PS2_RIGHTARROW) {
			if (str[i] != 0) {
				setc8(str[i]);
				i += type;
			}
		} else if (buf == PS2_HOME) {
			if (i > 0) {
				setc8(str[i]);
				cursorX -= (i + 1) * LCD.getFontXsize();
				i = 0;
			}
		} else if (buf == PS2_END) {
			setc8(str[i]);
			while(str[i] != 0)
				i += type;
		} else if (buf == PS2_ENTER) {
			setc8(str[i]);
			setc8('\n');
		} else if (buf >= 32 && i < len * type) {
			if (str[i] == 0)
				str[i + type] = 0;
			str[i] = buf;
			i += type;
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
uint8_t exe(File &exe_file, File &work_file, int16_t* registers, int8_t* data, int8_t &delta, uint8_t &call, uint16_t* call_stack) {
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
			getstr((char*)(registers + data[1]), arg2, 2);
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
		case 48:
		case 49:
		case 50:
		case 51:
		case 52:
			for (uint8_t i = 0; i <= 13 && registers[data[1] + i]; ++i)
				work_file_name[i] = registers[data[1] + i];
			strcpy(path_buf, directory);
			strcat(path_buf, work_file_name);
			switch(opcode) {
			case 47:
				if (!SD.exists(work_file_name))
					return file_not_exist;
				work_file = SD.open(work_file_name, FILE_READ);
				if (!work_file)
					return opening_error;
				break;
			case 48:
				if (SD.exists(work_file_name) && !SD.remove(work_file_name))
					return rewrite_error;
				work_file = SD.open(work_file_name, FILE_WRITE);
				if (!work_file)
					return opening_error;
				break;
			case 49:
				work_file = SD.open(work_file_name, FILE_WRITE);
				if (!work_file)
					return opening_error;
				break;
			case 50:
				SD.mkdir(work_file_name);
				break;
			case 51:
				SD.rmdir(work_file_name);
				break;
			case 52:
				SD.remove(work_file_name);
				break;
			}
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
void printDirectory(File &dir, const uint8_t numTabs) {
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
			cursorX = LCD.getFontXsize() * (LCD.getDisplayXSize() / LCD.getFontXsize() - 5);
			setc16(entry.size());
			setc8('\n');
		}
		entry.close();
	}
}
void vi() {
	#define STRLEN 29
	#define STRLEN_1 28
	#ifdef __arm__
		#define STRCOUNT 576
		#define STRCOUNT_1 575
	#else
		#define STRCOUNT 64
		#define STRCOUNT_1 63
	#endif
	uint8_t SHOWSTRCOUNT = (LCD.getDisplayYSize() / LCD.getFontYsize()) - 4;
	uint8_t SHOWSTRCOUNT_1 = SHOWSTRCOUNT - 1;
	uint8_t MAX_PAGES = STRCOUNT / SHOWSTRCOUNT;
	uint8_t MAX_PAGES_1 = MAX_PAGES - 1;
	char text_buf[STRCOUNT][STRLEN], t = 1;
	//i-th string, k-th character
	int16_t i = 0;
	int8_t k = 0, page = 0;
	if (SD.exists(input_str)) {
		File exe_file = SD.open(input_str, FILE_READ);
		for (int16_t ti = 0; ti < STRCOUNT; ++ti) {
			for (uint8_t tk = 0, t = 1; tk < STRLEN; ++tk)
				if (exe_file.available() && t) {
					t = exe_file.read();
					if (t < ' ' || tk == STRLEN_1) {
						t = 0;
						while (exe_file.peek() < ' ' && exe_file.peek() != '\n' && exe_file.available())
							exe_file.read();
					}
					text_buf[ti][tk] = t;
				} else
					text_buf[ti][tk] = 0;
		}
		exe_file.close();
	} else
		for (int16_t ti = 0; ti <= STRCOUNT_1; ++ti)
			for (uint8_t tk = 0; tk <= STRLEN_1; ++tk)
				text_buf[ti][tk] = 0;
	bool refresh = true;
	while (true) {
		if (k > 0)
			if (text_buf[page * SHOWSTRCOUNT + i][k - 1] == 0) {
				for (k = 0;k < STRLEN_1 && text_buf[page * SHOWSTRCOUNT + i][k]; ++k);
			}
		if (k > STRLEN_1) {
			if (page * SHOWSTRCOUNT + i < STRCOUNT_1) {
				k = 0;
				++i;
			} else
				k = STRLEN_1;
		} else if (k < 0) {
			if (i > 0 || page > 0) {
				--i;
				for (k = 0; k <= STRLEN_1 && text_buf[page * SHOWSTRCOUNT + i][k]; ++k);
			} else
				k = 0;
		}
		if (i > SHOWSTRCOUNT_1 || i < 0 || page < 0 || page > MAX_PAGES_1 || refresh) {
			refresh = false;
			if (i < 0) {
				if (page > 0) {
					--page;
					i = SHOWSTRCOUNT_1;
				} else
					i = 0;
			}
			else if (i > SHOWSTRCOUNT_1) {
				if (page < MAX_PAGES_1) {
					++page;
					i = 0;
					k = 0;
				} else
					i = SHOWSTRCOUNT_1;
			}
			ClrScr();
			for (int16_t ti = 0; ti <= SHOWSTRCOUNT_1; ++ti) {
				setc16(page * SHOWSTRCOUNT + ti);
				if (page * SHOWSTRCOUNT + ti < 100)
					setc8(' ');
				if (page * SHOWSTRCOUNT + ti < 10)
					setc8(' ');
				setc8(':');
				for (uint8_t tk = 0; tk <= STRLEN_1; ++tk)
					if (text_buf[page * SHOWSTRCOUNT + ti][tk] == 0) {
						setc8('\n');
						break;
					} else
						setc8(text_buf[page * SHOWSTRCOUNT + ti][tk]);
			}
			echoln(input_str);
			setc8(page + (page < 10 ? 48 : (65 - 10)));
			setc8(':');
			echo("q-exit w-save");
		}
		cursorX = (k + 4) * LCD.getFontXsize();
		cursorY = i * LCD.getFontYsize();
		{
			swapcolors();
			setc8(text_buf[page * SHOWSTRCOUNT + i][k]);
			swapcolors();
		}
		cursorX -= LCD.getFontXsize();
		while (!keyboard.available()) delay(16);
		t = keyboard.read();
		if (text_buf[page * SHOWSTRCOUNT + i][STRLEN - 2] == 0 && t >= ' ' && t != PS2_BACKSPACE) {
			for (uint8_t tk = STRLEN - 2; tk >= k + 1; --tk)
				text_buf[page * SHOWSTRCOUNT + i][tk] = text_buf[page * SHOWSTRCOUNT + i][tk - 1];
			text_buf[page * SHOWSTRCOUNT + i][k] = t;
			for (uint8_t tk = k; tk < STRLEN_1; ++tk)
				setc8(text_buf[page * SHOWSTRCOUNT + i][tk]);
			++k;
		} else {
			setc8(text_buf[page * SHOWSTRCOUNT + i][k]);
			if (t == PS2_ESC) {
				cursorX = LCD.getFontXsize() * 2;
				cursorY = LCD.getFontYsize() * SHOWSTRCOUNT;
				echo("             ");
				cursorX = LCD.getFontXsize() * 2;
				//cursorY = LCD.getFontYsize() * SHOWSTRCOUNT;
				getstr(comline, 7);
				if (comline[0] != 0) {
					if (cmp(comline, "w")) {
						SD.remove(input_str);
						File exe_file = SD.open(input_str, FILE_WRITE);
						int16_t p = 0;
						for (int16_t ti = STRCOUNT; ti > 0; --ti)
							if (text_buf[ti - 1][0] != 0) {
								p = ti;
								break;
							}
						for (int16_t ti = 0; ti < p; ++ti)
							for (uint8_t tk = 0; tk < STRLEN_1; ++tk)
								if (text_buf[ti][tk])
									exe_file.write(text_buf[ti][tk]);
								else {
									exe_file.write('\n');
									tk = STRLEN;
								}
						exe_file.close();
					} else if (cmp(comline, "q")) {
						break;
					} else {
						page = (atoi(comline) / SHOWSTRCOUNT) % MAX_PAGES;
						i = atoi(comline) % SHOWSTRCOUNT;
					}
				}
				refresh = true;
			} else if (t == PS2_ENTER) {
				if (page * SHOWSTRCOUNT + i < STRCOUNT_1) {
					++i;
					if (text_buf[STRCOUNT_1][0] == 0) {
						for (int16_t ti = STRCOUNT_1; ti >= page * SHOWSTRCOUNT + i + 1; --ti)
							for (uint8_t tk = STRLEN_1 + 1; tk > 0; --tk)
								text_buf[ti][tk - 1] = text_buf[ti - 1][tk - 1];
						for (uint8_t tk = 0; tk <= STRLEN_1 - k; ++tk) {
							text_buf[page * SHOWSTRCOUNT + i][tk] = text_buf[page * SHOWSTRCOUNT + i - 1][k + tk];
							text_buf[page * SHOWSTRCOUNT + i - 1][k + tk] = 0;
						}
						for (uint8_t tk = STRLEN_1 - k + 1; tk <= STRLEN_1; ++tk)
							text_buf[page * SHOWSTRCOUNT + i][tk] = 0;
						refresh = true;
					}
					k = 0;
				}
			} else if (t == PS2_BACKSPACE || t == PS2_DELETE) {
				if (t == PS2_DELETE && PS2_BACKSPACE != PS2_DELETE) {
					if (text_buf[page * SHOWSTRCOUNT + i][k] == 0 || k >= STRLEN_1)
						continue;
					else
						++k;
				} else
					cursorX -= LCD.getFontXsize();
				if (text_buf[page * SHOWSTRCOUNT + i][0] == 0) {
					for (int16_t ti = page * SHOWSTRCOUNT + i; ti <= STRCOUNT - 2; ++ti)
						for (uint8_t tk = 0; tk <= STRLEN_1; ++tk)
							text_buf[ti][tk] = text_buf[ti + 1][tk];
					for (uint8_t tk = 0; tk <= STRLEN_1; ++tk)
						text_buf[STRCOUNT_1][tk] = 0;
					refresh = true;
				} else if (k > 0) {
					cursorX -= LCD.getFontXsize();
					for (uint8_t tk = k; tk <= STRLEN_1; ++tk) {
						text_buf[page * SHOWSTRCOUNT + i][tk - 1] = text_buf[page * SHOWSTRCOUNT + i][tk];
						setc8(text_buf[page * SHOWSTRCOUNT + i][tk]);
					}
				}
				--k;
			} else if (t == PS2_RIGHTARROW) {
				if (text_buf[page * SHOWSTRCOUNT + i][k])
					++k;
				else if (page * SHOWSTRCOUNT + i != STRCOUNT_1){
					++i;
					k = 0;
				}
			} else if (t == PS2_LEFTARROW) {
				--k;
			} else if (t == PS2_DOWNARROW) {
				if (page * SHOWSTRCOUNT + i < STRCOUNT_1) ++i;
			} else if (t == PS2_UPARROW) {
				if (page * SHOWSTRCOUNT + i > 0) --i;
			} else if (t == PS2_HOME) {
				if (i)
					i = 0;
				else
					k = 0;
			} else if (t == PS2_END) {
				if (i != SHOWSTRCOUNT_1)
					i = SHOWSTRCOUNT_1;
				else
				for (k = 0;k < STRLEN_1 && text_buf[page * SHOWSTRCOUNT + i][k]; ++k);
			} else if (t == PS2_PAGEDOWN) {
				if (page < MAX_PAGES_1) {
					++page;
					refresh = true;
				} else {
					i = SHOWSTRCOUNT_1;
				for (k = 0;k < STRLEN_1 && text_buf[page * SHOWSTRCOUNT + i][k]; ++k);
				}
			} else if (t == PS2_PAGEUP) {
				if (page > 0) {
					--page;
					refresh = true;
				} else {
					i = 0;
					k = 0;
				}
			} else if (t == PS2_TAB) {
				for (k = 0;k < STRLEN_1 && text_buf[page * SHOWSTRCOUNT + i][k]; ++k);
			}
		}
	}
	ClrScr();
}
void asm_lite() {
	char op[4], arg1[7], arg2[7], t;
	uint16_t labels[32], sz = 0;
	uint8_t opcode, i;
	if (SD.exists(input_str)) {
		File exe_file = SD.open(input_str, FILE_READ);
		input_str[0] = (input_str[0] == '~' ? '_' : '~');
		SD.remove(input_str);
		File work_file = SD.open(input_str, FILE_WRITE);
		while (exe_file.available()) {
			t = ' ';
			i = 0;
			while (isspace(t) && exe_file.available())
				t = exe_file.read();
			while (!isspace(t) && exe_file.available()) {
				op[i] = t;
				t = exe_file.read();
				++i;
			}
			op[i] = 0;
			while (isspace(t) && exe_file.available() && t != '\n')
				t = exe_file.read();
			i = 0;
			while (!isspace(t) && exe_file.available()) {
				arg1[i] = t;
				t = exe_file.read();
				++i;
			}
			arg1[i] = 0;
			while (isspace(t) && exe_file.available() && t != '\n')
				t = exe_file.read();
			i = 0;
			while (!isspace(t) && exe_file.available()) {
				arg2[i] = t;
				t = exe_file.read();
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
				else {
					echoln("Unknown op");
					opcode = 0;
				}
				if (opcode <= 4) {
					work_file.write(opcode);
					sz += 1;
				} else if (opcode <= 5) {
					work_file.write(opcode);
					for (uint8_t k = 0; k < 6; ++k) {
						arg1[k] = arg1[k + 1];
						arg2[k] = arg2[k + 1];
					}
					work_file.write(atoi(arg1));
					work_file.write(atoi(arg2));
					sz += 3;
				} else if (opcode <= 36) {
					if (isdigit(arg2[0])) {
						work_file.write(opcode);
						for (uint8_t k = 0; k < 6; ++k)
							arg1[k] = arg1[k + 1];
						work_file.write(atoi(arg1));
						work_file.write(atoi(arg2) >> 8);
						work_file.write(atoi(arg2) & 255);
						sz += 4;
					} else {
						work_file.write(opcode + 128);
						for (uint8_t k = 0; k < 6; ++k) {
							arg1[k] = arg1[k + 1];
							arg2[k] = arg2[k + 1];
						}
						work_file.write(atoi(arg1));
						work_file.write(atoi(arg2));
						sz += 3;
					}
				} else if (opcode <= 43) {
					if (isdigit(arg1[0])) {
						work_file.write(opcode);
						work_file.write(atoi(arg1) >> 8);
						work_file.write(atoi(arg1) & 255);
						sz += 3;
					} else {
						work_file.write(opcode + 128);
						for (uint8_t k = 0; k < 6; ++k)
							arg1[k] = arg1[k + 1];
						work_file.write(atoi(arg1));
						sz += 2;
					}
				} else if (opcode <= 52) {
					work_file.write(opcode);
					for (uint8_t k = 0; k < 6; ++k)
						arg1[k] = arg1[k + 1];
					work_file.write(atoi(arg1));
					sz += 2;
				} else if (opcode <= 56) {
					work_file.write(opcode);
					for (uint8_t k = 0; k < 6; ++k) {
						arg1[k] = arg1[k + 1];
						arg2[k] = arg2[k + 1];
					}
					work_file.write(atoi(arg1));
					work_file.write(labels[atoi(arg2)] >> 8);
					work_file.write(labels[atoi(arg2)] & 255);
					sz += 3;
				} else if (opcode <= 58) {
					work_file.write(opcode);
					for (uint8_t k = 0; k < 6; ++k)
						arg1[k] = arg1[k + 1];
					work_file.write(labels[atoi(arg1)] >> 8);
					work_file.write(labels[atoi(arg1)] & 255);
					sz += 2;
				} else if (opcode <= 60) {
					for (uint8_t k = 0; k < 6; ++k)
						arg1[k] = arg1[k + 1];
					labels[atoi(arg1)] = sz;
				}
			}
		}
		exe_file.close();
		work_file.close();
		echoln(input_str);
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
int16_t math(const char* c) {
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
			bstack[stsz++] = vars[exp[0] - (exp[0] > 90 ? 97 : 65)];
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
			File entry = SD.open(input_str);
			printDirectory(entry, 0);
			entry.close();
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
			clnum = 0;
			File exe_file = SD.open(input_str, FILE_READ);
			while (exe_file.available()) {
				if (clnum % 8 == 0) {
					setc8(hexnums[clnum >> 4]);
					setc8(hexnums[clnum & 15]);
					echo(" | ");
				}
				i = exe_file.read();
				setc8(hexnums[i >> 4]);
				setc8(hexnums[i & 15]);
				setc8(' ');
				if ((clnum + 1) % 8 == 0)
					setc8('\n');
				++clnum;
			}
			exe_file.close();
			setc8('\n');
		}
	} else if (cmp(comline, "cat")) {
		if (get_file_name(true)) {
			File exe_file = SD.open(input_str, FILE_READ);
			while (exe_file.available())
				setc8(exe_file.read());
			exe_file.close();
		}
	} else if (cmp(comline, "mkdir")) {
		get_file_name();
		SD.mkdir(input_str);
	} else if (cmp(comline, "rmdir")) {
		get_file_name();
		SD.rmdir(input_str);
	} else if (cmp(comline, "mk")) {
		get_file_name();
		File work_file = SD.open(input_str, FILE_WRITE);
		work_file.close();
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
				File work_file = SD.open(path_buf);
				if (work_file.isDirectory())
					strcpy(directory, path_buf);
				else
					echoln("Isn't dir");
				work_file.close();
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
		vars[i] = math(input_str);
	} else if (cmp(comline, "echo")) {
		if (input_str[0] == '"') {
			for (i = 1; i < 255 && input_str[i] != '"'; ++i)
				input_str[i - 1] = input_str[i];
			input_str[i - 1] = 0;
		} else if (isalpha(input_str[0]) && input_str[1] == 0) {
			itoa(vars[input_str[0] - (input_str[0] > 90 ? 97 : 65)], input_str, 10);
		} else {
			itoa(math(input_str), input_str, 10);
		}
		echoln(input_str);
	} else if (cmp(comline, "input")) {
		if (isalpha(input_str[0]) && input_str[1] == 0)
			getc16(vars[input_str[0] - (input_str[0] > 90 ? 97 : 65)]);
		else
			echoln("not a var");
	} else if (cmp(comline, "sh")) {
		if (get_file_name(true)) {
			File exe_file = SD.open(input_str, FILE_READ);
			while (exe_file.available()) {
				i = 0;
				get_file_str(exe_file, input_str);
				if (cmp(input_str, "end")) break;
				if (cmp(input_str, "sh ", 3)) continue;
				if (cmp(input_str, "goto ", 5)) {
					uint8_t p = 4;
					while (p < 255) {
						input_str[p - 4] = input_str[p + 1];
						++p;
					}
					p = math(input_str);
					exe_file.seek(0);
					while (i != p && exe_file.available())
						if (exe_file.read() == '\n') ++i;
				} else if (cmp(input_str, "endif")) {
					;
				} else if (cmp(input_str, "if ", 3)) {
					uint8_t p = 2;
					while (p < 255) {
						input_str[p - 2] = input_str[p + 1];
						++p;
					}
					if (!math(input_str)) {
						while(!cmp(input_str, "endif")) get_file_str(exe_file, input_str);
					}
				} else
					shell();
			}
			exe_file.close();
		}
	} else if (cmp(comline, "man") || cmp(comline, "help") || cmp(comline, "?")) {
		echoln("asm F    compile file");
		echoln("b+       increase backlight");
		echoln("b-       decrease backlight");
		echoln("cat F    show file");
		echoln("cd D     change working dir");
		echoln("clear    clear the display");
		echoln("color FB set font/back colors");
		echoln("date M   set M = DDMMYYhhmm");
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
		if (input_str[0] == 0)
			echoln("DDMMYYhhmm");
		else {
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
		}
	} else if (cmp(comline, "color")) {
		if (strlen(input_str) == 2) {
			LCD.setColor(VGA_COLORS[(input_str[0] <= 57 ? input_str[0] - 48 : (input_str[0] <= 90 ? input_str[0] - 55 : input_str[0] - 87)) % 16]);
			LCD.setBackColor(VGA_COLORS[(input_str[1] <= 57 ? input_str[1] - 48 : (input_str[1] <= 90 ? input_str[1] - 55 : input_str[1] - 87)) % 16]);
		} else
			echoln("Incorrect");
	} else if (cmp(comline, "run")) {
		File work_file;
		int8_t exe_code;//code returned by exe()
		int8_t delta = 4;//length of command, 1..4
		int8_t data[4];//a place for pre-loading from a file operation code and the three following bytes
		uint8_t call = 0;//number of the next entry in the call stack
		int16_t registers[256];
		uint16_t call_stack[255];//because the "call" can not be more than 255
		if (get_file_name(true)) {
			File exe_file = SD.open(input_str, FILE_READ);
			if (exe_file) {
				while (exe_file.available()) {
					for (i = 0; i < 4; ++i)
						data[i] = exe_file.available() ? exe_file.read() : 0;
					exe_code = exe(exe_file, work_file, registers, data, delta, call, call_stack);
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
	LCD.setFont(lcd_font);
	LCD.setColor(VGA_LIME);
	analogWrite(pin_BL, BL);
	keyboard.begin(ps2key_data_pin, ps2key_int_pin);
	Wire.begin();
	cursorYbar = LCD.getDisplayYSize() - LCD.getFontYsize();
	ClrScr();
	Serial.begin(9600);
	timer_init_ISR_1Hz(TIMER_DEFAULT);
	echo("MicConOS 1.0b ");
	setc16(LCD.getDisplayXSize() / LCD.getFontXsize());
	setc8('x');
	setc16(LCD.getDisplayYSize() / LCD.getFontYsize());
	setc8('\n');
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
