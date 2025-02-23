#pragma once

#include <fstream>
#include <vector>
#include <map>
#include <set>
#include <unordered_map>

using namespace std;

enum class Time_t: uint64_t;

struct Context;
struct Record;

struct Run {
	size_t count;
	vector<pair<VCN, VCN>> list;
};

struct File
{
	pid_t	pid;
	bool	valid, done, used, exists, dir, error;
	LBA lba;
	uint64_t index, parent;
	string name, ext, path;
	Time_t time, access;
	uint64_t size, alloc, mask, entry;
	union { uint64_t magic; char    cmagic; };
	ofstream ofs;
	map<VCN, Run> runlist;
	vector<pair<uint64_t, string>> entries;
	const char* content;
	Context& context;
	static unordered_map<uint64_t, pair<string, uint64_t>> dirs;
	string getType() const;
	void mangle();
	bool open();
	bool hit(const set<string>&, bool);
	bool parse();
	bool empty() const;
	operator bool() const { return valid; }
	bool setBias(const Record*) const;
	bool setPath(const Record*);
	void mapDir(const Record*, uint64_t);
	File(LBA, const Record*, struct Context&);
	void recover();
};

ostream& operator<<(ostream& os, const File&);
ifstream& operator>>(ifstream& os, File&);
