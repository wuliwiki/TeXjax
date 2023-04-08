﻿#pragma once
#include "../SLISC/file/sqlite_ext.h"
#include <SQLiteCpp/Statement.h>
#include <SQLiteCpp/Database.h>
#include "../SLISC/str/str.h"
#include "../SLISC/str/disp.h"
#include "../SLISC/algo/sort.h"
#include "../SLISC/algo/graph.h"
#include "../SLISC/util/sha1sum.h"


using namespace slisc;

// global variables, must be set only once
namespace gv {
    Str32 path_in; // e.g. ../PhysWiki/
    Str32 path_out; // e.g. ../littleshi.cn/online/
    Str32 path_data; // e.g. ../littleshi.cn/data/
    Str32 url; // e.g. https://wuli.wiki/online/
    Bool is_wiki; // editing wiki or note
    Bool is_eng = false; // use english for auto-generated text (Eq. Fig. etc.)
    Bool is_entire = false; // running one tex or the entire wiki
}

class scan_err : std::exception
{
private:
    Str m_msg;
public:
    explicit scan_err(Str_I msg): m_msg(msg) {}
    explicit scan_err(Str32_I msg): m_msg(u8(msg)) {}

    const char* what() const noexcept {
        return m_msg.c_str();
    }
};

// 内部错误
class internal_err : public scan_err
{
public:
    explicit internal_err(Str_I msg): scan_err(u8"内部错误（请联系管理员）： " + msg) {}
    explicit internal_err(Str32_I msg): internal_err(u8(msg)) {}
};

#include "../TeX/tex2html.h"
#include "check_entry.h"
#include "labels.h"
#include "sqlite_db.h"
#include "code.h"
#include "bib.h"
#include "toc.h"
#include "tex_envs.h"
#include "tex_tidy.h"
#include "statistics.h"
#include "google_trans.h"

// trim "\n" and " " on both sides
// remove unnecessary "\n"
// replace “\n\n" with "\n</p>\n<p>　　\n"
inline Long paragraph_tag1(Str32_IO str)
{
    Long ind0 = 0, N = 0;
    trim(str, U" \n");
    // delete extra '\n' (more than two continuous)
    while (1) {
        ind0 = str.find(U"\n\n\n", ind0);
        if (ind0 < 0)
            break;
        eatR(str, ind0 + 2, U"\n");
    }

    // replace "\n\n" with "\n</p>\n<p>　　\n"
    N = replace(str, U"\n\n", U"\n</p>\n<p>　　\n");
    return N;
}

