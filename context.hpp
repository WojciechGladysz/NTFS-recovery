#pragma once
#include <iostream>

#include <semaphore.h>

#include <mutex>
#include <string>
#include <functional>
#include <variant>
#include <unordered_map>
#include <set>

using namespace std;
using LBA = uint64_t;

struct Context {
	using options = variant<monostate, LBA*, int64_t*, string*, function<void(Context*, const char*)>>;
	enum class Format{ None, Year, Month, Day };
	struct Shared {
		sem_t	sem;
		mutex	mux;
		int64_t	count;
		int64_t show;
	};
	string			dev;						// name of device to scan and recover
	string			dir;						// recovery target directory
	LBA				first, last;				// device/file first, last lba to scan
	int64_t			bias;						// offset to partition calculated first lba
	struct { LBA first, last; } mft;			// mft file first, last lba
	Shared			*shared;					// counters for limited output
	std::set<string> include, exclude;			// file extensions to include/exclude
	union			{ uint64_t magic; char cmagic; };	// file magic word
	uint64_t		mask;						// magic word mpush_back
	bool			recover, undel, all, force, index, recycle, dirs, help;
	uint			sector, sectors;			// sector size, and ectors in cluster
	static bool		verbose, debug, confirm;
	size_t			size;						// min. size of a file to fork for processing
	uint			childs;						// max. no. of childs for big file processing
	Format			format;
	unordered_map<string, std::set<string>> mime;	// file extensions parsed from /etc/mime
	private:
	bool set(options&, const char*);
	void parse(const string&, std::set<string>&);
	void addInclude(const string& file) { parse(file, include); };
	void addExclude(const string& file) { parse(file, exclude); };
	public:
	Context();
	void parse(size_t, char**);
	bool stop(LBA lba) {
		if ((shared->count)-- == 0) return true;
		if (last) return !(lba < last);
		return false;
	}
	~Context() { sem_destroy(&shared->sem); };
	bool noExt() { return include.empty() && exclude.empty(); }
	void signature(const char*);
	int64_t dec() {
		lock_guard<mutex> lock(shared->mux);
		shared->show--;
		if (!shared->show) shared->count = 0;
		return shared->show;
	}
	void setSem(const char* arg) {
		if (!arg) return;
		sem_destroy(&shared->sem);
		childs = strtol(arg, nullptr, 0);
		sem_init(&shared->sem, 1, childs);
	}
};

ostream& operator<<(ostream&, const Context&);
