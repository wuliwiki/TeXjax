#pragma once
#include "global.h"
#include <sstream>
#include <fstream>
#include <locale>
#include <codecvt>
#ifdef _MSC_VER
#include <Windows.h> // for console unicode output
#endif
#include "string.h"
#include "utfcpp/utf8.h"

#ifdef _MSC_VER
#define SLS_USE_UTFCPP
#endif

namespace slisc {

using std::stringstream;

// set windows console to display utf-8
#ifdef _MSC_VER
struct set_windows_console_utf8 {
	set_windows_console_utf8() {
		SetConsoleOutputCP(65001); // code page 65001 is UTF-8
	}
};
// in case of ODR error, put this in main function;
inline set_windows_console_utf8 yes_set_windows_console_utf8;
#endif

// write Str to file
inline void write_file(Str_I str, Str_I name)
{
	ofstream fout(name, std::ios::binary);
	fout << str;
	fout.close();
}

#ifdef SLS_USE_UTFCPP
// convert from UTF-8 Str to UTF-32 Str32
inline void utf8to32(Str32_O str32, Str_I str)
{
	str32.clear();
	utf8::utf8to32(str.begin(), str.end(), back_inserter(str32));
}

// convert from UTF-32 Str32 to UTF-8 Str
inline void utf32to8(Str_O str, Str32_I str32)
{
	str.clear();
	utf8::utf32to8(str32.begin(), str32.end(), back_inserter(str));
}
#else
// convert from UTF-8 Str to UTF-32 Str32
inline void UTF8_to_UTF32(Str32_O str32, Str_I str)
{
	std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> myconv;
	str32 = myconv.from_bytes(str);
}

// convert from UTF-32 Str32 to UTF-8 Str
inline void UTF32_to_UTF8(Str_O str, Str32_I str32)
{
	std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> myconv;
	str = myconv.to_bytes(str32);
}
#endif

// convert from UTF-8 Str32 to UTF-32 Str
inline Str32 utf8to32(Str_I str)
{
	Str32 str32;
	utf8to32(str32, str);
	return str32;
}

// convert from UTF-32 Str32 to UTF-8 Str
inline Str utf32to8(Str32_I str32)
{
	Str str;
	utf32to8(str, str32);
	return str;
}

inline void write_file(Str_I str, Str32_I name)
{
	ofstream fout(utf32to8(name), std::ios::binary);
	fout << str;
	fout.close();
}

inline void read_file(Str_O str, Str32_I fname)
{
	read_file(str, fname);
}

// display Str32
inline std::ostream &operator<<(std::ostream &out, Str32_I str32)
{
	Str str;
	utf32to8(str, str32);
	out << str;
	return out;
}

// operator+ that converts Str to Str32
Str32 operator+(Str32_I str32, Str_I str)
{
	return str32 + utf8to32(str);
}

Str32 operator+(Str_I str, Str32_I str32)
{
	return utf8to32(str) + str32;
}

template <class T>
inline void num2str(Str32_O str, const T &num)
{
	Str str0;
	num2str(str0, num);
	utf8to32(str, str0);
}