inline Long paragraph_tag(Str32_IO str)
{
    Long N = 0, ind0 = 0, left = 0, length = -500, ikey = -500;
    Str32 temp, env, end, begin = U"<p>　　\n";

    // "begin", and commands that cannot be in a paragraph
    vecStr32 commands = {U"begin", U"subsection", U"subsubsection", U"pentry"};

    // environments that must be in a paragraph (use "<p>" instead of "<p>　　" when at the start of the paragraph)
    vecStr32 envs_eq = {U"equation", U"align", U"gather", U"lstlisting"};

    // environments that needs paragraph tags inside
    vecStr32 envs_p = { U"example", U"exercise", U"definition", U"theorem", U"lemma", U"corollary"};

    // 'n' (for normal); 'e' (for env_eq); 'p' (for env_p); 'f' (end of file)
    char next, last = 'n';

    Intvs intv, intvInline, intv_tmp;

    // handle normal text intervals separated by
    // commands in "commands" and environments
    while (1) {
        // decide mode
        if (ind0 == size(str)) {
            next = 'f';
        }
        else {
            ind0 = find_command(ikey, str, commands, ind0);
            find_double_dollar_eq(intv, str, 'o');
            find_sqr_bracket_eq(intv_tmp, str, 'o');
            combine(intv, intv_tmp);
            Long itmp = is_in_no(ind0, intv);
            if (itmp >= 0) {
                ind0 = intv.R(itmp) + 1; continue;
            }
            if (ind0 < 0)
                next = 'f';
            else {
                next = 'n';
                if (ikey == 0) { // found environment
                    find_inline_eq(intvInline, str);
                    if (is_in(ind0, intvInline)) {
                        ++ind0; continue; // ignore in inline equation
                    }
                    command_arg(env, str, ind0);
                    if (search(env, envs_eq) >= 0) {
                        next = 'e';
                    }
                    else if (search(env, envs_p) >= 0) {
                        next = 'p';
                    }
                }
            }
        }

        // decide ending tag
        if (next == 'n' || next == 'p') {
            end = U"\n</p>\n";
        }
        else if (next == 'e') {
            // equations can be inside paragraph
            if (ExpectKeyReverse(str, U"\n\n", ind0 - 1) >= 0) {
                end = U"\n</p>\n<p>\n";
            }
            else
                end = U"\n";
        }
        else if (next == 'f') {
            end = U"\n</p>\n";
        }

        // add tags
        length = ind0 - left;
        temp = str.substr(left, length);
        N += paragraph_tag1(temp);
        if (temp.empty()) {
            if (next == 'e' && last != 'e') {
                temp = U"\n<p>\n";
            }
            else if (last == 'e' && next != 'e') {
                temp = U"\n</p>\n";
            }
            else {
                temp = U"\n";
            }
        }
        else {
            temp = begin + temp + end;
        }
        str.replace(left, length, temp);
        find_inline_eq(intvInline, str);
        if (next == 'f')
            return N;
        ind0 += temp.size() - length;
        
        // skip
        if (ikey == 0) {
            if (next == 'p') {
                Long ind1 = skip_env(str, ind0);
                Long left, right;
                left = inside_env(right, str, ind0, 2);
                length = right - left;
                temp = str.substr(left, length);
                paragraph_tag(temp);
                str.replace(left, length, temp);
                find_inline_eq(intvInline, str);
                ind0 = ind1 + temp.size() - length;
            }
            else
                ind0 = skip_env(str, ind0);
        }
        else {
            ind0 = skip_command(str, ind0, 1);
            if (ind0 < size(str)) {
                Long ind1 = expect(str, U"\\label", ind0);
                if (ind1 > 0)
                    ind0 = skip_command(str, ind1 - 6, 1);
            }
        }

        left = ind0;
        last = next;
        if (ind0 == size(str)) {
            continue;
        }
        
        // beginning tag for next interval
        
        if (last == 'n' || last == 'p')
            begin = U"\n<p>　　\n";
        else if (last == 'e') {
            if (expect(str, U"\n\n", ind0) >= 0) {
                begin = U"\n</p>\n<p>　　\n";
            }
            else
                begin = U"\n";
        }    
    }
}

// replace \pentry comman with html round panel
inline Long pentry(Str32_IO str)
{
    if (!gv::is_eng)
        return Command2Tag(U"pentry", U"<div class = \"w3-panel w3-round-large w3-light-blue\"><b>预备知识</b>　", U"</div>", str);
    return Command2Tag(U"pentry", U"<div class = \"w3-panel w3-round-large w3-light-blue\"><b>Prerequisite</b>　", U"</div>", str);
}

// mark incomplete
inline Long addTODO(Str32_IO str)
{
    return Command2Tag(U"addTODO", U"<div class = \"w3-panel w3-round-large w3-sand\">未完成：", U"</div>", str);
}

// replace "<" and ">" in equations
inline Long rep_eq_lt_gt(Str32_IO str)
{
    Long N = 0;
    Intvs intv, intv1;
    Str32 tmp;
    find_inline_eq(intv, str);
    find_display_eq(intv1, str); combine(intv, intv1);
    for (Long i = intv.size() - 1; i >= 0; --i) {
        Long ind0 = intv.L(i), Nstr = intv.R(i) - intv.L(i) + 1;
        tmp = str.substr(ind0, Nstr);
        N += ensure_space_around(tmp, U'<') + ensure_space_around(tmp, U'>');
        str.replace(ind0, Nstr, tmp);
    }
    return N;
}

