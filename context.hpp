#pragma once
#include <fstream>
#include <semaphore.h>
#include <unordered_map>
#include <set>

using namespace std;
using LBA = uint64_t;

// enum class Verbose { Normal, Verbose, Debug };

struct Context {
    enum class Format{ None, Year, Month, Day };
    string          dev;    // device to scan and recover
    string			dir;	// recovery target directory
    LBA				first, last;
    int64_t         bias;
    int64_t			count, show;
    set<string>		include, exclude;
    union           { uint64_t magic; char cmagic; };
    uint64_t		mask;
    bool			recover, all, force, index, recycle, extra;
    uint            sector, sectors;
    static bool     verbose, debug, confirm;
    size_t          size;
	uint			childs;
    sem_t*          sem;
    unordered_map<string, set<string>>     mime;
    void parse(const string&, set<string>&);
    Format format;
    Context();
    bool stop(LBA lba) {
        if (!count--) return true;
        if (last) return !(lba < last);
        return false;
    } 
    ~Context() { sem_destroy(sem); };
    void signature(const char*);
    void dec() { if (!--show) count = 0; }
};

ostream& operator<<(ostream&, const Context&);
