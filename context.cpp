#include <iostream>
#include <sstream>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include "helper.hpp"
#include "context.hpp"

bool Context::verbose = false;
bool Context::debug = false;
bool Context::confirm = false;

ostream& operator<<(ostream& oss, const Context& context) {
    oss << "Device:" << context.dev << ", "
        << "lba:" << outvar(context.first);
    if (context.last) oss << '>' << outvar(context.last);
    oss << ", ";
    if (context.count > 0) oss << "count:" << context.count << ", ";
    if (context.show > 0) oss << "show:" << context.show << ", ";
    if (context.magic) oss << "magic:" << hex << uppercase << 'x' << context.magic << ", ";
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
    oss << "processes:" << context.childs << ", ";
    oss << "size:" << context.size / (1 << 20) << "MB, ";
    if (context.verbose) {
        if (!context.debug) oss << "verbose, ";
        else oss << "debug, ";
    }
    if (context.confirm) oss << "confirm, ";
    if (context.all) oss << "show all, ";
    if (context.format != Context::Format::None) {
        oss << "path:/yyyy/";
        if (context.format > Context::Format::Year) oss << "mm/";
        if (context.format > Context::Format::Month) oss << "dd/";
        oss << ", ";
    }
    oss << "pid:" << getpid() << endl;
    if (context.recover) oss << "RECOVER to target dir:" << context.dir;
    if (context.force) oss << ", overwrite existing files";
    if (context.last && context.first > context.last)
        cerr << "End lba lower that start lba";
    return oss << endl;
}

Context::Context(): dir("."), count(-1L), show(-1L), sector(512), sectors(8) {
    bias = first = last = 0;
    magic = 0;
    verbose = false;
    debug = false;
    confirm = false;
    recover = false;
    all = false;
    force = false;
    format = Context::Format::None;
    size = 1 << 24;
    childs = 4;
    sem = (sem_t*)mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    sem_init(sem, 1, 4);
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