// replace all wikipedia domain to another mirror domain
// return the number replaced
inline Long wikipedia_link(Str32_IO str)
{
    Str32 alter_domain = U"jinzhao.wiki";
    Long ind0 = 0, N = 0;
    Str32 link;
    while (1) {
        ind0 = find_command(str, U"href", ind0);
        if (ind0 < 0)
            return N;
        command_arg(link, str, ind0);
        ind0 = skip_command(str, ind0);
        ind0 = expect(str, U"{", ind0);
        if (ind0 < 0)
            throw scan_err(u8"\\href{网址}{文字} 命令格式错误！");
        Long ind1 = pair_brace(str, ind0 - 1);
        if (Long(link.find(U"wikipedia.org")) > 0) {
            replace(link, U"wikipedia.org", alter_domain);
            str.replace(ind0, ind1 - ind0, link);
        }
        ++N;
    }
}

// if a position is in \pay...\paid
inline Bool ind_in_pay(Str32_I str, Long_I ind)
{
    Long ikey;
    Long ind0 = rfind(ikey, str, { U"\\pay", U"\\paid" }, ind);
    if (ind0 < 0)
        return false;
    else if (ikey == 1)
        return false;
    else if (ikey == 0)
        return true;
    else
        throw scan_err("ind_in_pay(): unknown!");
}

// deal with "\pay"..."\paid"
inline Long pay2div(Str32_IO str)
{
    Long ind0 = 0, N = 0;
    while (1) {
        ind0 = find_command(str, U"pay", ind0);
        if (ind0 < 0)
            return N;
        ++N;
        str.replace(ind0, 4, U"<div id=\"pay\" style=\"display:inline\">");
        ind0 = find_command(str, U"paid", ind0);
        if (ind0 < 0)
            throw scan_err(u8"\\pay 命令没有匹配的 \\paid 命令");
        str.replace(ind0, 5, U"</div>");
    }
}

inline void last_next_buttons(Str32_IO html, SQLite::Database &db_read, Long_I order, Str32_I entry, Str32_I title)
{
    Str32 last_entry, last_title, last_url, next_entry, next_title, next_url;
    SQLite::Statement stmt_select(db_read,
        R"(SELECT "id", "caption" FROM "entries" WHERE "order"=?;)");

    if (order < 0)
        throw internal_err(u8"last_next_buttons()");

    // find last
    if (order == 0) {
        last_entry = entry;
        last_title = U"没有上一篇了哟~（本文不在目录中）";
    }
    else if (order == 1) {
        last_entry = entry;
        last_title = U"没有上一篇了哟~";
    }
    else {
        stmt_select.bind(1, (int)order-1);
        if (!stmt_select.executeStep())
            throw internal_err(u8"找不到具有以下 order 的词条： " + num2str(order-1));
        else {
            last_entry = u32(stmt_select.getColumn(0));
            last_title = u32(stmt_select.getColumn(1));
        }
        stmt_select.reset();
    }

    // find next
    if (order == 0) {
        next_entry = entry;
        next_title = U"没有下一篇了哟~（本文不在目录中）";
    }
    else {
        stmt_select.bind(1, (int) order + 1);
        if (!stmt_select.executeStep()) {
            next_entry = entry;
            next_title = U"没有下一篇了哟~";
        } else {
            next_entry = u32(stmt_select.getColumn(0));
            next_title = u32(stmt_select.getColumn(1));
        }
        stmt_select.reset();
    }

    // insert into html
    if (replace(html, U"PhysWikiLastEntryURL", gv::url+last_entry+U".html") != 2)
        throw internal_err(u8"\"PhysWikiLastEntry\" 在 entry_template.html 中数量不对");
    if (replace(html, U"PhysWikiNextEntryURL", gv::url+next_entry+U".html") != 2)
        throw internal_err(u8"\"PhysWikiNextEntry\" 在 entry_template.html 中数量不对");
    if (replace(html, U"PhysWikiLastTitle", last_title) != 2)
        throw internal_err(u8"\"PhysWikiLastTitle\" 在 entry_template.html 中数量不对");
    if (replace(html, U"PhysWikiNextTitle", next_title) != 2)
        throw internal_err(u8"\"PhysWikiNextTitle\" 在 entry_template.html 中数量不对");
}

