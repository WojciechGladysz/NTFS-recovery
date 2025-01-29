#pragma once

#include <fstream>
#include <vector>
#include <set>
#include <unordered_map>

#include "ntfs.hpp"

using namespace std;

enum class Time_t:uint64_t;

struct Context;

struct File
{
	pid_t	pid;
    bool	valid, done, used, exists, dir, error;
    LBA lba;
    uint32_t index;
    string name, ext, path;
    Time_t time, access;
    uint64_t size, alloc, magic;
    ofstream ofs;
    vector<pair<uint32_t, uint32_t>> runlist;
    vector<string> entries;
    const char* content;
    Context& context;
    static unordered_map<uint32_t, pair<string, uint32_t>> dirs;
    string getType() const;
    void mangle();
    bool open();
    bool hit(const set<string>&, bool);
    bool parse();
    operator bool() const { return valid; }
    File(LBA, const vector<char>&, struct Context&);
    void recover();
};

time_t convert(const Time);
ostream& operator<<(ostream& os, const Time time);
ostream& operator<<(ostream& os, const File&);
ifstream& operator>>(ifstream& os, File&);
