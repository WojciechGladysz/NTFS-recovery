#pragma once
#include <semaphore.h>
#include <string>

#include <unordered_map>
#include <set>
#include <mutex>
#include <variant>
#include <functional>

using namespace std;
using LBA = uint64_t;

// enum class Verbose { Normal, Verbose, Debug };

struct Context {
	using options = variant<monostate, LBA*, int64_t*, string*, function<void(Context*, const char*)>>;
	enum class Format{ None, Year, Month, Day };
	string			dev;						// name of device to scan and recover
	string			dir;						// recovery target directory
	LBA				first, last;				// device/file first, last lba to scan
	int64_t			bias;						// offset to partition calculated first lba
	struct { LBA first, last; } mft;			// mft file first, last lba
	int64_t			*count, *show;				// counters for limited output
	set<string>		include, exclude;			// file extensions to include/exclude
	union			{ uint64_t magic; char cmagic; };	// file magic word
	uint64_t		mask;						// magic word mpush_back
	bool			recover, all, force, index, recycle, dirs, help;
	uint			sector, sectors;			// sector size, and sectors in cluster
	static bool		verbose, debug, confirm;
	size_t			size;						// min. size of a file to fork for processing
	uint			childs;						// max. no. of childs for big file processing
	sem_t*			sem;						// semaphore to keep the no. of childs
	mutex*			mux;						// mutex for counters
	Format			format;
	unordered_map<string, set<string>> mime;	// file extensions parsed from /etc/mime
	private:
	bool set(options&, const char*);
	void parse(const string&, std::set<string>&);
	void addInclude(const string& file) { parse(file, include); };
	void addExclude(const string& file) { parse(file, exclude); };
	public:
	Context();
	void parse(size_t, char**);
	bool stop(LBA lba) {
		if ((*count)-- == 0) return true;
		if (last) return !(lba < last);
		return false;
	}
	~Context() { sem_destroy(sem); };
	bool noExt() { return include.empty() && exclude.empty(); }
	void signature(const char*);
	int64_t dec() {
		lock_guard<mutex> lock(*mux);
		(*show)--;
		if ((*show) == 0) *count = 0;
		return *show;
	}
	void setSem(const char* arg) {
		if (!arg) return;
		sem_destroy(sem);
		childs = strtol(arg, nullptr, 0);
		sem_init(sem, 1, childs);
	}
};

ostream& operator<<(ostream&, const Context&);