// generate html from a single tex
inline void PhysWikiOnline1(Str32_O title, vecStr32_O img_ids, vecLong_O img_orders, vecStr32_O img_hashes,
    Bool_O draft, vecStr32_O keywords, vecStr32_O labels, vecLong_O label_orders,
    vecStr32_O pentries, Str32_I entry, vecStr32_I rules, SQLite::Database &db_read)
{
    Str32 str;
    read(str, gv::path_in + "contents/" + entry + ".tex"); // read tex file
    CRLF_to_LF(str);

    // read title from first comment
    get_title(title, str);
    draft = is_draft(str);
    Str32 db_title, authors, db_keys_str, db_pentry_str;
    Long order, db_draft;
    SQLite::Statement stmt_select
            (db_read,
             R"(SELECT "caption", "authors", "order", "keys", "pentry", "draft" FROM "entries" WHERE "id"=?;)");
    stmt_select.bind(1, u8(entry));
    if (!stmt_select.executeStep())
        internal_err(u8"数据库中找不到词条（应该由 editor 在创建时添加）： " + entry);
    db_title = u32(stmt_select.getColumn(0));
    authors = u32(stmt_select.getColumn(1));
    order = (int)stmt_select.getColumn(2);
    db_keys_str = u32(stmt_select.getColumn(3));
    db_pentry_str = u32(stmt_select.getColumn(4));
    db_draft = (int)stmt_select.getColumn(5);

    // read html template and \newcommand{}
    Str32 html;
    read(html, gv::path_out + "templates/entry_template.html");
    CRLF_to_LF(html);


    // check language: U"\n%%eng\n" at the end of file means english, otherwise chinese
    if ((size(str) > 7 && str.substr(size(str) - 7) == U"\n%%eng\n") ||
            gv::path_in.substr(gv::path_in.size()-4) == U"/en/")
        gv::is_eng = true;
    else
        gv::is_eng = false;

    // add keyword meta to html
    if (get_keywords(keywords, str) > 0) {
        Str32 keywords_str = keywords[0];
        for (Long i = 1; i < size(keywords); ++i) {
            keywords_str += U"," + keywords[i];
        }
        if (replace(html, U"PhysWikiHTMLKeywords", keywords_str) != 1)
            throw internal_err(u8"\"PhysWikiHTMLKeywords\" 在 entry_template.html 中数量不对");
    }
    else {
        if (replace(html, U"PhysWikiHTMLKeywords", U"高中物理, 物理竞赛, 大学物理, 高等数学") != 1)
            throw internal_err(u8"\"PhysWikiHTMLKeywords\" 在 entry_template.html 中数量不对");
    }

    // TODO: enable this when editor can do the rename in db
//    if (!db_title.empty() && title != db_title)
//        throw scan_err(U"检测到标题改变（" + db_title + U" ⇒ " + title + U"）， 请使用重命名按钮修改标题");

    // insert HTML title
    if (replace(html, U"PhysWikiHTMLtitle", title) != 1)
        throw internal_err(u8"\"PhysWikiHTMLtitle\" 在 entry_template.html 中数量不对");

    // check globally forbidden char
    global_forbid_char(str);
    // save and replace verbatim code with an index
    vecStr32 str_verb;
    verbatim(str_verb, str);
    // wikipedia_link(str);
    rm_comments(str); // remove comments
    limit_env_cmd(str);
    if (!gv::is_eng)
        autoref_space(str, true); // set true to error instead of warning
    autoref_tilde_upref(str, entry);
    if (str.empty()) str = U" ";
    // ensure spaces between chinese char and alphanumeric char
    chinese_alpha_num_space(str);
    // ensure spaces outside of chinese double quotes
    chinese_double_quote_space(str);
    // check escape characters in normal text i.e. `& # _ ^`
    check_normal_text_escape(str);
    // check non ascii char in equations (except in \text)
    check_eq_ascii(str);
    // forbid empty lines in equations
    check_eq_empty_line(str);
    // add spaces around inline equation
    inline_eq_space(str);
    // replace "<" and ">" in equations
    rep_eq_lt_gt(str);
    // escape characters
    NormalTextEscape(str);
    // add paragraph tags
    paragraph_tag(str);
    // add html id for links
    EnvLabel(labels, label_orders, entry, str);
    // replace environments with html tags
    equation_tag(str, U"equation"); equation_tag(str, U"align"); equation_tag(str, U"gather");
    // itemize and enumerate
    Itemize(str); Enumerate(str);
    // process table environments
    Table(str);
    // ensure '<' and '>' has spaces around them
    EqOmitTag(str);
    // process example and exercise environments
    theorem_like_env(str);
    // process figure environments
    FigureEnvironment(img_ids, img_orders, img_hashes, str, entry);
    // get dependent entries from \pentry{}
    get_pentry(pentries, str, db_read);
    // issues environment
    issuesEnv(str);
    addTODO(str);
    // process \pentry{}
    pentry(str);
    // replace user defined commands
    newcommand(str, rules);
    subsections(str);
    // replace \name{} with html tags
    Command2Tag(U"subsubsection", U"<h3><b>", U"</b></h3>", str);
    Command2Tag(U"bb", U"<b>", U"</b>", str); Command2Tag(U"textbf", U"<b>", U"</b>", str);
    Command2Tag(U"textsl", U"<i>", U"</i>", str);
    pay2div(str); // deal with "\pay" "\paid" pseudo command
    // replace \upref{} with link icon
    upref(str, entry);
    href(str); // hyperlinks
    cite(str, db_read); // citation
    
    // footnote
    footnote(str, entry, gv::url);
    // delete redundent commands
    replace(str, U"\\dfracH", U"");
    // remove spaces around chinese punctuations
    rm_punc_space(str);
    // verbatim recover (in inverse order of `verbatim()`)
    lstlisting(str, str_verb);
    lstinline(str, str_verb);
    verb(str, str_verb);

    Command2Tag(U"x", U"<code>", U"</code>", str);
    // insert body Title
    if (replace(html, U"PhysWikiTitle", title) != 1)
        throw internal_err(u8"\"PhysWikiTitle\" 在 entry_template.html 中数量不对");
    // insert HTML body
    if (replace(html, U"PhysWikiHTMLbody", str) != 1)
        throw internal_err(u8"\"PhysWikiHTMLbody\" 在 entry_template.html 中数量不对");
    if (replace(html, U"PhysWikiEntry", entry) != (gv::is_wiki? 8:2))
        throw internal_err(u8"\"PhysWikiEntry\" 在 entry_template.html 中数量不对");

    last_next_buttons(html, db_read, order, entry, title);

    if (replace(html, U"PhysWikiAuthorList", db_get_author_list(entry, db_read)) != 1)
        throw internal_err(u8"\"PhysWikiAuthorList\" 在 entry_template.html 中数量不对");

    // save html file
    write(html, gv::path_out + entry + ".html.tmp");

    // update db "entries"
    Str32 key_str; join(key_str, keywords, U"|");
    Str32 pentry_str; join(pentry_str, pentries);
    if (title != db_title || key_str != db_keys_str
        || draft != db_draft || pentry_str != db_pentry_str) {
        SQLite::Statement stmt_update
                (db,
                 R"(UPDATE "entries" SET "caption"=?, "keys"=?, "draft"=?, "pentry"=? )"
                 R"(WHERE "id"=?;)");
        stmt_update.bind(1, u8(title));
        stmt_update.bind(2, u8(key_str));
        stmt_update.bind(3, (int) draft);
        stmt_update.bind(4, u8(pentry_str));
        stmt_update.bind(5, u8(entry));
        stmt_update.exec(); stmt_update.reset();
    }
}

