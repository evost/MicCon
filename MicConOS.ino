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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
#include <SD.h>
#include <UTFT.h>
#include <PS2Keyboard.h>
#include <Wire.h>
#include <RTClib.h>
#include <MemoryFree.h>
char exe_file_name[256];//current executable, 255 for name and path, and 1 for null terminator
char work_file_name[256];//file with which the program works
int8_t exe_code;//code returned by exe()
int8_t delta = 0;//the number of bytes to jump after executing the instruction
int8_t data[4];//a place for pre-loading from a file operation code and the three following bytes
uint8_t BL = 64;//brightness of the backlight
int16_t registers[256];
uint16_t call_stack[255];//because the "call" can not be more than 255
uint8_t call = 0;//number of the next entry in the call stack
uint16_t cursorY = 0;
uint16_t cursorX = 0;
char sTime[3];
File exe_file;
File work_file;
PS2Keyboard keyboard;
UTFT LCD(ILI9486, 38, 39, 40, 41);
RTC_DS3231 rtc;
extern uint8_t BigFont[];
const unsigned int VGA_COLORS[16] = {VGA_BLACK, VGA_WHITE, VGA_RED, VGA_GREEN, VGA_BLUE, VGA_SILVER, VGA_GRAY, VGA_MAROON, VGA_YELLOW, VGA_OLIVE, VGA_LIME, VGA_AQUA, VGA_TEAL, VGA_NAVY, VGA_FUCHSIA, VGA_PURPLE};
int16_t dbytes(const int8_t &a, const int8_t &b) {
  return (((int16_t)a) << 8) + b;
}
void setc8(const char &k) {
  char c[2] = {k, 0};
  if ((cursorX > LCD.getDisplayXSize() - LCD.getFontXsize()) || k == '\n') {
    cursorX = 0;
    cursorY += LCD.getFontYsize();
    if (cursorY > LCD.getDisplayYSize() -  LCD.getFontYsize()) {
      LCD.clrScr();
      cursorY = 0;
    }
  } else if (k >= 32 && k != 127) {
    LCD.print(c, cursorX, cursorY);
    cursorX += LCD.getFontXsize();
  }
}
inline void echo(const char* s, const uint8_t len) {
  for (uint8_t i = 0; i < len; i++)
  if (s[i] == 0)
    break;
  else
    setc8(s[i]);
}
inline void echoln(const char* s, const uint8_t len) {
  echo(s, len);
  setc8('\n');
}
void setc16(const int16_t &k) {
  char s[6];
  itoa(k, s, 10);
  echo(s, strlen(s));
}
inline void setf8(const int8_t &u) {
  work_file.write(u);
}
inline void setf16(const int16_t &u) {
  setf8(u >> 8);
  setf8((u << 8) >> 8);
}
void getstr(char* str, const uint8_t &len = 255) { //len - length of the string without the null terminator
  echo("> ", 2);
  uint8_t i = 0,  buf;
  while (buf != PS2_ENTER) {
    while (!keyboard.available()) delay(16);
    buf = keyboard.read();
    if (buf >= 32 && buf != PS2_DELETE && i < len) {
      str[i++] = buf;
      setc8(buf);
    } else if (buf == PS2_DELETE) {
      if (i > 0) {
        str[i] = ' ';
        i--;
        cursorX -= LCD.getFontXsize();
        setc8(' ');
        cursorX -= LCD.getFontXsize();
      }
    } else {
      str[i] = 0;
      cursorY += LCD.getFontYsize();
      cursorX = 0;
      if (cursorY > LCD.getDisplayYSize() -  LCD.getFontYsize()) {
        LCD.clrScr();
        cursorY = 0;
      }
      break;
    }
  }
  while (keyboard.available())
    keyboard.read();
}
void getc16(int16_t &u) {
  char s[6];
  getstr(s, 5);
  u = (int16_t)atoi(s);
}
void getc8(int16_t &u) {
  char s[2];
  getstr(s, 1);
  u = s[0];
}
inline void getf8(int16_t u) {
  if (work_file.available())
    u = work_file.read();
  else
    u = 0;
}
void getf16(int16_t &u) {//if there is one byte left in the file, it will be returned as the big one (xxxxxxxx00000000)!
  int16_t a, b;
  getf8(a);
  getf8(b);
  u = dbytes(a, b);
}
uint8_t exe() {
  int8_t opcode = data[0];
  int16_t swpbuf;
  if (opcode > 128)
    opcode -= 128;
  delta = 0;
  switch (opcode) {
    case 0:
      delta = 1;
      break;
    case 1:
      if (call == 0)
        return off_by_one;
      call--;
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
      LCD.clrScr();
      cursorX = 0;
      cursorY = 0;
      break;
    case 5:
      swpbuf = registers[data[1]];
      registers[data[1]] = registers[data[2]];
      registers[data[2]] = swpbuf;
      delta = 3;
      break;
    case 6:
      if (data[0] >= 128) {
        registers[data[1]] += registers[data[2]];
        delta = 3;
      } else {
        registers[data[1]] += dbytes(data[2], data[3]);
        delta = 4;
      }
      break;
    case 7:
      if (data[0] >= 128) {
        registers[data[1]] -= registers[data[2]];
        delta = 3;
      } else {
        registers[data[1]] -= dbytes(data[2], data[3]);
        delta = 4;
      }
      break;
    case 8:
      if (data[0] >= 128) {
        registers[data[1]] *= registers[data[2]];
        delta = 3;
      } else {
        registers[data[1]] *= dbytes(data[2], data[3]);
        delta = 4;
      }
      break;
    case 9:
      if (data[0] >= 128) {
        if (registers[data[2]] == 0)
          return division_by_zero;
        registers[data[1]] /= registers[data[2]];
        delta = 3;
      } else {
        if (dbytes(data[2], data[3] == 0))
          return division_by_zero;
        registers[data[1]] /= dbytes(data[2], data[3]);
        delta = 4;
      }
      break;
    case 10:
      if (data[0] >= 128) {
        if (registers[data[2]] == 0)
          return division_by_zero;
        registers[data[1]] %= registers[data[2]];
        delta = 3;
      } else {
        if (dbytes(data[2], data[3] == 0))
          return division_by_zero;
        registers[data[1]] %= dbytes(data[2], data[3]);
        delta = 4;
      }
      break;
    case 11:
      if (data[0] >= 128) {
        registers[data[1]] = pow(registers[data[1]], registers[data[2]]);
        delta = 3;
      } else {
        registers[data[1]] = pow(registers[data[1]], dbytes(data[2], data[3]));
        delta = 4;
      }
      break;
    case 12:
      if (data[0] >= 128) {
        registers[data[1]] = registers[data[2]];
        delta = 3;
      } else {
        registers[data[1]] = dbytes(data[2], data[3]);
        delta = 4;
      }
      break;
    case 13:
      if (data[0] >= 128) {
        registers[data[1]] |= registers[data[2]];
        delta = 3;
      } else {
        registers[data[1]] |= dbytes(data[2], data[3]);
        delta = 4;
      }
      break;
    case 14:
      if (data[0] >= 128) {
        registers[data[1]] &= registers[data[2]];
        delta = 3;
      } else {
        registers[data[1]] &= dbytes(data[2], data[3]);
        delta = 4;
      }
      break;
    case 15:
      if (data[0] >= 128) {
        registers[data[1]] = ~registers[data[2]];
        delta = 3;
      } else {
        registers[data[1]] = ~dbytes(data[2], data[3]);
        delta = 4;
      }
      break;
    case 16:
      if (data[0] >= 128) {
        registers[data[1]] ^= registers[data[2]];
        delta = 3;
      } else {
        registers[data[1]] ^= dbytes(data[2], data[3]);
        delta = 4;
      }
      break;
    case 17:
      if (data[0] >= 128) {
        registers[data[1]] = -registers[data[2]];
        delta = 3;
      } else {
        registers[data[1]] = -dbytes(data[2], data[3]);
        delta = 4;
      }
      break;
    case 18:
      if (data[0] >= 128) {
        registers[data[1]] = (registers[data[1]] << (registers[data[2]] % 16)) + (registers[data[1]] >> (16 - registers[data[2]] % 16));
        delta = 3;
      } else {
        registers[data[1]] = (registers[data[1]] << (dbytes(data[2], data[3]) % 16)) + (registers[data[1]] >> (16 - dbytes(data[2], data[3]) % 16));
        delta = 4;
      }
      break;
    case 19:
      if (data[0] >= 128) {
        registers[data[1]] = (registers[data[1]] >> (registers[data[2]] % 16)) + (registers[data[1]] << (16 - registers[data[2]] % 16));
        delta = 3;
      } else {
        registers[data[1]] = (registers[data[1]] >> (dbytes(data[2], data[3]) % 16)) + (registers[data[1]] << (16 - dbytes(data[2], data[3]) % 16));
        delta = 4;
      }
      break;
    case 20:
      if (data[0] >= 128) {
        registers[data[1]] = registers[data[1]] << registers[data[2]];
        delta = 3;
      } else {
        registers[data[1]] = registers[data[1]] << dbytes(data[2], data[3]);
        delta = 4;
      }
      break;
    case 21:
      if (data[0] >= 128) {
        registers[data[1]] = registers[data[1]] >> registers[data[2]];
        delta = 3;
      } else {
        registers[data[1]] = registers[data[1]] >> dbytes(data[2], data[3]);
        delta = 4;
      }
      break;
    case 22:
      if (data[0] >= 128) {
        registers[data[1]] = random(registers[data[2]]);
        delta = 3;
      } else {
        registers[data[1]] = random(dbytes(data[2], data[3]));
        delta = 4;
      }
      break;
    case 23:
      if (data[0] >= 128) {
        for (int i = 0; i < registers[data[2]]; i++)
          getc16(registers[data[1] + i]);
        delta = 3;
      } else {
        for (int i = 0; i < dbytes(data[2], data[3]); i++)
          getc16(registers[data[1] + i]);
        delta = 4;
      }
      break;
    case 24:
      if (data[0] >= 128) {
        for (int i = 0; i < registers[data[2]]; i++)
          getc8(registers[data[1] + i]);
        delta = 3;
      } else {
        for (int i = 0; i < dbytes(data[2], data[3]); i++)
          getc8(registers[data[1] + i]);
        delta = 4;
      }
      break;
    case 25:
      if (data[0] >= 128) {
        for (int i = 0; i < registers[data[2]]; i++)
          setc16(registers[data[1] + i]);
        delta = 3;
      } else {
        for (int i = 0; i < dbytes(data[2], data[3]); i++)
          setc16(registers[data[1] + i]);
        delta = 4;
      }
      break;
    case 26:
      if (data[0] >= 128) {
        for (int i = 0; i < registers[data[2]]; i++)
          setc8(registers[data[1] + i]);
        delta = 3;
      } else {
        for (int i = 0; i < dbytes(data[2], data[3]); i++)
          setc8(registers[data[1] + i]);
        delta = 4;
      }
      break;
    case 27:
      if (work_file.available()) {
        if (data[0] >= 128) {
          for (int i = 0; i < registers[data[2]]; i++)
            getf16(registers[data[1] + i]);
          delta = 3;
        } else {
          for (int i = 0; i < dbytes(data[2], data[3]); i++)
            getf16(registers[data[1] + i]);
          delta = 4;
        }
      } else
        return end_of_file;
      break;
    case 28:
      if (work_file.available()) {
        if (data[0] >= 128) {
          for (int i = 0; i < registers[data[2]]; i++)
            getf8(registers[data[1] + i]);
          delta = 3;
        } else {
          for (int i = 0; i < dbytes(data[2], data[3]); i++)
            getf8(registers[data[1] + i]);
          delta = 4;
        }
      } else
        return end_of_file;
      break;
    case 29:
      if (data[0] >= 128) {
        for (int i = 0; i < registers[data[2]]; i++)
          setf16(registers[data[1] + i]);
        delta = 3;
      } else {
        for (int i = 0; i < dbytes(data[2], data[3]); i++)
          setf16(registers[data[1] + i]);
        delta = 4;
      }
      break;
    case 30:
      if (data[0] >= 128) {
        for (int i = 0; i < registers[data[2]]; i++)
          setf8(registers[data[1] + i]);
        delta = 3;
      } else {
        for (int i = 0; i < dbytes(data[2], data[3]); i++)
          setf8(registers[data[1] + i]);
        delta = 4;
      }
      break;
    case 31:
      if (data[0] >= 128) {
        registers[data[1]] = digitalRead(registers[data[2]]);
        delta = 3;
      } else {
        registers[data[1]] = digitalRead(dbytes(data[2], data[3]));
        delta = 4;
      }
      break;
    case 32:
      if (data[0] >= 128) {
        registers[data[1]] = analogRead(registers[data[2]]);
        delta = 3;
      } else {
        registers[data[1]] = analogRead(dbytes(data[2], data[3]));
        delta = 4;
      }
      break;
    case 33:
      if (data[0] >= 128) {
        digitalWrite(registers[data[1]], registers[data[2]]);
        delta = 3;
      } else {
        digitalWrite(registers[data[1]], dbytes(data[2], data[3]));
        delta = 4;
      }
      break;
    case 34:
      if (data[0] >= 128) {
        analogWrite(registers[data[1]], registers[data[2]]);
        delta = 3;
      } else {
        analogWrite(registers[data[1]], dbytes(data[2], data[3]));
        delta = 4;
      }
      break;
    case 35:
      if (data[0] >= 128) {
        digitalWrite(registers[data[2]], registers[data[1]]);
        delta = 3;
      } else {
        digitalWrite(dbytes(data[2], data[3]), registers[data[1]]);
        delta = 4;
      }
      break;
    case 36:
      if (data[0] >= 128) {
        analogWrite(registers[data[2]], registers[data[1]]);
        delta = 3;
      } else {
        analogWrite(dbytes(data[2], data[3]), registers[data[1]]);
        delta = 4;
      }
      break;
    case 37:
      if (data[0] >= 128) {
        cursorX = registers[data[1]] * LCD.getFontXsize();
        delta = 3;
      } else {
        cursorX = dbytes(data[1], data[2]) * LCD.getFontXsize();
        delta = 4;
      }
      break;
    case 38:
      if (data[0] >= 128) {
        cursorY = registers[data[1]] * LCD.getFontYsize();
        delta = 3;
      } else {
        cursorY = dbytes(data[1], data[2]) * LCD.getFontYsize();
        delta = 4;
      }
      break;
    case 39:
      if (data[0] >= 128) {
        LCD.setBackColor(VGA_COLORS[(registers[data[1]] >> 8) % 16]);
        LCD.setColor(VGA_COLORS[((registers[data[1]] << 8) >> 8) % 16]);
        delta = 3;
      } else {
        LCD.setBackColor(VGA_COLORS[(dbytes(data[1], data[2]) >> 8) % 16]);
        LCD.setColor(VGA_COLORS[((dbytes(data[1], data[2]) << 8) >> 8) % 16]);
        delta = 4;
      }
      break;
    case 40:
      if (data[0] >= 128) {
        pinMode(registers[data[1]], OUTPUT);
        delta = 3;
      } else {
        pinMode(dbytes(data[1], data[2]), OUTPUT);
        delta = 4;
      }
      break;
    case 41:
      if (data[0] >= 128) {
        pinMode(registers[data[1]], INPUT);
        delta = 3;
      } else {
        pinMode(dbytes(data[1], data[2]), INPUT);
        delta = 4;
      }
      break;
    case 42:
      if (data[0] >= 128) {
        Serial.write(registers[data[1]]);
        delta = 3;
      } else {
        Serial.write(dbytes(data[1], data[2]));
        delta = 4;
      }
      break;
    case 43:
      registers[data[1]] = Serial.read();
      delta = 2;
      break;
    case 44:
      registers[data[1]] = registers[data[1]] + 1;
      delta = 2;
      break;
    case 45:
      registers[data[1]] = registers[data[1]] - 1;
      delta = 2;
      break;
    case 46:
      for (int8_t i = 0; i < 256; i++) {
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
    case 47:
      for (int8_t i = 0; i < 256; i++) {
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
    case 48:
      for (int8_t i = 0; i < 256; i++) {
        work_file_name[i] = registers[data[1] + i];
        if (registers[data[1] + i] == 0)
          break;
      }
      work_file = SD.open(work_file_name, FILE_WRITE);
      if (!work_file)
        return opening_error;
      delta = 2;
      break;
    case 49:
      for (int8_t i = 0; i < 256; i++) {
        work_file_name[i] = registers[data[1] + i];
        if (registers[data[1] + i] == 0)
          break;
      }
      SD.mkdir(work_file_name);
      break;
    case 50:
      for (int8_t i = 0; i < 256; i++) {
        work_file_name[i] = registers[data[1] + i];
        if (registers[data[1] + i] == 0)
          break;
      }
      SD.rmdir(work_file_name);
      break;
    case 51:
      for (int8_t i = 0; i < 256; i++) {
        work_file_name[i] = registers[data[1] + i];
        if (registers[data[1] + i] == 0)
          break;
      }
      SD.remove(work_file_name);
      break;
    case 52:
      if (registers[data[1]] == 0)
        exe_file.seek(dbytes(data[2], data[3]));
      else
        delta = 4;
      break;
    case 53:
      if (registers[data[1]] != 0)
        exe_file.seek(dbytes(data[2], data[3]));
      else
        delta = 4;
      break;
    case 54:
      if (registers[data[1]] > 0)
        exe_file.seek(dbytes(data[2], data[3]));
      else
        delta = 4;
      break;
    case 55:
      if (registers[data[1]] < 0)
        exe_file.seek(dbytes(data[2], data[3]));
      else
        delta = 4;
      break;
    case 56:
      exe_file.seek(dbytes(data[1], data[2]));
      break;
    case 57:
      if (call == 255)
        return stack_overflow;
      call_stack[call] = exe_file.position();
      exe_file.seek(dbytes(data[1], data[2]));
      call++;
      break;
    default:
      return data[0];
      break;
  }
  return exe_ok;
}
void printDirectory(File dir, int numTabs) {
  while (true) {
    File entry =  dir.openNextFile();
    if (! entry)
      break;
    for (int8_t i = 0; i < numTabs; i++)
      setc8('-');
    echo(entry.name(), strlen(entry.name()));
    if (entry.isDirectory()) {
      setc8('/');
      setc8('\n');
      printDirectory(entry, numTabs + 1);
    } else {
      echo("    ", 4);
      setc16(entry.size());
      setc8('\n');
    }
    entry.close();
  }
}
void nano() {
  File edit;
  char text_buf[128][16], t;
  for (uint8_t ti = 0; ti <= 127; ti++)
    for (uint8_t tk = 0; tk <= 15; tk++)
      text_buf[ti][tk] = 0;
  int8_t i = 0, k = 0, page = 0;//i-th string, k-th character
  if (SD.exists(exe_file_name)) {
    edit = SD.open(exe_file_name, FILE_READ);
    while (edit.available() && i <= 127) {
      text_buf[i][k] = edit.read();
      if (text_buf[i][k] == '\n' || text_buf[i][k] == 0) {
        text_buf[i][k] = 0;
        k = 0;
        i++;
      } else if (text_buf[i][k] == '\r')
        text_buf[i][k] = 0;
      else
        k++;
      if (k == 15) {
        text_buf[i][k] = 0;
        k = 0;
        i++;
      }
    }
    edit.close();
  }
  cursorX = 0;
  cursorY = LCD.getFontYsize();
  i = 0;
  k = 0;
  bool refresh = true;
  while (t != PS2_ESC) {
    if (k >= 15) {
      text_buf[page * 16 + i][15] = 0;
      k = 0;
      i++;
    }
    if (k < 0) {
      k = 0;
      i--;
    }
    if (i > 15 || i < 0 || refresh) {
      if (i < 0)
        page--;
      else if (i > 15)
        page++;
      if (!refresh) {
          k = 0;
          i = 0;
      }
      refresh = false;
      if (page > 15)
        page = 15;
      if (page < 0)
        page = 0;
      LCD.clrScr();
      cursorX = 0;
      cursorY = 0;
      echo("ESC-exit TAB-save PAGE=", 23);
      setc16(page);
      setc8('\n');
      setc8('\n');
      for (uint8_t x = 0; x <= 15; x++)
        for (uint8_t y = 0; y <= 15; y++)
          if (text_buf[page * 16 + x][y] == 0) {
            setc8('\n');
            y = 16;
          }
          else
            setc8(text_buf[page * 16 + x][y]);
    }
    cursorX = k * LCD.getFontXsize();
    cursorY = (i + 2) * LCD.getFontYsize();
    setc8('_');
    cursorX -= LCD.getFontXsize();
    while (!keyboard.available()) delay(16);
    t = keyboard.read();
    if (t == PS2_TAB) {
      SD.remove(exe_file_name);
      edit = SD.open(exe_file_name, FILE_WRITE);
      for (uint8_t ti = 0; ti <= 127; ti++)
        for (uint8_t tk = 0; tk <= 15; tk++)
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
      i++;
      if (text_buf[127][0] == 0) {
        for (int8_t ti = 127; ti >= i + 1; ti--)
          for (int8_t tk = 15; tk >= 0; tk--)
            text_buf[ti][tk] = text_buf[ti - 1][tk];
        for (int8_t tk = 0; tk < 15 - k; tk++) {
          text_buf[i][tk] = text_buf[i - 1][k + tk + 1];
          text_buf[i - 1][k + tk + 1] = 0;
        }
        for (int8_t tk = 15 - k; tk <= 15; tk++)
          text_buf[i][tk] = 0;
        refresh = true;
      }
      k = 0;
    } else if (t == PS2_DELETE){
      if (text_buf[page * 16 + i][0] == 0) {
        for (int8_t ti = i; ti <= 126; ti++)
          for (int8_t tk = 0; tk <= 15; tk++)
            text_buf[ti][tk] = text_buf[ti + 1][tk];
        for (int8_t tk = 0; tk <= 15; tk++)
          text_buf[127][tk] = 0;
        refresh = true;
      } else if (k > 0) {
        cursorX -= LCD.getFontXsize();
        for (int8_t tk = k; tk <= 15; tk++) {
          text_buf[i][tk - 1] = text_buf[i][tk];
          setc8(text_buf[i][tk]);
        }
        k--;
      }
    } else if (t == PS2_RIGHTARROW){
      setc8(text_buf[page * 16 + i][k]);
      if (text_buf[page * 16 + i][k] != 0) {
        k++;
      } else {
        k = 0;
        i++;
      }
    } else if (t == PS2_LEFTARROW){
      setc8(text_buf[page * 16 + i][k]);
      k--;
    } else if (t == PS2_DOWNARROW){
      setc8(text_buf[page * 16 + i][k]);
      i++;
      k = 0;
    } else if (t == PS2_UPARROW){
      setc8(text_buf[page * 16 + i][k]);
      i--;
      k = 0;
    } else if (t == PS2_PAGEDOWN){
      i = 16;
      k = 0;
    } else if (t == PS2_PAGEUP){
      i = -1;
      k = 0;
    } else if (text_buf[i][14] == 0) {
      for (int8_t tk = 14; tk > k; tk--)
        text_buf[i][tk] = text_buf[i][tk - 1];
      text_buf[page * 16 + i][k] = t;
      for (int8_t tk = k; tk < 15; tk++)
        setc8(text_buf[page * 16 + i][tk]);
      k++;
    }
  }
  LCD.clrScr();
  cursorX = 0;
  cursorY = 0;
}
void setup() {
  pinMode(pin_SD, OUTPUT);
  pinMode(pin_BL, OUTPUT);
  LCD.InitLCD();
  LCD.clrScr();
  LCD.setFont(BigFont);
  LCD.setColor(VGA_COLORS[10]);
  analogWrite(pin_BL, BL);
  echoln("MicConOS 0.8b", 13);
  if (!rtc.begin())
    echoln("RTC fail", 8);
  if (rtc.lostPower())
    rtc.adjust(DateTime(__DATE__, __TIME__));
  keyboard.begin(ps2key_data_pin, ps2key_int_pin);
  if (!SD.begin(pin_SD))
    echoln("SD fail", 7);
  #if (__AVR__)//freeMemory() does not work without these two lines on AVR-boards
  File root = SD.open("/");
  root.close();
  #endif
}
void loop() {
  echo("/", 1);
  getstr(exe_file_name);
  if (strlen(exe_file_name) > 0)
    if (!strcmp(exe_file_name, "ls")) {
      File root = SD.open("/");
      printDirectory(root, 0);
      root.close();
    } else if (!strcmp(exe_file_name, "uptime")) {
      setc16(millis() / 1000);
      echoln(" s", 2);
    } else if (!strcmp(exe_file_name, "time")) {
      itoa(rtc.now().hour(), sTime, 10);
      echo(sTime, 2);
      echo(":", 1);
      itoa(rtc.now().minute(), sTime, 10);
      echoln(sTime, 2);
    } else if (!strcmp(exe_file_name, "ram")) {
      setc16(freeMemory());
      echoln(" bytes free RAM", 15);
    } else if (!strcmp(exe_file_name, "nano")) {
      getstr(exe_file_name);
      nano();
    } else if (!strcmp(exe_file_name, "mkdir")) {
      getstr(exe_file_name);
      SD.mkdir(exe_file_name);
    } else if (!strcmp(exe_file_name, "rmdir")) {
      getstr(exe_file_name);
      SD.rmdir(exe_file_name);
    } else if (!strcmp(exe_file_name, "mk")) {
      File temp = SD.open(exe_file_name, FILE_WRITE);
      temp.close();
    } else if (!strcmp(exe_file_name, "rm")) {
      getstr(exe_file_name);
      SD.remove(exe_file_name);
    } else if (!strcmp(exe_file_name, "b+")) {
      BL += 16;
      analogWrite(pin_BL, BL);
    } else if (!strcmp(exe_file_name, "b-")) {
      BL -= 16;
      analogWrite(pin_BL, BL);
    } else if (!strcmp(exe_file_name, "color")) {
      getstr(exe_file_name);
      LCD.setColor(VGA_COLORS[atoi(exe_file_name)]);
      getstr(exe_file_name);
      LCD.setBackColor(VGA_COLORS[atoi(exe_file_name)]);
    } else if (!SD.exists(exe_file_name))
      echoln("File not exist or bad command", 29);
    else {
      exe_file = SD.open(exe_file_name, FILE_READ);
      if (exe_file) {
        while (exe_file.available()) {
          for (int8_t i = 0; i < 4; i++)
            if (exe_file.available())
              data[i] = exe_file.read();
            else
              data[i] = 0;
          exe_code = exe();
          if (exe_code == exe_end)
            break;
          else if (exe_code != exe_ok) {
            setc16(exe_code);
            echoln(" runtime error", 14);
            break;
          }
          if (delta != 0 && delta != 4)
            exe_file.seek(exe_file.position() - (4 - delta));
        }
        exe_file.close();
      } else {
        echo(exe_file_name, strlen(exe_file_name));
        echoln(" error opening", 14);
      }
    }
}
