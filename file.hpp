#pragma once

#include "helper.hpp"

#include <fstream>
#include <map>
#include <set>
#include <unordered_map>

using VCN = uint64_t;
enum class Time_t: uint64_t;

struct Context;
struct Record;

struct Run {
	size_t count;
	std::vector<std::pair<VCN, VCN>> list;
};

struct File
{
	pid_t	pid;
	bool	valid, done, used, exists, dir, error;
	LBA		lba;
	uint64_t index, parent;
	std::string	name, ext, path;
	Time_t	time, access;
	uint64_t size, alloc, mask, entry;
	union { uint64_t magic; char    cmagic; };
	std::ofstream ofs;
	std::map<VCN, Run> runlist;
	std::vector<std::pair<uint64_t, std::string>> entries;
	const char* content;
	Context& context;
	static	std::unordered_map<uint64_t, std::pair<std::string, uint64_t>> dirs;
	std::string	getType() const;

	void mangle();
	bool open();
	bool hit(const std::set<std::string>&, bool);
	bool parse();
	bool empty() const;
	operator bool() const { return valid; }
	bool setBias(const Record*) const;
	bool setPath(const Record*);
	void mapDir(const Record*, uint64_t);
	File(LBA, const Record*, struct Context&);
	void recover();
};

std::ostream& operator<<(std::ostream& os, const File&);
std::ifstream& operator>>(std::ifstream& os, File&);