inline void db_check_add_entry_simulate_editor(Str32_I entry, SQLite::Database &db_read)
{
    if (!exist(db_read.getHandle(), "entries", "id", u8(entry))) {
        SLS_WARN(U"词条不存在数据库中， 将模拟 editor 添加： " + entry);
        // 从 tex 文件获取标题
        Str32 str;
        read(str, gv::path_in + "contents/" + entry + ".tex"); // read tex file
        CRLF_to_LF(str);
        Str32 title; get_title(title, str);
        // 写入数据库
        SQLite::Database db(u8(gv::path_data + "scan.db"), SQLite::OPEN_READWRITE);
        SQLite::Statement stmt_insert(db,
            R"(INSERT INTO "entries" ("id", "caption", "draft") VALUES (?, ?, 1);)");
        stmt_insert.bind(1, u8(entry));
        stmt_insert.bind(2, u8(title));
        stmt_insert.exec(); stmt_insert.reset();
    }
}

// like PhysWikiOnline, but convert only specified files
// requires ids.txt and labels.txt output from `PhysWikiOnline()`
inline void PhysWikiOnlineN(vecStr32_I entries)
{
    vecStr32 titles(entries.size());

    {
        SQLite::Database db_read(u8(gv::path_data + "scan.db"), SQLite::OPEN_READONLY);
        SQLite::Statement stmt_select(
                db_read,
                R"(SELECT "caption", "keys", "pentry", "draft", "issues", "issueOther" from "entries" WHERE "id"=?;)");

        vecStr32 rules;  // for newcommand()
        define_newcommands(rules);

        cout << u8"\n\n======  第 1 轮转换 ======\n" << endl;
        Bool isdraft;
        vecStr32 keywords;
        vecLong img_orders, label_orders;
        vecStr32 img_ids, img_hashes, labels, pentries;

        for (Long i = 0; i < size(entries); ++i) {
            auto &entry = entries[i];

            cout << std::left << entries[i] << std::setw(20); cout.flush();

            db_check_add_entry_simulate_editor(entry, db_read);
            PhysWikiOnline1(titles[i], img_ids, img_orders, img_hashes, isdraft,
                            keywords, labels, label_orders, pentries, entry,
                            rules, db_read);
            
            cout << std::left << titles[i] << endl; cout.flush();

            {
                SQLite::Database db_rw(u8(gv::path_data + "scan.db"), SQLite::OPEN_READWRITE);
                db_update_labels({entry}, {labels}, {label_orders}, db_rw);
                db_update_figures({entry}, {img_ids}, {img_orders},
                                  {img_hashes}, db_rw);
            }

            // check dependency tree
            {
                vector<DGnode> tree;
                vecStr32 _entries, _titles, parts, chapters;
                db_get_tree1(tree, _entries, _titles, parts, chapters, entry, db_read);
            }
        }
    }

    {
        cout << "\n\n\n\n" << u8"====== 第 2 轮转换 ======\n" << endl;
        SQLite::Database db_rw(u8(gv::path_data + "scan.db"), SQLite::OPEN_READWRITE);
        Str32 html, fname;
        for (Long i = 0; i < size(entries); ++i) {
            auto &entry = entries[i];
            cout << std::setw(5) << std::left << i
                 << std::setw(10) << std::left << entry
                 << std::setw(20) << std::left << titles[i] << endl; cout.flush();
            fname = gv::path_out + entry + ".html";
            read(html, fname + ".tmp"); // read html file
            // process \autoref and \upref
            autoref(html, entry, db_rw);
            write(html, fname); // save html file
            file_remove(u8(fname) + ".tmp");
        }
        cout << endl; cout.flush();
    }
}

