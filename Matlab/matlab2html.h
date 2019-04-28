#pragma once
#include "matlab.h"

namespace slisc {

// find all strings in a matlab code
inline Long Matlab_strings(vector<Long> &ind, Str32_I str)
{
	Long ind0 = 0;
	Bool in_string = false;
	while (true) {
		ind0 = str.find(U'\'', ind0);
		if (ind0 < 0) {
			if (isodd(ind.size()))
				SLS_ERR("range pairs must be even!");
			return ind.size() / 2;
		}

		if (!in_string) {
			// first quote in comment
			Long ikey;
			FindMultipleReverse(ikey, str, { U"\n", U"%" }, ind0 - 1);
			if (ikey == 1) {
				++ind0; continue;
			}
			// transpose, not a single quote
			if (Matlab_is_trans(str, ind0)) {
				++ind0; continue;
			}
		}

		ind.push_back(ind0);
		in_string = !in_string;
		++ind0;
	}
}

// highlight Matlab keywords
// str is Matlab code only
// replace keywords with <span class="keyword_class">...</span>
inline Long Matlab_keywords(Str32_IO str, const vector<Str32> keywords, Str32_I keyword_class)
{
	// find comments and strings
	vector<Long> ind_comm, ind_str;
	Matlab_strings(ind_str, str);
	Matlab_comments(ind_comm, str, ind_str);

	Long N = 0;
	Long ind0 = str.size() - 1;
	while (true) {
		// process str backwards so that unprocessed ranges doesn't change

		// find the last keyword
		vector<Long> ikeys;
		ind0 = FindMultipleReverseN(ikeys, str, keywords, ind0);
		if (ind0 < 0)
			break;

		// in case of multiple match, use the longest keyword (e.g. elseif/else)
		Long ikey, max_size = 0;
		for (Long i = 0; i < ikeys.size(); ++i) {
			if (max_size < keywords[ikeys[i]].size()) {
				max_size = keywords[ikeys[i]].size();
				ikey = ikeys[i];
			}
		}

		// check whole word
		if (!is_whole_word(str, ind0, keywords[ikey].size())) {
			--ind0;  continue;
		}
		// ignore keyword in comment or in string
		if (IndexInRange(ind0, ind_comm) || IndexInRange(ind0, ind_str)) {
			--ind0; continue;
		}
		// found keyword
		str.replace(ind0, keywords[ikey].size(),
			U"<span class = \"" + keyword_class + "\">" + keywords[ikey] + U"</span>");
		++N;
	}
	return N;
}

// highlight comments in Matlab code
// code is Matlab code snippet
// replace keywords with <span class="string_class">...</span>
// return the total number of strings and commens processed
inline Long Matlab_string(Str32_IO code, Str32_I str_class)
{
	Long N = 0;
	// find comments and strings
	vector<Long> ind_str;
	Matlab_strings(ind_str, code);

	// highlight backwards
	for (Long i = ind_str.size() - 2; i >= 0; i -= 2) {
		code.insert(ind_str[i + 1] + 1, U"</span>");
		code.insert(ind_str[i], U"<span class = \"" + str_class + "\">");
		++N;
	}
	return N;
}

// highlight comments in Matlab code
// code is Matlab code snippet
// replace keywords with <span class="string_class">...</span>
// return the total number of strings and commens processed
inline Long Matlab_comment(Str32_IO code, Str32_I comm_class)
{
	Long N = 0;
	// find comments and strings
	vector<Long> ind_comm, ind_str;
	Matlab_strings(ind_str, code);
	Matlab_comments(ind_comm, code, ind_str);
	
	// highlight backwards
	for (Long i = ind_comm.size() - 2; i >= 0; i -= 2) {
		code.insert(ind_comm[i + 1] + 1, U"</span>");
		code.insert(ind_comm[i], U"<span class = \"" + comm_class + "\">");
		++N;
	}
	return N;
}

// highlight matlab code
// str is Matlab code only
// return the number of highlight
inline Long Matlab_highlight(Str32_IO str)
{
	// ======= settings ========
	vector<Str32> keywords = { U"break", U"case", U"catch", U"classdef",
		U"continue", U"else", U"elseif", U"end", U"for", U"function",
		U"global", U"if", U"otherwie", U"parfor", U"persistent",
		U"return", U"spmd", U"switch", U"try", U"while"};
	
	Str32 keyword_class = U"keyword";
	Str32 str_class = U"string";
	Str32 comm_class = U"comment";

	Long Nwidth = 160; // maximum line width
	// ==========================

	Long N = 0;
	// highlight keywords
	N += Matlab_keywords(str, keywords, keyword_class);
	// highlight comments
	Matlab_comment(str, comm_class);
	// highlight strings
	Matlab_string(str, str_class);
	return N;
}

} // namespace slisc
