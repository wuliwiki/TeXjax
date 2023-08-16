// global variables, must be set only once
#pragma once

#include "../SLISC/file/sqlite_ext.h"
#include "../SLISC/str/str.h"
#include "../SLISC/algo/graph.h"

using namespace slisc;

namespace gv {
	Str path_in; // e.g. ../PhysWiki/
	Str path_out; // e.g. ../littleshi.cn/online/
	Str path_data; // e.g. ../littleshi.cn/data/
	Str url; // e.g. https://wuli.wiki/online/
	Bool is_wiki; // editing wiki or note
	Bool is_eng = false; // use english for auto-generated text (Eq. Fig. etc.)
	Bool is_entire = false; // running one tex or the entire wiki
}

Str sb, sb1; // string buffer

class scan_err : public std::exception
{
private:
	Str m_msg;
public:
	explicit scan_err(Str msg): m_msg(std::move(msg)) {}

	const char* what() const noexcept override {
		return m_msg.c_str();
	}
};

// internal error to throw
class internal_err : public scan_err
{
public:
	explicit internal_err(Str_I msg): scan_err(u8"内部错误（请联系管理员）： " + msg) {}
};

// write log
void scan_log(Str_I str, bool print_time = false)
{
	// write to file
	const char *log_file = "scan_log.txt";
	ofstream file(log_file, std::ios::app);
	if (!file.is_open())
		throw internal_err(Str("Unable to open ") + log_file);
	// get time
	if (print_time) {
		time_t time = std::time(nullptr);
		std::tm *ptm = localtime(&time);
		stringstream ss;
		ss << std::put_time(ptm, "%Y-%m-%d %H:%M:%S");
		static Str time_str = std::move(ss.str());
		file << time_str << "  ";
	}
	file << str << endl;
	file.close();
}