// --titles
inline void arg_titles()
{
    // update entries.txt and titles.txt
    vecStr32 titles, entries, isDraft;
    entries_titles(titles, entries);
    write_vec_str(titles, gv::path_data + U"titles.txt");
    write_vec_str(entries, gv::path_data + U"entries.txt");
}

// --toc
// table of contents
// generate index.html from main.tex
inline void arg_toc()
{
    vecStr32 titles, entries;
    SQLite::Database db(u8(gv::path_data + "scan.db"), SQLite::OPEN_READWRITE);
    vecBool is_draft;
    vecStr32 part_ids, part_name, chap_first, chap_last, chap_ids, chap_name, entry_first, entry_last;
    vecLong entry_part, chap_part, entry_chap;
    table_of_contents(part_ids, part_name, chap_first, chap_last,
                      chap_ids, chap_name, chap_part, entry_first, entry_last,
                      entries, titles, is_draft, entry_part, entry_chap, db);
    db_update_parts_chapters(part_ids, part_name, chap_first, chap_last, chap_ids, chap_name, chap_part,
                             entry_first, entry_last);
    db_update_entries_from_toc(entries, entry_part, part_ids, entry_chap, chap_ids);
}

// --bib
inline void arg_bib()
{
    // process bibliography
    vecStr32 bib_labels, bib_details;
    bibliography(bib_labels, bib_details);
    db_update_bib(bib_labels, bib_details);
}

