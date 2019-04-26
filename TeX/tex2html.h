﻿#pragma once
#include "tex.h"

namespace slisc {

// ensure space around (CString)name
// return number of spaces added
// will only check equation env. and inline equations
Long EnsureSpace(Str32_I name, Str32_IO str, Long start, Long end)
{
	Long N{}, ind0{ start };
	while (true) {
		ind0 = str.find(name, ind0);
		if (ind0 < 0 || ind0 > end) break;
		if (ind0 == 0 || str.at(ind0 - 1) != U' ') {
			str.insert(ind0, U" "); ++ind0; ++N;
		}
		ind0 += name.size();
		if (ind0 == str.size() || str.at(ind0) != U' ') {
			str.insert(ind0, U" "); ++N;
		}
	}
	return N;
}

// ensure space around '<' and '>' in equation env. and $$ env
// return number of spaces added
// return -1 if failed
Long EqOmitTag(Str32_IO str)
{
	Long i{}, N{}, Nrange{};
	vector<Long> ind, indInline;
	FindEnv(ind, str, U"equation");
	FindInline(indInline, str);
	Nrange = CombineRange(ind, ind, indInline);
	if (Nrange < 0) return -1;
	for (i = 2 * Nrange - 2; i >= 0; i -= 2) {
		N += EnsureSpace(U"<", str, ind[i], ind[i + 1]);
		N += EnsureSpace(U">", str, ind[i], ind[i + 1]);
	}
	return N;
}

// detect \name* command and replace with \nameStar
// return number of commmands replaced
// must remove comments first
Long StarCommand(Str32_I name, Str32_IO str)
{
	Long N{}, ind0{}, ind1{};
	while (true) {
		ind0 = str.find(U"\\" + name, ind0 + 1);
		if (ind0 < 0) break;
		ind0 += name.size() + 1;
		ind1 = ExpectKey(str, U"*", ind0);
		if (ind1 < 0) continue;
		str.erase(ind0, ind1 - ind0); // delete * and spaces
		str.insert(ind0, U"Star");
		++N;
	}
	return N;
}

// detect \name{} with variable parameters
// replace \name{}{} with \nameTwo{}{} and \name{}{}{} with \nameThree{}{}{}
// replace \name[]{}{} with \nameTwo[]{}{} and \name[]{}{}{} with \nameThree[]{}{}{}
// return number of commmands replaced
// maxVars  = 2 or 3
// must remove comments first
Long VarCommand(Str32_I name, Str32_IO str, Int_I maxVars)
{
	Long i{}, N{}, Nvar{}, ind0{}, ind1{};

	while (true) {
		ind0 = str.find(U"\\" + name, ind0 + 1);
		if (ind0 < 0) break;
		ind0 += name.size() + 1;
		ind1 = ExpectKey(str, U"[", ind0);
		if (ind1 > 0)
			ind1 = PairBraceR(str, ind1 - 1, U']') + 1;
		else
			ind1 = ind0;
		ind1 = ExpectKey(str, U"{", ind1);
		if (ind1 < 0) continue;
		ind1 = PairBraceR(str, ind1 - 1);
		ind1 = ExpectKey(str, U"{", ind1 + 1);
		if (ind1 < 0) continue;
		ind1 = PairBraceR(str, ind1 - 1);
		if (maxVars == 2) { str.insert(ind0, U"Two"); ++N; continue; }
		ind1 = ExpectKey(str, U"{", ind1 + 1);
		if (ind1 < 0) {
			str.insert(ind0, U"Two"); ++N; continue;
		}
		str.insert(ind0, U"Three"); ++N; continue;
	}
	return N;
}

// detect \name() commands and replace with \nameRound{}
// also replace \name[]() with \nameRound[]{}
// must remove comments first
Long RoundSquareCommand(Str32_I name, Str32_IO str)
{
	Long N{}, ind0{}, ind1{}, ind2{}, ind3;
	while (true) {
		ind0 = str.find(U"\\" + name, ind0);
		if (ind0 < 0) break;
		ind0 += name.size() + 1;
		ind2 = ExpectKey(str, U"(", ind0);
		if (ind2 > 0) {
			--ind2;
			ind3 = PairBraceR(str, ind2, U')');
			str.erase(ind3, 1); str.insert(ind3, U"}");
			str.erase(ind2, 1); str.insert(ind2, U"{");
			str.insert(ind0, U"Round");
			++N;
		}
		else {
			ind2 = ExpectKey(str, U"[", ind0);
			if (ind2 < 0) break;
			--ind2;
			ind3 = PairBraceR(str, ind2, U']');
			str.erase(ind3, 1); str.insert(ind3, U"}");
			str.erase(ind2, 1); str.insert(ind2, U"{");
			str.insert(ind0, U"Square");
			++N;
		}
	}
	return N;
}

// detect \name() commands and replace with \nameRound{}
// also replace \name[]() with \nameRound[]{}
// must remove comments first
Long MathFunction(Str32_I name, Str32_IO str)
{
	Long N{}, ind0{}, ind1{}, ind2{}, ind3;
	while (true) {
		ind0 = str.find(U"\\" + name, ind0);
		if (ind0 < 0) break;
		ind0 += name.size() + 1;
		ind1 = ExpectKey(str, U"[", ind0);
		if (ind1 > 0) {
			--ind1;
			ind1 = PairBraceR(str, ind1, ']');
			ind2 = ind1 + 1;
		}
		else
			ind2 = ind0;
		ind2 = ExpectKey(str, U"(", ind2);
		if (ind2 < 0) continue;
		--ind2;
		ind3 = PairBraceR(str, ind2, ')');
		str.erase(ind3, 1); str.insert(ind3, U"}");
		str.erase(ind2, 1); str.insert(ind2, U"{");
		str.insert(ind0, U"Round");
		++N;
	}
	return N;
}

// deal with escape simbols in normal text
// str must be normal text
Long TextEscape(Str32_IO str)
{
	Long N{};
	N += replace(str, U"\\ ", U" ");
	N += replace(str, U"{}", U"");
	N += replace(str, U"\\^", U"^");
	N += replace(str, U"\\%", U"%");
	N += replace(str, U"\\&", U"&amp");
	N += replace(str, U"<", U"&lt");
	N += replace(str, U">", U"&gt");
	N += replace(str, U"\\{", U"{");
	N += replace(str, U"\\}", U"}");
	N += replace(str, U"\\#", U"#");
	N += replace(str, U"\\~", U"~");
	N += replace(str, U"\\_", U"_");
	N += replace(str, U"\\,", U" ");
	N += replace(str, U"\\;", U" ");
	N += replace(str, U"\\textbackslash", U"&bsol;");
	return N;
}

// deal with escape simbols in normal text, \x{} commands, Command environments
// must be done before \command{} and environments are replaced with html tags
// return -1 if failed
Long NormalTextEscape(Str32_IO str)
{
	Long i{}, N1{}, N{}, Nnorm{};
	Str32 temp;
	vector<Long> ind, ind1;
	FindNormalText(ind, str);
	FindComBrace(ind1, U"\\x", str);
	Nnorm = CombineRange(ind, ind, ind1);
	if (Nnorm < 0) return -1;
	FindEnv(ind1, str, U"Command");
	Nnorm = CombineRange(ind, ind, ind1);
	if (Nnorm < 0) return -1;
	for (i = 2 * Nnorm - 2; i >= 0; i -= 2) {
		temp = str.substr(ind[i], ind[i + 1] - ind[i] + 1);
		N1 = TextEscape(temp); if (N1 < 0) continue;
		N += N1;
		str.erase(ind[i], ind[i + 1] - ind[i] + 1);
		str.insert(ind[i], temp);
	}
	return N;
}

// process table
// return number of tables processed, return -1 if failed
// must be used after EnvLabel()
Long Table(Str32_IO str)
{
	Long i{}, j{}, N{}, ind0{}, ind1{}, ind2{}, ind3{}, Nline;
	vector<Long> ind;
	vector<Long> indLine; // stores the position of "\hline"
	Str32 caption;
	N = FindEnv(ind, str, U"table", 'o');
	if (N == 0) return 0;
	for (i = 2 * N - 2; i >= 0; i -= 2) {
		ind0 = str.find(U"\\caption");
		if (ind0 < 0 || ind0 > ind[i + 1]) {
			cout << "table no caption!" << endl; return -1;  // break point here
		}
		ind0 += 8; ind0 = ExpectKey(str, U"{", ind0);
		ind1 = PairBraceR(str, ind0 - 1);
		caption = str.substr(ind0, ind1 - ind0);
		// recognize \hline and replace with tags, also deletes '\\'
		while (true) {
			ind0 = str.find(U"\\hline", ind0);
			if (ind0 < 0 || ind0 > ind[i + 1]) break;
			indLine.push_back(ind0);
			ind0 += 5;
		}
		Nline = indLine.size();
		str.erase(indLine[Nline - 1], 6);
		str.insert(indLine[Nline - 1], U"</td></tr></table>");
		ind0 = ExpectKeyReverse(str, U"\\\\", indLine[Nline - 1] - 1);
		str.erase(ind0 + 1, 2);
		for (j = Nline - 2; j > 0; --j) {
			str.erase(indLine[j], 6);
			str.insert(indLine[j], U"</td></tr><tr><td>");
			ind0 = ExpectKeyReverse(str, U"\\\\", indLine[j] - 1);
			str.erase(ind0 + 1, 2);
		}
		str.erase(indLine[0], 6);
		str.insert(indLine[0], U"<table><tr><td>");
	}
	// second round, replace '&' with tags
	// delete latex code
	// TODO: add title
	FindEnv(ind, str, U"table", 'o');
	for (i = 2 * N - 2; i >= 0; i -= 2) {
		ind0 = ind[i] + 12; ind1 = ind[i + 1];
		while (true) {
			ind0 = str.find('&', ind0);
			if (ind0 < 0 || ind0 > ind1) break;
			str.erase(ind0, 1);
			str.insert(ind0, U"</td><td>");
			ind1 += 8;
		}
		ind0 = str.rfind(U"</table>", ind1) + 8;
		str.erase(ind0, ind1 - ind0 + 1);
		ind0 = str.find(U"<table>", ind[i]) - 1;
		str.erase(ind[i], ind0 - ind[i] + 1);
	}
	return N;
}

// process Itemize environments
// return number processed, or -1 if failed
Long Itemize(Str32_IO str)
{
	Long i{}, j{}, N{}, Nitem{}, ind0{};
	vector<Long> indIn, indOut;
	vector<Long> indItem; // positions of each "\item"
	N = FindEnv(indIn, str, U"itemize");
	FindEnv(indOut, str, U"itemize", 'o');
	for (i = 2 * N - 2; i >= 0; i -= 2) {
		// delete paragraph tags
		ind0 = indIn[i];
		while (true) {
			ind0 = str.find(U"<p>　　", ind0);
			if (ind0 < 0 || ind0 > indIn[i + 1])
				break;
			str.erase(ind0, 5); indIn[i + 1] -= 5; indOut[i + 1] -= 5;
		}
		ind0 = indIn[i];
		while (true) {
			ind0 = str.find(U"</p>", ind0);
			if (ind0 < 0 || ind0 > indIn[i + 1])
				break;
			str.erase(ind0, 4); indIn[i + 1] -= 4;  indOut[i + 1] -= 4;
		}
		// replace tags
		indItem.resize(0);
		str.erase(indIn[i + 1] + 1, indOut[i + 1] - indIn[i + 1]);
		str.insert(indIn[i + 1] + 1, U"</li></ul>");
		ind0 = indIn[i];
		while (true) {
			ind0 = str.find(U"\\item", ind0);
			if (ind0 < 0 || ind0 > indIn[i + 1]) break;
			indItem.push_back(ind0); ind0 += 5;
		}
		Nitem = indItem.size();

		for (j = Nitem - 1; j > 0; --j) {
			str.erase(indItem[j], 5);
			str.insert(indItem[j], U"</li><li>");
		}
		str.erase(indItem[0], 5);
		str.insert(indItem[0], U"<ul><li>");
		str.erase(indOut[i], indItem[0] - indOut[i]);
	}
	return N;
}

// process enumerate environments
// similar to Itemize()
Long Enumerate(Str32_IO str)
{
	Long i{}, j{}, N{}, Nitem{}, ind0{};
	vector<Long> indIn, indOut;
	vector<Long> indItem; // positions of each "\item"
	N = FindEnv(indIn, str, U"enumerate");
	FindEnv(indOut, str, U"enumerate", 'o');
	for (i = 2 * N - 2; i >= 0; i -= 2) {
		// delete paragraph tags
		ind0 = indIn[i];
		while (true) {
			ind0 = str.find(U"<p>　　", ind0);
			if (ind0 < 0 || ind0 > indIn[i + 1])
				break;
			str.erase(ind0, 5); indIn[i + 1] -= 5; indOut[i + 1] -= 5;
		}
		ind0 = indIn[i];
		while (true) {
			ind0 = str.find(U"</p>", ind0);
			if (ind0 < 0 || ind0 > indIn[i + 1])
				break;
			str.erase(ind0, 4); indIn[i + 1] -= 4; indOut[i + 1] -= 4;
		}
		// replace tags
		indItem.resize(0);
		str.erase(indIn[i + 1] + 1, indOut[i + 1] - indIn[i + 1]);
		str.insert(indIn[i + 1] + 1, U"</li></ol>");
		ind0 = indIn[i];
		while (true) {
			ind0 = str.find(U"\\item", ind0);
			if (ind0 < 0 || ind0 > indIn[i + 1]) break;
			indItem.push_back(ind0); ind0 += 5;
		}
		Nitem = indItem.size();

		for (j = Nitem - 1; j > 0; --j) {
			str.erase(indItem[j], 5);
			str.insert(indItem[j], U"</li><li>");
		}
		str.erase(indItem[0], 5);
		str.insert(indItem[0], U"<ol><li>");
		str.erase(indOut[i], indItem[0] - indOut[i]);
	}
	return N;
}

// process \footnote{}, return number of \footnote{} found
Long footnote(Str32_IO str)
{
	Long ind0{}, ind1{}, ind2{}, N{};
	Str32 note, idNo;
	while (true) {
		ind0 = str.find(U"\\footnote", ind0);
		if (ind0 < 0) break;
		N++;
		if (N == 1)
			str += U"\n<hr><p>\n";
		ind1 = ExpectKey(str, U"{", ind0 + 9);
		ind2 = PairBraceR(str, ind1 - 1);
		note = str.substr(ind1, ind2 - ind1);
		str.erase(ind0, ind2 - ind0 + 1);
		ind0 -= DeleteSpaceReturn(str, ind0 - 1, 'l');
		num2str(idNo, N);
		str.insert(ind0, U"<sup><a href = \"#footnote" + idNo + U"\">" + idNo + U"</a></sup>");
		str += U"<span id = \"footnote" + idNo + U"\"></span>" + idNo + U". " + note + U"<br>\n";
	}
	if (N > 0)
		str += U"</p>";
	return N;
}

}