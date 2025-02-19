#include <iostream>
#include <sstream>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstring>

#include "helper.hpp"
#include "context.hpp"

bool Context::verbose = false;
bool Context::debug = false;
bool Context::confirm = false;

ostream& operator<<(ostream& oss, const Context& context) {
    oss << "Device:" << context.dev << ", "
        << "lba:" << outvar(context.first);
    if (context.last) oss << " >> " << outvar(context.last);
    oss << ", ";
    if (*context.count > 0) oss << "count:" << *context.count << ", ";
    if (*context.show > 0) oss << "process:" << *context.show << ", ";
    if (context.magic) {
        oss << "magic:" << hex << uppercase << 'x' << context.magic << '/';
        cerr.write(&context.cmagic, sizeof(context.magic)) << ", ";
    }
    if (!context.include.empty()) {
        oss << "include:";
        for (auto extension: context.include) oss << extension << ",";
        oss << ' ';
    }
    if (!context.exclude.empty()) {
        oss << "exclude:";
        for (auto extension: context.exclude) oss << extension << ",";
        oss << ' ';
    }
    oss << "max child:" << context.childs << ", ";
    oss << "big file:" << context.size / (1 << 20) << "MB, ";
    if (context.verbose) {
        if (!context.debug) oss << "verbose, ";
        else oss << "debug, ";
    }
    if (context.confirm) oss << "confirm, ";
    if (context.all) oss << "show all, ";
    else if (context.extra) oss << "show dirs, ";
    if (context.index) oss << "show index entries, ";

    if (context.recycle) oss << "include recycle bin, ";
    oss << "pid:" << getpid() << endl;
    if (context.recover) {
        oss << "RECOVER to target dir: " << context.dir;
        if (context.force) oss << ", overwrite existing files";
        oss << endl;
    }
    if (context.last && context.first > context.last) cerr << "End lba lower that start lba" << endl;
    if (context.recover) {
        cerr << endl << "\tPress enter to confirm...";
        cin.get();
    }
    return oss << endl;
}

void Context::signature(const char* key) {
    magic = strtoll(key, nullptr, 0);
    if (!magic) strncpy((char*)&magic, key, sizeof(magic));
    uint64_t temp = magic;
    while (temp) {
        mask =  (mask << 8) + 0xFF;
        temp = temp/0x100;
    }
    magic &= mask;
    // cerr << "Magic: " << hex << magic << '/'; cerr.write(&cmagic, sizeof(magic)) << endl;
}

Context::Context(): dir("."), sector(512), sectors(8) {
    first = last = bias = mft.first = mft.last = 0;
    magic = mask = 0;
    verbose = debug = confirm = recover = all = force = index = recycle = extra = false;
    size = 1 << 24;     // 16MB
    childs = 4;
    count = (int64_t*)mmap(NULL, sizeof(int64_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    *count = -1L;
    show = (int64_t*)mmap(NULL, sizeof(int64_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    *show = -1L;
    sem = (sem_t*)mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    sem_init(sem, 1, 4);
    mux = (mutex*)mmap(NULL, sizeof(mutex), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (dir.back() == '/') dir.pop_back();
    ifstream mime("/etc/mime.types");
    string line, type, extensions, file;
    if (mime.is_open())
        while(getline(mime, line))
            if (!line.empty() && line.front() != '#') {
                istringstream entry(line);
                getline(entry, type, '/');
                getline(entry, extensions);
                istringstream extension(extensions);
                extension >> file;
                while (extension >> file) this->mime[type].insert(file);
            }
}

void Context::parse(const string& types, set<string>& set) {
    istringstream iss(types);
    string name;
    while (getline(iss, name, ',')) set.insert(lower(name));
}
