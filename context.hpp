#pragma once
#include <fstream>
#include <semaphore.h>
#include <unordered_map>
#include <set>
#include <mutex>

using namespace std;
using LBA = uint64_t;

// enum class Verbose { Normal, Verbose, Debug };

struct Context {
	enum class Format{ None, Year, Month, Day };
	string          dev;                    // name of device to scan and recover
	string			dir;	                // recovery target directory
	LBA				first, last;            // device/file first, last lba to scan
	int64_t         bias;                   // offset to partition calculated first lba
	struct { LBA first, last; } mft;        // mft file first, last lba
	int64_t         *count, *show;          // counters for limited output
	set<string>		include, exclude;       // file extensions to include/exclude
	union           { uint64_t magic; char cmagic; };   // file magic word
	uint64_t		mask;                   // magic word mask
	bool			recover, all, force, index, recycle, dirs;
	uint            sector, sectors;        // sector size, and sectors in cluster
	static bool     verbose, debug, confirm;
	size_t          size;                   // min. size of a file to fork for processing
	uint			childs;                 // max. no. of childs for big file processing
	sem_t*          sem;                    // semaphore to keep the no. o childs
	mutex*          mux;                    // mutex for counters
	Format          format;
	unordered_map<string, set<string>>     mime;    // file extensions parsed from /etc/mime
	void parse(const string&, set<string>&);
	Context();
	bool stop(LBA lba) {
		if ((*count)-- == 0) return true;
		if (last) return !(lba < last);
		return false;
	}
	~Context() { sem_destroy(sem); };
	void signature(const char*);
	int64_t dec() {
		lock_guard<mutex> lock(*mux);
		(*show)--;
		if ((*show) == 0) *count = 0;
		return *show;
	}
	bool noExt() { return include.empty() && exclude.empty(); }
};

ostream& operator<<(ostream&, const Context&);
