﻿#pragma once
#include "tex.h"
#include "../SLISC/arith/scalar_arith.h"

namespace slisc {

// ensure space around (CString)name
// return number of spaces added
// will only check equation env. and inline equations
inline Long EnsureSpace(Str32_I name, Str32_IO str, Long start, Long end)
{
    Long N{}, ind0{ start };
    while (true) {
        ind0 = str.find(name, ind0);
        if (ind0 < 0 || ind0 > end) break;
        if (ind0 == 0 || str.at(ind0 - 1) != U' ') {
            str.insert(ind0, U" "); ++ind0; ++N;
        }
        ind0 += name.size();
        if (ind0 == size(str) || str.at(ind0) != U' ') {
            str.insert(ind0, U" "); ++N;
        }
    }
    return N;
}

// check if an index is in an HTML tag "<name>...</name>
inline Bool is_in_tag(Str32_I str, Str32_I name, Long_I ind)
{
    Long ind1 = str.rfind(U"<" + name + U">", ind);
    if (ind1 < 0)
        return false;
    Long ind2 = str.rfind(U"</" + name + U">", ind);
    if (ind2 < ind1)
        return true;
    return false;
}

// ensure space around '<' and '>' in equation env. and $$ env
// return number of spaces added
inline Long EqOmitTag(Str32_IO str)
{
    Long i{}, N{}, Nrange{};
    Intvs intv, indInline;
    find_display_eq(intv, str);
    find_inline_eq(indInline, str);
    Nrange = combine(intv, indInline);
    for (i = Nrange - 1; i >= 0; --i) {
        N += EnsureSpace(U"<", str, intv.L(i), intv.R(i));
        N += EnsureSpace(U">", str, intv.L(i), intv.R(i));
    }
    return N;
}

inline Long define_newcommands(vecStr32_O rules)
{
    // rules (order matters for the same command name)
    // suggested order: from complex to simple
    // an extra space will be inserted at both ends of each rule
    // 1. cmd name | 2. format | 3. number of arguments (including optional) | 4. rule
    rules.clear();
    ifstream fin("new_commands.txt");
    string line;
    while (getline(fin, line)) {
        Long ind0 = line.find("\""), ind1;
        if (ind0 < 0) continue;
        ind0 = -1;
        for (Long i = 0; i < 4; ++i) {
            if ((ind0 = line.find("\"", ind0+1)) < 0)
                throw Str32(U"内部错误： new_commands.txt 格式错误！");
            if ((ind1 = line.find("\"", ind0+1)) < 0)
                throw Str32(U"内部错误： new_commands.txt 格式错误！");
            rules.push_back(u32(line.substr(ind0+1, ind1-ind0-1)));
            ind0 = ind1;
        }
    }
    for (Long i = 4; i < size(rules); i+=4) {
        if (rules[i] < rules[i - 4])
            throw Str32(U"内部错误： define_newcommands(): rules not sorted: " + rules[i]);
    }
    return rules.size();
}

// ==== directly implement \newcommand{}{} by replacing ====
// does not support multiple layer! (call multiple times for that)
// braces can be omitted e.g. \cmd12 (will match in the end)
// matching order: from complicated to simple
// does not support more than 9 arguments (including optional arg)
// command will be ignored if there is no match
// return the number of commands replaced
inline Long newcommand(Str32_IO str, vecStr32_I rules)
{
    Long N = 0;

    // all commands to find
    if (rules.size() % 4 != 0)
        throw Str32(U"rules.size() illegal!");
    // == print newcommands ===
    // for (Long i = 0; i < size(rules); i += 4)
    //     cout << rules[i] << " || " << rules[i+1] << " || " << rules[i+2] << " || " << rules[i+3] << endl;

    // all commands to replace
    vecStr32 cmds;
    for (Long i = 0; i < size(rules); i+=4)
        cmds.push_back(rules[i]);

    Long ind0 = 0, ikey;
    Str32 cmd, new_cmd, format;
    vecStr32 args; // cmd arguments, including optional
    while (true) {
        ind0 = find_command(ikey, str, cmds, ind0);
        if (ind0 < 0)
            break;
        cmd = cmds[ikey];
        Bool has_st = command_star(str, ind0);
        Bool has_op = command_has_opt(str, ind0);
        Long Narg = command_Narg(str, ind0); // actual args used in [] or {}
        format.clear();
        if (has_st)
            format += U"*";
        if (Narg == -1) {
            Narg = 1;
            format += U"()";
        }
        else if (Narg == -2) {
            Narg = 2;
            format += U"[]()";
        }
        else {
            if (has_op)
                format += U"[]";
        }

        // decide which rule to use (result: rules[ind])
        Long ind = -1, Narg_rule = -1;
        Long candi = -1; // `rules[candi]` is the last compatible rule with omitted `{}`
        Str32 rule_format;
        while (1) {
            Long ind1;
            if (ind == 0) {
                lookup(ind1, cmds, cmd, ind+1, cmds.size() - 1);
                while (ind1 > 0 && cmds[ind1 - 1] == cmd)
                    --ind1;
            }
            else
                ind1 = search(cmd, cmds, ind+1);
            if (ind1 < 0) {
                if (candi < 0) {
                    ind = -1; break;
                    // throw Str32(U"内部错误：命令替换规则不存在：" + cmd + U" （格式：" + format + U"）");
                }
                ind = candi;
                break;
            }
            else
                ind = ind1;
            
            rule_format = rules[ind*4 + 1];
            // match format
            if (format.substr(0, rule_format.size()) == rule_format) {
                Narg_rule = Long(rules[ind*4 + 2][0]) - Long(U'0');
                if (Narg_rule > Narg) { // has omitted `{}`
                    candi = ind;
                    continue;
                }
                break;
            }
        }
        if (ind < 0) {
            ++ind0; continue;
        }
        // get arguments
        Long end = -1; // replace str[ind0] to str[end-1]
        if (rule_format == U"[]()" || rule_format == U"*[]()") {
            args.resize(2);
            args[0] = command_opt(str, ind0);
            Long indL = str.find(U'(', ind0);
            Long indR = pair_brace(str, indL);
            args[1] = str.substr(indL + 1, indR - indL - 1);
            trim(args[1]);
            end = indR + 1;
        }
        else if (rule_format == U"()" || rule_format == U"*()") {
            Long indL = str.find(U'(', ind0);
            Long indR = pair_brace(str, indL);
            args.resize(1);
            args[0] = str.substr(indL + 1, indR - indL - 1);
            trim(args[0]);
            end = indR + 1;
        }
        else {
            args.resize(Narg_rule);
            for (Long i = 0; i < Narg_rule; ++i)
                end = command_arg(args[i], str, ind0, i, true, false, true);
            if (Narg_rule == 0) {
                if (rule_format[0] == U'*')
                    end = skip_command(str, ind0, 0, false, false, true);
                else
                    end = skip_command(str, ind0, 0, false, false, false);
            }
        }
        // apply rule
        new_cmd = rules[ind*4 + 3];
        for (Long i = 0; i < Narg_rule; ++i)
            replace(new_cmd, U"#" + num2str32(i + 1), args[i]);
        str.replace(ind0, end - ind0, U' ' + new_cmd + U' '); ++N;
    }
    return N;
}

// deal with escape simbols in normal text
// str must be normal text
inline Long TextEscape(Str32_IO str)
{
    Long N{};
    N += replace(str, U"<", U"&lt;");
    N += replace(str, U">", U"&gt;");
    Long tmp = replace(str, U"\\\\", U"<br>");
    if (tmp > 0)
        throw Str32(U"正文中发现 '\\\\' 强制换行！");
    N += tmp;
    N += replace(str, U"\\ ", U" ");
    N += replace(str, U"{}", U"");
    N += replace(str, U"\\^", U"^");
    N += replace(str, U"\\%", U"%");
    N += replace(str, U"\\&", U"&amp");
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

// deal with escape simbols in normal text, Command environments
// must be done before \command{} and environments are replaced with html tags
inline Long NormalTextEscape(Str32_IO str)
{
    Long N1{}, N{}, Nnorm{};
    Str32 temp;
    Intvs intv;
    Bool warned = false;
    Nnorm = FindNormalText(intv, str);
    for (Long i = Nnorm - 1; i >= 0; --i) {
        temp = str.substr(intv.L(i), intv.R(i) - intv.L(i) + 1);
        try {N1 = TextEscape(temp);}
        catch(Str32 str) {
            if (!warned) {
                SLS_WARN(str); warned = true;
            }
        }
        if (N1 < 0)
            continue;
        N += N1;
        str.replace(intv.L(i), intv.R(i) - intv.L(i) + 1, temp);
    }
    return N;
}

// process table
// return number of tables processed
// must be used after EnvLabel()
inline Long Table(Str32_IO str)
{
    Long N{}, ind0{}, ind1{}, Nline;
    Intvs intv;
    vecLong indLine; // stores the position of "\hline"
    vecStr32 captions;
    Str32 str_beg = U"<div class = \"eq\" align = \"center\">"
        U"<div class = \"w3 - cell\" style = \"width:710px\">\n<table><tr><td>";
    Str32 str_end = U"</td></tr></table>\n</div></div>";
    N = find_env(intv, str, U"table", 'o');
    if (N == 0) return 0;
    captions.resize(N);
    for (Long i = N - 1; i >= 0; --i) {
        indLine.clear();
        ind0 = find_command(str, U"caption", intv.L(i));
        if (ind0 < 0 || ind0 > intv.R(i))
            throw Str32(U"table no caption!");
        command_arg(captions[i], str, ind0);
        if ((Long)captions[i].find(U"\\footnote") >= 0)
            throw Str32(U"表格标题中不能添加 \\footnote");
        // recognize \hline and replace with tags, also deletes '\\'
        while (true) {
            ind0 = str.find(U"\\hline", ind0);
            if (ind0 < 0 || ind0 > intv.R(i)) break;
            indLine.push_back(ind0);
            ind0 += 5;
        }
        Nline = indLine.size();
        str.replace(indLine[Nline - 1], 6, str_end);
        ind0 = ExpectKeyReverse(str, U"\\\\", indLine[Nline - 1] - 1);
        str.erase(ind0 + 1, 2);
        for (Long j = Nline - 2; j > 0; --j) {
            str.replace(indLine[j], 6, U"</td></tr><tr><td>");
            ind0 = ExpectKeyReverse(str, U"\\\\", indLine[j] - 1);
            str.erase(ind0 + 1, 2);
        }
        str.replace(indLine[0], 6, str_beg);
    }
    // second round, replace '&' with tags
    // delete latex code
    // TODO: add title
    find_env(intv, str, U"table", 'o');
    for (Long i = N - 1; i >= 0; --i) {
        ind0 = intv.L(i) + 12; ind1 = intv.R(i);
        while (true) {
            ind0 = str.find('&', ind0);
            if (ind0 < 0 || ind0 > ind1) break;
            str.erase(ind0, 1);
            str.insert(ind0, U"</td><td>");
            ind1 += 8;
        }
        ind0 = str.rfind(str_end, ind1) + str_end.size();
        str.erase(ind0, ind1 - ind0 + 1);
        ind0 = str.find(str_beg, intv.L(i)) - 1;
        str.replace(intv.L(i), ind0 - intv.L(i) + 1,
            U"<div align = \"center\"> " + Str32(gv::is_eng?U"Tab. ":U"表") + num2str(i + 1) + U"：" +
            captions[i] + U"</div>");
    }
    return N;
}

// process Itemize environments
// return number processed
inline Long Itemize(Str32_IO str)
{
    Long i{}, j{}, N{}, Nitem{}, ind0{};
    Intvs intvIn, intvOut;
    vecLong indItem; // positions of each "\item"
    N = find_env(intvIn, str, U"itemize");
    find_env(intvOut, str, U"itemize", 'o');
    for (i = N - 1; i >= 0; --i) {
        // delete paragraph tags
        ind0 = intvIn.L(i);
        while (true) {
            ind0 = str.find(U"<p>　　", ind0);
            if (ind0 < 0 || ind0 > intvIn.R(i))
                break;
            str.erase(ind0, 5); intvIn.R(i) -= 5; intvOut.R(i) -= 5;
        }
        ind0 = intvIn.L(i);
        while (true) {
            ind0 = str.find(U"</p>", ind0);
            if (ind0 < 0 || ind0 > intvIn.R(i))
                break;
            str.erase(ind0, 4); intvIn.R(i) -= 4;  intvOut.R(i) -= 4;
        }
        // replace tags
        indItem.resize(0);
        str.erase(intvIn.R(i) + 1, intvOut.R(i) - intvIn.R(i));
        str.insert(intvIn.R(i) + 1, U"</li></ul>");
        ind0 = intvIn.L(i);
        while (true) {
            ind0 = str.find(U"\\item", ind0);
            if (ind0 < 0 || ind0 > intvIn.R(i)) break;
            if (str[ind0 + 5] != U' ' && str[ind0 + 5] != U'\n')
                throw Str32(U"\\item 命令后面必须有空格或回车！");
            indItem.push_back(ind0); ind0 += 5;
        }
        Nitem = indItem.size();
        if (Nitem == 0)
            continue;

        for (j = Nitem - 1; j > 0; --j) {
            str.erase(indItem[j], 5);
            str.insert(indItem[j], U"</li><li>");
        }
        str.erase(indItem[0], 5);
        str.insert(indItem[0], U"<ul><li>");
        str.erase(intvOut.L(i), indItem[0] - intvOut.L(i));
    }
    return N;
}

// process enumerate environments
// similar to Itemize()
inline Long Enumerate(Str32_IO str)
{
    Long i{}, j{}, N{}, Nitem{}, ind0{};
    Intvs intvIn, intvOut;
    vecLong indItem; // positions of each "\item"
    N = find_env(intvIn, str, U"enumerate");
    find_env(intvOut, str, U"enumerate", 'o');
    for (i = N - 1; i >= 0; --i) {
        // delete paragraph tags
        ind0 = intvIn.L(i);
        while (true) {
            ind0 = str.find(U"<p>　　", ind0);
            if (ind0 < 0 || ind0 > intvIn.R(i))
                break;
            str.erase(ind0, 5); intvIn.R(i) -= 5; intvOut.R(i) -= 5;
        }
        ind0 = intvIn.L(i);
        while (true) {
            ind0 = str.find(U"</p>", ind0);
            if (ind0 < 0 || ind0 > intvIn.R(i))
                break;
            str.erase(ind0, 4); intvIn.R(i) -= 4; intvOut.R(i) -= 4;
        }
        // replace tags
        indItem.resize(0);
        str.erase(intvIn.R(i) + 1, intvOut.R(i) - intvIn.R(i));
        str.insert(intvIn.R(i) + 1, U"</li></ol>");
        ind0 = intvIn.L(i);
        while (true) {
            ind0 = str.find(U"\\item", ind0);
            if (ind0 < 0 || ind0 > intvIn.R(i)) break;
            if (str[ind0 + 5] != U' ' && str[ind0 + 5] != U'\n')
                throw Str32(U"\\item 命令后面必须有空格或回车！");
            indItem.push_back(ind0); ind0 += 5;
        }
        Nitem = indItem.size();
        if (Nitem == 0)
            continue;

        for (j = Nitem - 1; j > 0; --j) {
            str.erase(indItem[j], 5);
            str.insert(indItem[j], U"</li><li>");
        }
        str.erase(indItem[0], 5);
        str.insert(indItem[0], U"<ol><li>");
        str.erase(intvOut.L(i), indItem[0] - intvOut.L(i));
    }
    return N;
}

// process \footnote{}, return number of \footnote{} found
inline Long footnote(Str32_IO str, Str32_I entry, Str32_I url)
{
    Long ind0 = 0, N = 0;
    Str32 temp, idNo;
    ind0 = find_command(str, U"footnote", ind0);
    if (ind0 < 0)
        return 0;
    str += U"\n<hr><p>\n";
    while (true) {
        ++N;
        num2str(idNo, N);
        command_arg(temp, str, ind0);
        str += U"<a href = \"" + url + entry + U".html#ret" + idNo + U"\" id = \"note" + idNo + U"\">" + idNo + U". <b>^</b></a> " + temp + U"<br>\n";
        ind0 -= eatL(str, ind0 - 1, U" \n");
        str.replace(ind0, skip_command(str, ind0, 1) - ind0,
            U"<sup><a href = \"" + url + entry + U".html#note" + idNo + U"\" id = \"ret" + idNo + U"\"><b>" + idNo + U"</b></a></sup>");
        ++ind0;
        ind0 = find_command(str, U"footnote", ind0);
        if (ind0 < 0)
            break;
    }
    str += U"</p>";
    return N;
}

inline Long subsections(Str32_IO str)
{
    Long ind0 = 0, N = 0;
    Str32 subtitle;
    while (1) {
        ind0 = find_command(str, U"subsection", ind0);
        if (ind0 < 0)
            return N;
        ++N;
        command_arg(subtitle, str, ind0);
        Long ind1 = skip_command(str, ind0, 1);
        str.replace(ind0, ind1 - ind0, U"<h2 class = \"w3-text-indigo\"><b>" + num2str32(N) + U". " + subtitle + U"</b></h2>");
    }
}

// replace "\href{http://example.com}{name}"
// with <a href="http://example.com">name</a>
inline Long href(Str32_IO str)
{
    Long ind0 = 0, N = 0, tmp;
    Str32 name, url;
    while (1) {
        ind0 = find_command(str, U"href", ind0);
        if (ind0 < 0)
            return N;
        if (index_in_env(tmp, ind0, { U"equation", U"align", U"gather" }, str)) {
            ++ind0; continue;
        }
        command_arg(url, str, ind0, 0);
        command_arg(name, str, ind0, 1);
        if (url.substr(0, 7) != U"http://" &&
            url.substr(0, 8) != U"https://") {
            throw Str32(U"链接格式错误: " + url);
        }

        Long ind1 = skip_command(str, ind0, 2);
        str.replace(ind0, ind1 - ind0,
            "<a href=\"" + url + "\" target = \"_blank\">" + name + "</a>");
        ++N; ++ind0;
    }
}

} // namespace slisc
