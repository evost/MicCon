/*
 * MicConAsm - translator for MicConOS (virtual machine for Arduino Due and Mega)
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
#include <iostream>
#include <string>
#include <fstream>
#include <map>
#include <algorithm>
#include <regex>
using namespace std;
uint8_t current_var = 0;
uint16_t current_str = 0;
uint16_t p_size = 0;
ofstream outf;
map <string, uint8_t> var_names;
map <string, uint16_t> label_names;
map <string, uint8_t> coms = {
{ "nop", 0 },
{ "ret", 1 },
{ "clf", 2 },
{ "end", 3 },
{ "clr", 4 },
{ "xch", 5 },
{ "add", 6 },
{ "sub", 7 },
{ "mul", 8 },
{ "div", 9 },
{ "mod", 10 },
{ "pow", 11 },
{ "mov", 12 },
{ "orl", 13 },
{ "and", 14 },
{ "not", 15 },
{ "xor", 16 },
{ "neg", 17 },
{ "rol", 18 },
{ "ror", 19 },
{ "shl", 20 },
{ "shr", 21 },
{ "rnd", 22 },
{ "rci", 23 },
{ "rcc", 24 },
{ "wci", 25 },
{ "wcc", 26 },
{ "rfi", 27 },
{ "rfc", 28 },
{ "wfi", 29 },
{ "wfc", 30 },
{ "rdd", 31 },
{ "rda", 32 },
{ "wfd", 33 },
{ "wfa", 34 },
{ "wsd", 35 },
{ "wsa", 36 },
{ "stx", 37 },
{ "sty", 38 },
{ "stc", 39 },
{ "pni", 40 },
{ "pno", 41 },
{ "wsr", 42 },
{ "rsr", 43 },
{ "inc", 44 },
{ "dec", 45 },
{ "ofr", 46 },
{ "ofw", 47 },
{ "ofa", 48 },
{ "mkd", 49 },
{ "rmd", 50 },
{ "rmf", 51 },
{ "ife", 52 },
{ "ifn", 53 },
{ "ifb", 54 },
{ "ifs", 55 },
{ "jmp", 56 },
{ "cal", 57 },
{ "lab", 58 },
{ "prc", 59 },
{ "dat", 60 }
};
map <uint8_t, string> errors = {
{0, "UNKNOWN COMMAND"},
{1, "BAD NEW VARIABLE"},
{2, "VARIABLES OVERFLOW"},
{3, "BAD NEW LABEL"},
{4, "UNNECESSARY ARGUMENT"},
{5, "PROGRAM SIZE OVERFLOW"},
{6, "UNKNOWN SYMBOL"},
{7, "UNKNOWN VARIABLE"},
{8, "BAD VARIABLE"},
{9, "UNKNOWN LABEL"}
};
bool is_number(string s) {
	if (s.length() == 0)
		return false;
	uint8_t i = 0;
	if (s[0] == '-') {
		i++;
		if (s.length() == 1)
			return false;
	}
	for (; i < s.length(); i++)
		if (!isdigit(s[i]))
			return false;
	return true;
}
void push_data(int8_t data) {
	outf.put(data);
	p_size++;
}
uint16_t get_lab_num(string s) {
	if (label_names.find(s) == label_names.end() || s.length() == 0)
		throw 9;
	return label_names[s];
}
uint8_t get_var_num(string s) {
	string var = "", index = "";
	for (uint8_t i = 0; i < s.length(); i++)
		if (s[i] != '[')
			var += s[i];
		else
			for (i++; i < s.length(); i++)
				if (s[i] != ']')
					index += s[i];
				else
					if (i + 1 != s.length() || !is_number(index))
						throw 8;
	if (var_names.find(var) == var_names.end())
		throw 7;
	else
		if (index.length() > 0)
			return var_names[var] + stoi(index);
		else
			return var_names[var];
}
int8_t bigbits(int16_t i) {
	i = i >> 8;
	return (int8_t)i;
}
int8_t litbits(int16_t i) {
	i = i << 8;
	i = i >> 8;
	return (int8_t)i;
}
void parse(string s) {
	current_str++;
	transform(s.begin(), s.end(), s.begin(), ::tolower);
	s = regex_replace(s, regex("^ +| +$|( ) +"), "$1");
	if (s.length() > 0) {
		if (s[0] != ';') {
			string op = "", arg1, arg2;
			for (uint8_t i = 0; i < s.length(); i++)
				if (s[i] != ';')
					break;
				else if (!isspace(s[i]) && !isdigit(s[i]) && !isalpha(s[i]) && s[i] != '_' && s[i] != '[' && s[i] != ']' && s[i] != '-')
					throw 6;
			uint8_t i = 0;
			for (; i < s.length() && !isspace(s[i]); i++)
				op += s[i];
			for (i++; i < s.length() && isspace(s[i]); i++) {};
			for (; i < s.length() && !isspace(s[i]); i++)
				arg1 += s[i];
			for (i++; i < s.length() && isspace(s[i]); i++) {};
			for (; i < s.length() && !isspace(s[i]); i++)
				arg2 += s[i];
			if (coms.find(op) != coms.end()) {
				if (coms[op] <= coms["clr"]) {
					if (arg1 != "" || arg2 != "")
						throw 4;
					push_data(coms[op]);
				}
				else if (coms[op] <= coms["xch"]) {
					push_data(coms[op] + 128);
					push_data(get_var_num(arg1));
					push_data(get_var_num(arg2));
				}
				else if (coms[op] <= coms["wsa"]) {
					if (is_number(arg2)) {
						push_data(coms[op]);
						push_data(get_var_num(arg1));
						push_data(bigbits(stoi(arg2)));
						push_data(litbits(stoi(arg2)));
					}
					else {
						push_data(coms[op] + 128);
						push_data(get_var_num(arg1));
						push_data(get_var_num(arg2));
					}
				}
				else if (coms[op] <= coms["wsr"]) {
					if (arg2 != "")
						throw 4;
					if (is_number(arg1)) {
						push_data(coms[op]);
						push_data(bigbits(stoi(arg1)));
						push_data(litbits(stoi(arg1)));
					}
					else {
						push_data(coms[op] + 128);
						push_data(get_var_num(arg1));
					}
				}
				else if (coms[op] <= coms["rmf"]) {
					if (arg2.length() != 0)
						throw 4;
					push_data(coms[op]);
					push_data(get_var_num(arg1));
				}
				else if (coms[op] <= coms["ifs"]) {
					push_data(coms[op]);
					push_data(get_var_num(arg1));
					push_data(bigbits(get_lab_num(arg2)));
					push_data(litbits(get_lab_num(arg2)));
				}
				else if (coms[op] <= coms["cal"]) {
					if (arg2.length() != 0)
						throw 4;
					push_data(coms[op]);
					push_data(bigbits(get_lab_num(arg1)));
					push_data(litbits(get_lab_num(arg1)));
				}
				else if (coms[op] <= coms["prc"]) {
					if (p_size >= 65536)
						throw 5;
					if (!isalpha(arg1[0]) || label_names.find(arg1) != label_names.end() || label_names.size() == 4096)//why 4096?
						throw 3;
					else label_names[arg1] = p_size;
				}
				else if (coms[op] <= coms["dat"]) {
					if (current_var + stoi(arg2) > 255)
						throw 2;
					if (!isalpha(arg1[0]) || !is_number(arg2) || var_names.find(arg1) != var_names.end())
						throw 1;
					var_names[arg1] = current_var;
					current_var += stoi(arg2);
				}
				cout << current_str << ": " << op << ' ' << arg1 << ' ' << arg2 << ' ' << '\n';
			}
			else
				throw 0;
		}
	}
}
int main(int argc, char *argv[]) {
	if (argc == 1)
		cout << "PLEASE DROP SOURCE FILE TO THIS EXECUTABLE";
	else {
		string buff = "", outname, name = argv[1];
		try {
			ifstream inpf(name);
			outname = name;
			outname[outname.length() - 3] = 'b';
			outname[outname.length() - 2] = 'i';
			outname[outname.length() - 1] = 'n';
			outf = ofstream(outname, ios::out | ios::binary);
			while (getline(inpf, buff))
				parse(buff);
			inpf.close();
			outf.close();
			cout << "DONE " << p_size << " BYTES, " << to_string(current_str) << " STRINGS, IN " << clock() / 1000.0 << " S" << '\n';
		}
		catch (int a) {
			if (a < errors.size())
				cout << errors[a] << ". LINE " << to_string(current_str) << ": " << buff << '\n';
			else
				cout << a << ". LINE " << to_string(current_str) << ": " << buff << '\n';
		}
		catch (...) {
			cout << "UNKNOWN ERROR. LINE " << to_string(current_str) << ": " << buff << '\n';
		}
	}
	cin.get();
	return 0;
}