// --history
// update db "history" table from backup files
inline void arg_history(Str32_I path)
{
    SQLite::Database db(u8(gv::path_data + "scan.db"), SQLite::OPEN_READWRITE);
    db_update_author_history(path, db);
    db_update_authors(db);
}

// convert PhysWiki/ folder to wuli.wiki/online folder
// goal: should not need to be used!
inline void PhysWikiOnline()
{
    SQLite::Database db(u8(gv::path_data + "scan.db"), SQLite::OPEN_READWRITE);

    gv::is_entire = true;
    // remove matlab files
    vecStr fnames;
    ensure_dir(u8(gv::path_out) + "code/matlab/");
    file_list_full(fnames, u8(gv::path_out) + "code/matlab/");
    for (Long i = 0; i < size(fnames); ++i)
        file_remove(fnames[i]);

    // get entries from folder, titles from main.tex
    vecStr32 entries;

    {
        vecStr32 titles;
        entries_titles(entries, titles);
        write_vec_str(titles, gv::path_data + U"titles.txt");
        write_vec_str(entries, gv::path_data + U"entries.txt");

        SQLite::Statement stmt_insert(db,
            R"(INSERT OR IGNORE INTO "entries" ("id") VALUES (?);)");
        for (Long i = 0; i < size(entries); ++i) {
            stmt_insert.bind(1, u8(entries[i]));
            stmt_insert.exec();
            if (stmt_insert.getChanges() > 0)
                SLS_WARN(U"已插入数据库中不存在的词条（仅 id）： " + entries[i]);
            stmt_insert.reset();
        }
    }

    // for newcommand()
    vecStr32 rules;
    define_newcommands(rules);

    // process bibliography
    {
        vecStr32 bib_labels, bib_details;
        bibliography(bib_labels, bib_details);
        db_update_bib(bib_labels, bib_details);
    }

    // html tag id and corresponding latex label (e.g. Idlist[i]: "eq5", "fig3")
    // the number in id is the n-th occurrence of the same type of environment

    vecStr32 titles; // titles from files directly
    titles.resize(entries.size());

    {
        Bool draft;
        vecLong img_orders, label_orders;
        vecStr32 keywords, labels, img_ids, img_hashes, pentries;

        SQLite::Statement stmt_select(db,
                                      R"(SELECT "id", "type", "entry", "order" FROM "labels";)");
        SQLite::Statement stmt_insert(db,
                                      R"(INSERT INTO "labels" ("id", "type", "entry", "order") VALUES (?,?,?,?);)");
        SQLite::Statement stmt_update(db,
                                      R"(UPDATE "labels" SET "type"=?, "entry"=?, "order"=? WHERE "id"=?;)");
        SQLite::Statement stmt_select_fig(db,
                                              R"(SELECT "entry", "order", "hash" FROM "figures" WHERE "id"=?;)");

        SQLite::Statement stmt_insert_fig(db,
                                              R"(INSERT INTO "figures" ("id", "entry", "order", "hash") VALUES (?, ?, ?, ?);)");

        SQLite::Statement stmt_update_fig(db,
                                              R"(UPDATE "figures" SET "entry"=?, "order"=?, "hash"=?; WHERE "id"=?;)");

        // 1st loop through tex files
        cout << u8"\n\n\n\n======  第 1 轮转换 ======\n" << endl;
        for (Long i = 0; i < size(entries); ++i) {
            cout << std::setw(5) << std::left << i
                 << std::setw(10) << std::left << entries[i]; cout.flush();

            PhysWikiOnline1(titles[i], img_ids, img_orders, img_hashes, draft,
                            keywords, labels, label_orders, pentries, entries[i],
                            rules, db);

            cout << std::setw(20) << std::left << titles[i] << endl; cout.flush();

            db_update_labels({entries[i]}, {labels}, {label_orders}, stmt_select, stmt_insert, stmt_update);
            db_update_figures({entries[i]}, {img_ids}, {img_orders}, {img_hashes}, stmt_select_fig, stmt_insert_fig, stmt_update_fig);
        }
    }

    cout << u8"\n\n\n\n\n正在从 main.tex 生成目录 index.html ...\n" << endl;
    {
        vecBool is_drafts;
        vecLong entry_part, entry_chap, chap_part;
        vecStr32 entries_toc, titles_toc;
        vecStr32 part_ids, part_name, chap_first, chap_last; // \part{}
        vecStr32 chap_ids, chap_name, entry_first, entry_last; // \chapter{}

        table_of_contents(part_ids, part_name, chap_first, chap_last,
                          chap_ids, chap_name, chap_part, entry_first, entry_last,
                          entries_toc, titles_toc, is_drafts, entry_part, entry_chap, db);

        db_update_entries_from_toc(entries_toc, entry_part, part_ids, entry_chap, chap_ids);
        db_update_parts_chapters(part_ids, part_name, chap_first, chap_last, chap_ids, chap_name, chap_part,
                                 entry_first, entry_last);
    }

    // generate dep.json
    if (file_exist(gv::path_out + U"../tree/data/dep.json"))
        dep_json(db);

    // deal with autoref()
    {
        cout << "\n\n\n\n" << u8"====== 第 2 轮转换 ======\n" << endl;
        Str32 html, fname;
        SQLite::Statement stmt_select(db,
            R"(SELECT "order", "ref_by" FROM "labels" WHERE "id"=?;)");
        SQLite::Statement stmt_update_ref_by(db,
            R"(UPDATE "labels" SET "ref_by"=? WHERE "id"=?;)");
        SQLite::Statement stmt_select_fig(db,
            R"(SELECT "order", "ref_by" FROM "figures" WHERE "id"=?;)");
        SQLite::Statement stmt_update_ref_by_fig(db,
            R"(UPDATE "figures" SET "ref_by"=? WHERE "id"=?;)");

        for (Long i = 0; i < size(entries); ++i) {
            cout << std::setw(5) << std::left << i
                 << std::setw(10) << std::left << entries[i]
                 << std::setw(20) << std::left << titles[i] << endl; cout.flush();
            fname = gv::path_out + entries[i] + ".html";
            read(html, fname + ".tmp"); // read html file
            // process \autoref and \upref
            autoref(html, entries[i], stmt_select, stmt_update_ref_by, stmt_select_fig, stmt_update_ref_by);
            write(html, fname); // save html file
            file_remove(u8(fname) + ".tmp");
        }
        cout << endl;
    }

    // TODO: warn unused figures, based on "ref_by"

    if (!illegal_chars.empty()) {
        SLS_WARN("非法字符的 code point 已经保存到 data/illegal_chars.txt");
        ofstream fout("data/illegal_chars.txt");
        for (auto c: illegal_chars) {
            fout << Long(c) << endl;
        }
    }
}