 // same as str.insert(), but return one index after key after insertion
inline Long insert(Str32_IO str, Str32_I key, Long start)
{
	str.insert(start, key);
	return start + key.size();
}

// replace every U"\r\n" with U"\n"
inline Long CRLF_to_LF(Str32_IO str)
{
	Long ind0{}, N{};
	while (true) {
		ind0 = str.find(U"\r\n", ind0);
		if (ind0 < 0) return N;
		str.erase(ind0, 1);
	}
}

// Fiind the next appearance of
// output the ikey of key[ikey] found
// return the first index of key[ikey], return -1 if nothing found
// TODO: temporary algorithm, can be more efficient
Long FindMultiple(Long_O ikey, Str32_I str, const vector<Str32> &key, Long_I start)
{
	Long i{}, ind0{}, Nkey{}, imin;
	Nkey = key.size();
	imin = str.size();
	for (i = 0; i < Nkey; ++i) {
 		ind0 = str.find(key[i], start);
 		if (ind0 >= start && ind0 < imin) {
 			imin = ind0; ikey = i;
 		}
	}
	if (imin == str.size()) imin = -1;
	return imin;
}

// see if a key appears followed only by only white space or '\n'
// return the index after the key found, return -1 if nothing found.
Long ExpectKey(Str32_I str, Str32_I key, Long_I start)
{
	Long ind = start;
	Long ind0 = 0;
	Long L = str.size();
	Long L0 = key.size();
	Char32 c0, c;
	while (true) {
 		c0 = key.at(ind0);
 		c = str.at(ind);
 		if (c == c0) {
 			++ind0;
 			if (ind0 == L0)
 				return ind + 1;
 		}
 		else if (c != U' ' && c != U'\n')
 			return -1;
 		++ind;
 		if (ind == L)
 			return -1;
	}
}

// trim all occurance of key on the left
Long TrimLeft(Str32_IO str, Char32_I key)
{
	Long ind = str.find_first_not_of(key);
	if (ind > 0) {
 		str.erase(0, ind);
	}
	return ind;
}

// trim all occurance of key on the right
Long TrimRight(Str32_IO str, Char32_I key)
{
	Long ind = str.find_last_not_of(key);
	if (ind < str.size() - 1) {
 		str.erase(ind + 1);
	}
	return ind;
}

// replace all occurance of "key" with "new_str"
// return the number of keys replaced
Long replace(Str32_IO str, Str32_I key, Str32_I new_str)
{
	Long ind0 = 0;
	Long Nkey = key.size();
	Long N = 0;
	while (true) {
 		ind0 = str.find(key, ind0);
		if (ind0 < 0) break;
		str.erase(ind0, Nkey);
		str.insert(ind0, new_str);
		++N; ++ind0;
	}
	return N;
}

// find next character that is not a space
// output single character c, return the position of the c
// return -1 if not found
// TODO: replace this with basic_string::find_first_not_of
Long NextNoSpace(Str32_O c, Str32_I str, Long start)
{
	for (Long i = start; i < str.size(); ++i) {
		c = str.at(i);
		if (c == U" ")
			continue;
		else
			return i;
	}
	return -1;
}

// reverse version of Expect key
// return the previous index of the key found, return -2 if nothing found.
Long ExpectKeyReverse(Str32_I str, Str32_I key, Long start)
{
	Long ind = start;
	Long L = str.size();
	Long L0 = key.size();
	Long ind0 = L0 - 1;
	Char32 c0, c;
	while (true) {
		c0 = key.at(ind0);
		c = str.at(ind);
		if (c == c0) {
			--ind0;
			if (ind0 < 0)
				return ind - 1;
		}
		else if (c != ' ' && c != '\n')
			return -2;
		--ind;
		if (ind < 0)
			return -2;
	}
}

// Find the next number
// return -1 if not found
Long FindNum(Str32_I str, Long start)
{
	Long i{}, end = str.size() - 1;
	unsigned char c;
	for (i = start; i <= end; ++i) {
		c = str.at(i);
		if (c >= '0' && c <= '9')
			return i;
	}
}

// get non-negative integer from string
// return the index after the last digit, return -1 if failed
// str[start] must be a number
Long str2int(Long_O num, Str32_I str, Long start)
{
	Long i{};
	Char32 c;
	c = str.at(start);
	if (c < '0' || c > '9') {
		SLS_ERR("not a number!"); return -1;  // break point here
	}
	num = c - '0';
	for (i = start + 1; i < str.size(); ++i) {
		c = str.at(i);
		if (c >= '0' && c <= '9')
			num = 10 * num + (Long)(c - '0');
		else
			return i;
	}
	return i;
}

// get non-negative double from string
// return the index after the last digit, return -1 if failed
// str[start] must be a number
Long str2double(Doub& num, Str32_I str, Long start)
{
	Long ind0{}, num1{}, num2{};
	ind0 = str2int(num1, str, start);
	if (str.at(ind0) != '.') {
		num = (double)num1;
		return ind0;
	}
	ind0 = str2int(num2, str, ind0 + 1);
	num = num2;
	while (num >= 1)
		num /= 10;
	num += (double)num1;
	return ind0;
}

// delete any following ' ' or '\n' characters starting from "start" (not including "start")
// return the number of characters deleted
// when option = 'r' (right), will only delete ' ' or '\n' at "start" or to the right
// when option = 'l' (left), will only delete ' ' or '\n' at "start" or to the left
// when option = 'a' (all), will delete both direction
Long DeleteSpaceReturn(Str32& str, Long start, Char option = 'r')
{
	Long i{}, Nstr{}, left{ start }, right{ start };
	Nstr = str.size();
	if (str.at(start) != ' ' && str.at(start) != '\n')
		return 0;
	if (option == 'r' || option == 'a') {
		for (i = start + 1; i < Nstr; ++i) {
			if (str.at(i) == ' ' || str.at(i) == '\n')
				continue;
			else {
				right = i - 1; break;
			}
		}
	}
	if (option == 'l' || option == 'a') {
		for (i = start - 1; i >= 0; --i) {
			if (str.at(i) == ' ' || str.at(i) == '\n')
				continue;
			else
				left = i + 1; break;
		}
	}
	str.erase(left, right - left + 1);
	return right - left + 1;
}


// Pair right brace to left one (default)
// or () or [] or anying single character
// ind is inddex of left brace
// return index of right brace, -1 if failed
Long PairBraceR(Str32_I str, Long ind, Char32_I type = '{')
{
	Char32 left, right;
	if (type == '{' || type == '}') {
		left = '{'; right = '}';
	}
	else if (type == '(' || type == ')') {
		left = '('; right = ')';
	}
	else if (type == '[' || type == ']') {
		left = '['; right = ']';
	}
	else {// anything else
		left = type; right = type;
	}

	Char32 c, c0 = ' ';
	Long Nleft = 1;
	for (Long i = ind + 1; i < str.size(); i++)
	{
		c = str.at(i);
		if (c == left && c0 != '\\')
			++Nleft;
		else if (c == right && c0 != '\\')
		{
			--Nleft;
			if (Nleft == 0)
				return i;
		}
		c0 = c;
	}
	return -1;
}

// match braces
// return -1 means failure, otherwise return number of {} paired
// output ind_left, ind_right, ind_RmatchL
Long MatchBraces(vector<Long>& ind_left, vector<Long>& ind_right,
	vector<Long>& ind_RmatchL, Str32_I str, Long start, Long end)
{
	ind_left.resize(0); ind_right.resize(0); ind_RmatchL.resize(0);
	Char32 c, c_last = ' ';
	Bool continuous{ false };
	Long Nleft = 0, Nright = 0;
	vector<Bool> Lmatched;
	Bool matched;
	for (Long i = start; i <= end; ++i)
	{
		c = str.at(i);
		if (c == '{' && c_last != '\\')
		{
			++Nleft;
			ind_left.push_back(i);
			Lmatched.push_back(false);
		}
		else if (c == '}' && c_last != '\\')
		{
			++Nright;
			ind_right.push_back(i);
			matched = false;
			for (Long j = Nleft - 1; j >= 0; --j)
				if (!Lmatched[j])
				{
					ind_RmatchL.push_back(j);
					Lmatched[j] = true;
					matched = true;
					break;
				}
			if (!matched)
				return -1; // unbalanced braces
		}
		c_last = c;
	}
	if (Nleft != Nright)
		return -1;
	return Nleft;
}

} // namespace slisc