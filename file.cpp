#include <iostream>
#include <filesystem>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <utime.h>

#include "helper.hpp"
#include "context.hpp"
#include "attr.hpp"
#include "entry.hpp"
#include "file.hpp"

unordered_map<uint32_t, pair<string, uint32_t>> File::dirs;

ostream& operator<<(ostream& os, const Time_t time) {
    os << std::put_time(std::localtime(reinterpret_cast<const time_t*>(&time)), "%Y.%m.%d %H:%M:%S");
    return os;
}

bool File::hit(const set<string>& entries, bool must) {  // return means continue checking
    if (!valid) return false;           // no further checking
    if (entries.empty()) return true;   // continue other checking
    valid = !must;
    for (auto extension: entries) {
        auto hit = context.mime.find(extension);
        if (hit != context.mime.end())
            for (auto type: hit->second) {
                if (type != ext) continue;
                valid = must;
                return false;
            }
        if (ext == extension) {
            valid = must;
            return false;
        }
    }
    return false;
}

string File::getType() const {
    if (!used) return "not used";
    if (!valid) return "not valid";
    if (dir) return "DIR";
    if (error) return "ERROR";
    if (runlist.empty() && !content) return "empty";
    if (context.magic && context.magic != magic) return "no magic";
    if (!context.force && exists) return "exist";
    return "file";
}

ostream& operator<<(ostream& os, const File& file) {
    // cerr << "File/" << file.index << ':' << file.valid << file.dir << file.exists << !!file.content << !file.runlist.empty() << endl;
    os << "\033[2K\r";     // just print file basic info and return to line begin
    os << hex << uppercase << 'x' << file.lba << tab
        << file.getType() << '/' << dec << file.index
        << tab << file.path << file.name;
    if (!file.done) {
        os << "...\r";     // just print file basic info and return to line begin
        return os.flush();
    }
    if (!file.context.all) {
        if (!file.valid || file.exists) return os;
        if (!file.content && file.runlist.empty()) return os;
    }
    file.context.dec();
    os << tab << file.time << tab;
    if (!file.content) {
        if (file.size) {
            os << "size:" << (file.size > 1024? file.size/1024: file.size);
            if (file.size > 1024) os << 'k';
            os << tab;
            if (!file.runlist.empty())
                // LCN * sector size * sectors per cluster
                for (auto run: file.runlist)
                    // os << outpaix(run.first, run.second) << tab;
                    os << outpaix(run.first * file.context.sectors, run.second * file.context.sectors) << tab;
            // os << "max:" << outvar(Runlist::maxLcn * file.context.sectors) << tab;
        }
    }
    else os << "resident" << tab;

    if (file.dir && !file.entries.empty()) {
        os << endl;
        for (string entry: file.entries) os << tab << entry;
    }

    return os << endl;
}

File::File(LBA lba, const Record* record, Context& context):
        context(context), error(false), pid(-1),
        valid(false), lba(lba), dir(false), size(0), alloc(0),
        content(nullptr), done(false), exists(false)
{
    if (!record) return;
    uint32_t* endTag = (uint32_t*)(record->key + record->size) - 2;
    if (!record) return;
    used = record->used();
    if (!used) return;
    if (*endTag != 0xFFFFFFFF) return;
    index = record->rec;
    const Attr* attrs = (const Attr*)(record->key + record->attr);
    if (record->dir()) {
        string name;
        dir = true;
        uint16_t parent = attrs->getDir(name);
        if (dirs.end() == dirs.find(record->rec)) {
            dirs[record->rec] = make_pair(name, parent);
            if (context.extra) {
                if (!context.dirs.is_open()) context.dirs.open("ntfs.dirs", ios::binary);
                if (context.dirs.is_open()) context.dirs << record->rec << tab << name << tab << parent << endl;
            }
        }
    }
    const Attr* next = attrs;
    while (next) next = next->parse(this); 
    if (!index) {
        if (context.verbose && runlist.empty()) {
            cerr << "No runlist in $MFT file" << endl;
            confirm();
        }
        context.bias = runlist[0].first * context.sectors - lba;
        cerr << "New context LBA bias based on last $MFT record: " << outvar(context.bias) << endl;
    }
    hit(context.include, true);
    hit(context.exclude, false);
}

void File::recover() {
    cout << *this;
    if (context.recover && size > context.size) {
        sem_wait(context.sem);
        pid = fork();
        if (pid < 0) {
            cerr << "Failed to create read process, error: " << strerror(errno) << endl;
            cerr << "Continue in main: " << name << endl; 
            sem_post(context.sem);
        }
        else if (pid) {    // parent process, return
            int sem;
            sem_getvalue(context.sem, &sem);
            if (context.verbose) cerr << "New child/" << sem << ':' << pid << '/' << index << ',' << endl;
            return;
        }
    }

    ifstream idev(context.dev, ios::in | ios::binary);
    if (idev.is_open()) idev >> *this;
    else error = true;

    cout << *this;
    if (!pid) {
        sem_post(context.sem);
        exit(EXIT_SUCCESS);
    }
}

ifstream& operator>>(ifstream& ifs, File& file) {
    string full;
    utimbuf times;
    file.done = true;
    if (!file.context.recover) return ifs;
    if (file.dir || !file.used || !file.valid) return ifs;
    vector<char> buffer(file.context.sector * file.context.sectors);
    streamsize save, bytes = file.size;
    uint64_t magic, mask = 0;
    magic = file.context.magic;
    while (magic) {
        mask =  (mask << 8) + 0xFF;
        magic = magic/0x100;
    }
    if (!file.runlist.empty())
        for (auto run: file.runlist) {
            LBA first = run.first * file.context.sectors;
            if (first < file.context.bias) {
                cerr << "Runlist LBA negative. Try scanning disk device not partition. Aborting" << endl;
                file.error = true;
                confirm;
                return ifs;
            }
            else first -= file.context.bias;
            ifs.seekg(first * file.context.sector);
            for (auto lcn = run.first; lcn < run.second; lcn++) {
                if (!ifs.read(buffer.data(), buffer.size())) {
                    if (file.context.verbose) cerr << "Error reading: " << outvar(lcn) << ", error: " << strerror(errno) << endl;
                    file.error = true;
                    goto out;
                }
                file.magic = *reinterpret_cast<uint64_t*>(buffer.data());
                if (!file.ofs.is_open()) {
                    if ((file.magic & mask) != (file.context.magic & mask)) {
                        if (Context::verbose) cerr << "No magic: " << outpaix((file.magic & mask), (file.context.magic & mask)) << endl;
                        file.valid = false;
                        goto out;
                    }
                    else if (!file.open()) {
                        if (!file.done) file.error = false;
                        return ifs;
                    }
                }
                auto save = bytes/buffer.size()? buffer.size(): bytes % buffer.size();
                file.ofs.write(buffer.data(), save);
                bytes -= save;
                if (bytes > file.size)
                    cerr << "Bytes left math error: " << outpair(bytes, file.size) << endl;
            }
        }
    else if(file.content) {
        file.magic = *reinterpret_cast<const uint16_t*>(file.content);
        if (file.context.magic && (file.context.magic & mask) != (file.magic & mask) )
            file.valid = false;
        else if (!file.open()) return ifs;
        else file.ofs.write(file.content, file.size);
    }
    else {
        file.done = true;
        return ifs;
    }
    file.done = true;
out:
    if (file.ofs.is_open()) file.ofs.close();
    else return ifs;

    full = file.context.dir + file.path + file.name;
    times = {(time_t)file.access, (time_t)file.time};
    if (utime(full.c_str(), &times)) {
        cerr << "Failed to update file time modification: " << full << ", error: " << strerror(errno) << endl;
        confirm();
    }
    return ifs;
}

void File::mangle() {
    if (context.format == Context::Format::None) return;
    tm* te;
    te = localtime(reinterpret_cast<const time_t*>(&time));
    ostringstream path;
    path << '/' << te->tm_year + 1900 << '/';
    if (context.format > Context::Format::Year) {
        path << setfill('0') << setw(2) << (te->tm_mon + 1) << '/';
        if (context.format > Context::Format::Month)
            path << setfill('0')<< setw(2) << (te->tm_mday) << '/' ;
    }
    this->path = path.str();
}

bool File::open()
{
    string target(context.dir);
    mangle();
    target.append(path);
    string full = target + name;
    if (filesystem::exists(full)) {     // if file exists...
        struct stat info;
        stat(full.c_str(), &info);
        if (context.verbose) cerr << "File exists: " << full;
        if (size <= info.st_size && !context.force) {   // check file size against MFT record
            if (context.verbose) cerr << ", and its size is OK. Skipping" << endl;
            done = true;
            exists = true;
            return false;
        }
        if (context.verbose) cerr << ", but will be overwritten" << endl;
        exists = false;
    }
    else if (!filesystem::exists(target)) {
        if (context.verbose) cerr << "Creating file target directory: " << target << endl;
        if (!filesystem::create_directories(target)) {
            if (errno != 17) {
                cerr << "Failed to create directory: " << target
                    << ", error/" << (int)errno << ": " << strerror(errno) << endl;
                confirm();
                return false;
            }
        }
        else if (!filesystem::is_directory(target)) {
            cerr << "The path exists and is not a directory: " << target << endl;
            confirm();
            return false;
        }
    }

    ofstream file(full, std::ios::out | std::ios::binary);
    if (!file.is_open()) {
        cerr << "Can not open file for write: " << full
            << ", error: " << strerror(errno) << endl;
        confirm();
        return false;
    }
    if (context.verbose) cerr << "File opened for write: " << full << endl;

    ofs = std::move(file);
    return true;
}
