#include <unordered_map>
#include <iomanip>
#include <cstring>
#include <codecvt>
#include <functional>
#include <fstream>
#include <filesystem>
#include <stdexcept>
#include <utime.h>
#include <unistd.h>
#include <cerrno>
#include <sys/stat.h>
#include <sys/mman.h>

#include "ntfs.hpp"

#define tab "\t"

const uint64_t NTFS_TO_UNIX_EPOCH = 11644473600ULL; // Difference in seconds
const uint64_t INTERVALS_PER_SECOND = 10000000ULL;  //

using namespace std;


static unordered_map<uint32_t, pair<string, uint32_t>> dirs;
static unordered_map<string, set<string>> mime;
static set<string> exts;

void ldump(const void* data, uint length, uint start = 0) {
    if (!Context::debug) return;
    uint end = start + length;
    string txt; // for ASCII display
    const auto align = 32;
    auto offset(0);
    const char* chars = static_cast<const char*>(data);
    for (uint i = start; i < end; i++) {
        uint8_t c = chars[i];
        if (!(offset % align)) cout << setw(4) << dec << offset << 'x' << setw(3) << uppercase << hex << offset << ": ";
        cout << std::hex << std::setw(2) << static_cast<uint16_t>(c) << ' ';
        txt.push_back(isprint(c)? c: ' ');
        offset++;
        if (!(offset % 8) && (offset % align)) {
            cout << '|';
            txt.push_back('|');
        }
        if (!(offset % align)) {
            cout << '\t' << txt << endl;
            txt.clear();
        }
    }
    if (length > align) while (offset % align) {
        offset++;
        cout << "_ _";
        if (!(offset % 8) && (offset % align))
            cout << '|';
        if (!(offset % align))
            cout << '\t' << txt << endl;
    }
    else cout << tab << txt << endl;
};

void pdump(const void* start, const void* end) {
    uint length = reinterpret_cast<const char*>(end) - reinterpret_cast<const char*>(start);
    ldump(start, length);
}

void dump(LBA lba, const vector<char>& data) {
    if (!Context::debug) return;
    cout << "offset: " << outvar(lba) << endl;
    ldump(data.data(), (uint)data.size()); 
}

void confirm() {
    if (!Context::confirm) return;
    cerr << "\tPress a key to continue...";
    cin.get();
}

enum class AttrId: __attribute__ ((packed)) uint32_t
{
    StandardInfo = 0x10,
    AttributeList = 0x20,
    FileName = 0x30,
    ObjectId = 0x40,
    SecurityDescriptor = 0x50,
    VolumeName = 0x60,
    VolumeInformation = 0x70,
    Data = 0x80,
    IndexRoot = 0x90,
    IndexAllocation = 0xA0,
    Bitmap = 0xB0,
    ReparsePoint = 0xC0,
    EAInformation = 0xD0,
    EA = 0xE0,
    PropertySet = 0xF0,
    LoggedUtilityStream = 0x100,
    End = 0xFFFFFFFF
};

unordered_map<const AttrId, const string> attrName {
    { AttrId::StandardInfo, "STANDARD INFO" },
    { AttrId::AttributeList, "ATTRIBUTE LIST" },
    { AttrId::FileName, "FILE NAME" },
    { AttrId::ObjectId, "OBJECT ID" },
    { AttrId::SecurityDescriptor, "SECURITY DESCRIPTOR" },
    { AttrId::VolumeName, "VOLUME NAME" },
    { AttrId::VolumeInformation, "vOLUME INFORMATION" },
    { AttrId::Data, "DATA" },
    { AttrId::IndexRoot, "INDEX ROOT" },
    { AttrId::IndexAllocation, "INDEX ALLOCATION" },
    { AttrId::Bitmap, "BITMAP" },
    { AttrId::ReparsePoint, "REPARSE POINT" },
    { AttrId::EAInformation, "EA INFORMATION" },
    { AttrId::EA, "EA" },
    { AttrId::PropertySet, "PROPERTY SET" },
    { AttrId::LoggedUtilityStream, "LOGGED UTILITY STREAM" }
};

time_t convertTime(const Time time) {
    time_t timestamp = (uint64_t)time/INTERVALS_PER_SECOND - NTFS_TO_UNIX_EPOCH;
    return timestamp;
}

ostream& operator<<(ostream& os, const Time_t time) {
    os << std::put_time(std::localtime(reinterpret_cast<const time_t*>(&time)), "%Y.%m.%d %H:%M:%S");
    return os;
}

ostream& operator<<(ostream& os, const Time time) {
    time_t timestamp = convertTime(time);
    os << std::put_time(std::localtime(&timestamp), "%Y.%m.%d %H:%M:%S");
    return os;
}

bool StandardInfo::parse(File* file) const {
    file->time = static_cast<Time_t>(convertTime(changeTime));
    file->access = static_cast<Time_t>(convertTime(accessTime));
    return true;
}

const uint8_t Boot::jmp[] = {0xeb, 0x52, 0x90};

Boot::operator bool() const {
    return !memcmp(jmpCode, jmp, 3)
        && !strncmp(oemId, "NTFS", 4)
        && endTag == 0xAA55;
}

Attr* Attr::getNext() const {
    Attr* next = (Attr*)((char*)this + (size & 0xFFFF));
    // if (size > 0xFFFF) cerr << "Bad offset to next attr. using 0XFFFF mask" << endl;
    if (next->type == AttrId::End) return nullptr;
    if ((uint16_t)next->type && next->size)
        return (Attr*)((char*)this + (size & 0xFFFF));
    cerr << "Next attribute corrupted: " << outpair((uint16_t)next->type, next->size) << endl;
    confirm();
    return nullptr;
}

uint16_t Attr::getDir(string& name) const {
    uint16_t dir = 0;
    const char* data = nullptr;
    const Attr* next = this;
    while (next) {
        if (next->type == AttrId::FileName) {
            data = reinterpret_cast<const char*>(next) + next->res.offset;
            dir = reinterpret_cast<const FileName*>(data)->getDir(name);
        }
        next = next->getNext();
    }
    return dir;
}

Entry::operator bool() const {
    return !strncmp(key, "FILE", 4);
}

Index::operator bool() const {
    return !strncmp(key, "INDX", 4);
}

ostream& operator<<(ostream& os, const StandardInfo* attr) {
    os << "creation: " << outtime(attr->creatTime) << endl
        << "change: " << outtime(attr->changeTime) << endl
        << "write: " << outtime(attr->writeTime) << endl
        << "access: " << outtime(attr->accessTime) << endl
        << "attr: " << outvar(attr->attr) << endl;
    return os;
}

ostream& operator<<(ostream& os, const Resident attr) {
    os << "Resident/" << outpaix(attr.length, attr.offset) << tab;
    ldump(&attr, sizeof(attr));
    return os;
}

bool Resident::parse(File* file) const {
    return true;
}

ostream& operator<<(ostream& os, const NonResident attr) {
    os << "Nonresident" << endl;
    ldump(&attr, sizeof(attr));
    os << "first: " << outvar(attr.first) << tab
        << "last: " << outvar(attr.last) << tab
        << "size: " << outvar(attr.size) << tab
        << "alloc: " << outvar(attr.alloc) << tab
        << "used: " << outvar(attr.used) << endl;
    return os;
}

bool NonResident::parse(File* file) const {
    file->size = used & 0xFFFFFFFFFFFF;
    file->alloc = alloc & 0xFFFFFFFFFFFF;
    return true;
}

std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;

string FileName::getName() const {
    string name;
    name = converter.to_bytes(&data, &data + length);
    for (char& c:name) if (!isprint(c)) c = '_';
    return name;
}

string& lower(string& text) {
    for (auto& c: text)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

bool FileName::parse(File* file) const {
    string path = "/";
    uint16_t dir = this->dir;
    try {
        string name;
        uint16_t last = 0;
        do {
            last = dir;
            tie(name, dir) = dirs.at(dir);
            if (dir == last) break;
            path = "/" + name + path;
        } while (true);
    }
    catch (...) {
        path = "/@" + to_string(dir) + path;
    }
    file->name = getName();
    if (file->name.npos != file->name.find('.')) {
        istringstream ext(file->name);
        while (getline(ext, file->ext, '.'));
    }
    if (!file->ext.empty()) {
        lower(file->ext);
        if (file->ext.npos == file->ext.find(' ')
                && exts.end() == exts.find(file->ext)) {
            exts.insert(file->ext);
            if (file->context.extra) {
                if (!file->context.extFile.is_open()) file->context.extFile.open("ntfs.exts", ios::binary);
                if (file->context.extFile.is_open()) {
                    file->context.extFile << file->ext << ',';
                    file->context.extFile.flush();
                }
            }
        }
    }
    file->path = path;
    file->valid = true;
    return true;
}

ostream& operator<<(ostream& os, const Attr* attr) {
    os << attr->id << "." << attrName[attr->type] << '/' << outpaix((uint)attr->type, attr->size) << tab;
    pdump(attr, &attr->res);

    if (attr->noRes) os << attr->nonRes;
    else os << attr->res;

    if (attr->length) { // alternate data stream
        const char16_t* w_name = reinterpret_cast<const char16_t*>((char*)attr + attr->offset);
        os << "Attribute/" << outpair((uint)attr->length, attr->offset)
            << ": " <<  converter.to_bytes(w_name, w_name + attr->length) << tab;
        ldump(w_name, sizeof(*w_name) * attr->length);
    } else if (!Context::debug) os << endl;

    if (attr->noRes) {
        auto data = reinterpret_cast<const char*>(attr) + attr->nonRes.runlist;
        ldump(data, attr->size - attr->nonRes.runlist);
        os << (Runlist*)data;
    }
    else {
        auto data = reinterpret_cast<const char*>(attr) + attr->res.offset;
        if (attr->type == AttrId::StandardInfo) os << reinterpret_cast<const StandardInfo*>(data);
        else if (attr->type == AttrId::FileName) os << reinterpret_cast<const FileName*>(data);
        else if (attr->type == AttrId::IndexRoot) os << reinterpret_cast<const IndexRoot*>(data);
        else if (false && attr->type == AttrId::Data && attr->res.length) {
            os << "File content: " << endl;
            os.write(data, attr->res.length);
            os << endl << "EOF" << endl;
        }
    }

    return os;
}

Attr* Attr::parse(File* file) const {
    if (noRes) {
        auto data = reinterpret_cast<const char*>(this) + nonRes.runlist;
        if (type == AttrId::Data && !length) { // no alternate data stream
            nonRes.parse(file);
            reinterpret_cast<const Runlist*>(data)->parse(file);
        }
        else if (file->dir && type == AttrId::IndexAllocation) {
            nonRes.parse(file);
            reinterpret_cast<const Runlist*>(data)->parse(file);
        }
    }
    else {
        res.parse(file);
        void* data = reinterpret_cast<void*>((char*)this + res.offset);
        if (type == AttrId::StandardInfo) reinterpret_cast<const StandardInfo*>(data)->parse(file);
        else if (type == AttrId::FileName) reinterpret_cast<const FileName*>(data)->parse(file);
        else if (type == AttrId::IndexRoot) reinterpret_cast<const IndexRoot*>(data)->parse(file);
        else if (type == AttrId::Data && !length && res.length) {
            file->size = res.length;
            file->content = reinterpret_cast<char*>(data);
        }
    }
    return getNext();
}

ostream& operator<<(ostream& os, const Runlist* attr) {
    uint64_t length;
    int64_t offset;
    if (!attr->lenSize) return os << endl;
    os << "runlist: ";
    while (attr->lenSize && attr->offSize) {
        uint64_t maskL = ((1LL<<(8*attr->lenSize))-1LL);
        int64_t shift = attr->run >> (8*attr->lenSize);
        auto maskO = ((1LL<<(8*attr->offSize))-1LL);
        length = attr->run & (maskL);
        offset = shift & maskO;
        os << outpair(attr->offSize, attr->lenSize) << ':';
        os << outpaix(offset, length) << '\t';
        attr = (Runlist*)((char*)attr + 1 + attr->lenSize + attr->offSize);
    }
    os << endl;
    return os;
}

uint64_t Runlist::minLcn = 0xFFFFFFFFFFFFFFFF;
uint64_t Runlist::maxLcn = 0;

bool Runlist::parse(File* file) const {
    uint64_t length;
    int64_t offset;
    uint64_t lcn0 = 0;
    uint64_t lcn = 0;
    if (file) file->runlist.clear();
    const Runlist* attr = this;
    while (attr->lenSize && attr->offSize) {
        if (attr->lenSize > 4 || attr->offSize > 4) {
            if (file) file->valid = false;
            return false;
        }
        uint64_t maskL = ((1LL<<(8*attr->lenSize))-1LL);
        int64_t shift = attr->run >> (8*attr->lenSize);
        auto maskO = ((1LL<<(8*attr->offSize))-1LL);
        length = attr->run & maskL;
        offset = shift & maskO;
        if (offset & (1LL<<(8*attr->offSize-1))) offset |= (0xFFFFFFFFFFFFFFFF << (8*attr->offSize));
        lcn0 += offset;
        lcn = lcn0 + length;
        if (lcn0 < minLcn) minLcn = lcn0;
        if (lcn > maxLcn) maxLcn = lcn;
        if (file) file->runlist.push_back(make_pair(lcn0, lcn));
        attr = (Runlist*)((char*)attr + 1 + attr->lenSize + attr->offSize);
    }
    return true;
}

ostream& operator<<(ostream& os, const Header* attr) {
    if (Context::verbose) os << "Header: ";
    pdump(attr, attr->node);
    if (Context::verbose) {
    os << "offset: " << outvar(attr->offset) << tab
        << "size: " << outvar(attr->size) << tab
        << "allocated: " << outvar(attr->allocated) << tab
        << "flags: " << outvar(attr->flags) << endl;
    }
    return os;
}

ostream& operator<<(ostream& os, const Node* attr) {
        ldump(attr, attr->size);
        if (Context::verbose) {
        os << "index: " << outvar((attr->index & 0xFFFFFFFFFFFF)) << tab
            << "size:" << outvar(attr->size) << tab
            << "name end: " << outvar(attr->offset + offsetof(Node, unused)) << tab
            << "flags: " << outvar(attr->flags) << tab;
        }
        if (attr->length) {
            try {
                os << '\t' << converter.to_bytes(&attr->name, &attr->name + attr->length);
            }
            catch (...) { os << '\t' << "name exception"; }
        }
        else os << "no names";
        if (Context::verbose) os << endl;
    return os;
}

bool IndexRoot::parse(File* file) const {
    string name;
    const Node* node = reinterpret_cast<const Node*>((char*)header + header->offset);
    const Node* last = reinterpret_cast<const Node*>((char*)this + header->size);
    while (node < last && node->index && node->size) {
        try {
            name = converter.to_bytes(&node->name, &node->name + node->length);
            file->entries.push_back(name);
            node = reinterpret_cast<const Node*>((const char*)node + node->size);
        }
        catch (...) {
            node = reinterpret_cast<const Node*>((const char*)node + node->size);
        }
    }
    return true;
}

ostream& operator<<(ostream& os, const IndexRoot* attr) {
    ldump(attr, attr->header->size);
    os << "type: " << outvar((uint64_t)attr->type) << tab
        << "collation: " << outvar(attr->collation) << tab
        << "size: " << outvar(attr->size) << tab
        << "clusters: " << outchar(attr->clusters) << endl;
    os << attr->header;
    const Node* last = reinterpret_cast<const Node*>((char*)attr + attr->header->size);
    const Node* node = reinterpret_cast<const Node*>((char*)attr->header + attr->header->offset);
    while (node < last && node->index && node->size) {
        os << node;
        node = reinterpret_cast<const Node*>((char*)node + node->size);
    }
    return os;
}

ostream& operator<<(ostream& os, const FileName* attr) {
    string path = attr->getName();
    uint16_t dir = attr->dir;
    try {
        string name;
        uint16_t last = 0;
        while (true) {
            last = dir;
            tie(name, dir) = dirs.at(dir);
            if (dir == last) break;
            path = name.append("/").append(path);
        }
    }
    catch (...) {
        path = string("@").append(to_string(dir)).append("/").append(path);
    }
    os << "NAME: ";
    os << path << endl;
    os << "directory: " << outvar(attr->dir) << endl
        << "creation: " << outtime(attr->creatTime) << endl
        << "modification: " << outtime(attr->modTime) << endl
        << "entry change: " << outtime(attr->changeTime) << endl
        << "access: " << outtime(attr->accessTime) << endl
        << "size: " << outvar(attr->size) << endl
        << "alloc: " << outvar(attr->alloc) << endl
        << "flags: " << outvar(attr->flags) << endl
        << "length: " << outchar(attr->length) << endl
        << "space: " << outchar(attr->space) << endl;
    return os;
}
uint32_t Boot::getSize() const {
    uint32_t size = size;
    if (xsize < 0) size = 1 << (-xsize);
    return size;
}

ostream& operator<<(ostream& os, const Boot* boot) {
    uint64_t total = boot->total*sectorSize;
    os << "boot: " << boot->oemId << endl
        << "bytes per sector: " << outvar(boot->bytesPerSector) << endl
        << "sectors per cluster: " << outchar(boot->sectorsPerCluster) << endl
        << "media: " << outchar(boot->mediaId) << endl
        // << "sectors per track: " << outchar(boot->sectorsPerTrack) << endl
        // << "heads: " << outchar(boot->heads) << endl
        << "hidden: " << outvar(boot->hidden) << endl
        << "total: " << outvar(boot->total) << endl
        << "total: " << (total/(1LL<<30)) << "G" << endl
        << "start: " << outvar(boot->start) << endl
        << "mirror: " << outvar(boot->mirror) << endl
        << "size: " << outvar(boot->getSize()) << endl
        << "record: " << outchar(boot->record) << endl
        << "serial: " << outvar(boot->serial) << endl
        << endl;
    return os;
};

ostream& operator<<(ostream& os, const Entry* entry) {
    uint32_t* endTag = (uint32_t*)(entry->key + entry->size) - 2;
    os << "Entry: ";
    if (!entry->inUse())
        return os << "not used" << endl;
    if (*endTag != 0xFFFFFFFF) {
        os << "BAD END TAG: " << entry->size << '/' << entry->alloc << '/' << *endTag << endl;
        confirm();
        return os;
    }
    os
        << (entry->isDir()? "DIR": "file")
        << '/'
        << dec << entry->rec << endl;
    // os << "key: "; os.write(entry->key, 4); os << endl;
    os << "update sequence: " << outchar(entry->updateSeq) << endl
        << "update size: " << outchar(entry->updateSize) << endl
        << "log sequence nr: " << outvar(entry->logSeq) << endl
        << "sequence nr: " << outvar(entry->seq) << endl
        << "links: " << outvar(entry->ref) << endl
        << "attributes: " << outvar(entry->attr) << endl
        << "flags: " << outvar(entry->flags) << endl
        << "size: " << outvar(entry->size) << endl
        << "allocated: " << outvar(entry->alloc) << endl
        << "base: " << outvar(entry->base) << endl
        << "next: " << outvar(entry->next) << endl
        << endl
        << "Attributes:\n" << endl;
    const Attr* next = (Attr*)(entry->key + entry->attr);
    while (next) {
        os << '@' << hex << ((char*)next - entry->key) << '.';
        os << next << endl;
        next = next->getNext();
    }
    return os;
}

bool File::hit(const set<string> entries, bool must) {  // return means continue checking
    if (!valid) return false;           // no further checking
    if (entries.empty()) return true;   // continue other checking
    valid = !must;
    for (auto extension: entries) {
        auto hit = mime.find(extension);
        if (hit != mime.end())
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

ostream& operator<<(ostream& os, File& file) {
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
                    os << outpaix(run.first * sectorsPerCluster, run.second * sectorsPerCluster) << tab;
            // os << "max:" << outvar(Runlist::maxLcn * sectorsPerCluster) << tab;
        }
    }
    else os << "resident" << tab;

    if (file.dir && !file.entries.empty()) {
        for (string entry: file.entries) os << entry << '\t';
    }

    return os << endl;
}

File::File(LBA lba, const vector<char>& buffer, Context& context):
        context(context), error(false), pid(-1),
        valid(false), lba(lba), dir(false), size(0), alloc(0),
        content(nullptr), done(false), exists(false)
{
    const Entry* entry = reinterpret_cast<const Entry*>(buffer.data());
    uint32_t* endTag = (uint32_t*)(entry->key + entry->size) - 2;
    if (!entry) return;
    used = entry->inUse();
    if (!used) return;
    if (*endTag != 0xFFFFFFFF) return;
    index = entry->rec;
    const Attr* attrs = (const Attr*)(entry->key + entry->attr);
    if (entry->isDir()) {
        string name;
        dir = true;
        uint16_t parent = attrs->getDir(name);
        if (dirs.end() == dirs.find(entry->rec)) {
            dirs[entry->rec] = make_pair(name, parent);
            if (context.extra) {
                if (!context.dirFile.is_open()) context.dirFile.open("ntfs.dirs", ios::binary);
                if (context.dirFile.is_open()) context.dirFile << entry->rec << tab << name << tab << parent << endl;
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
        context.offset = runlist[0].first * sectorsPerCluster - lba;
        cerr << "New context LBA offset based on last $MFT record: " << outvar(context.offset) << endl;
    }
    hit(context.getIncludes(), true);
    hit(context.getExcludes(), false);
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
    vector<char> buffer(sectorSize * sectorsPerCluster);
    streamsize save, bytes = file.size;
    uint64_t magic, mask = 0;
    magic = file.context.magic;
    while (magic) {
        mask =  (mask << 8) + 0xFF;
        magic = magic/0x100;
    }
    if (!file.runlist.empty())
        for (auto run: file.runlist) {
            LBA first = run.first * sectorsPerCluster;
            if (first < file.context.offset) {
                cerr << "Runlist LBA negative. Try scanning disk device not partition. Aborting" << endl;
                file.error = true;
                confirm;
                return ifs;
            }
            else first -= file.context.offset;
            ifs.seekg(first * sectorSize);
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
                        ldump(buffer.data(), sizeof(file.magic));
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
    if (context.format == Format::None) return;
    tm* te;
    te = localtime(reinterpret_cast<const time_t*>(&time));
    ostringstream path;
    path << '/' << te->tm_year + 1900 << '/';
    if (context.format > Format::Year) {
        path << setfill('0') << setw(2) << (te->tm_mon + 1) << '/';
        if (context.format > Format::Month)
            path << setfill('0')<< setw(2) << (te->tm_mday) << '/' ;
    }
    this->path = path.str();
}

bool File::open() {
    string target(context.dir);
    mangle();
    target.append(path);
    string full = target + name;
    if (filesystem::exists(full)) {     // if file exists...
        struct stat info;
        stat(full.c_str(), &info);
        if (context.verbose) cerr << "File exists: " << full;
        if (size <= info.st_size && !context.force) {   // check file size against MFT entry
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

ostream& operator<<(ostream& oss, const Context& context) {
    oss << "Device:" << context.dev << ", "
        << "lba:" << outvar(context.start);
    if (context.stop) oss << '>' << outvar(context.stop);
    oss << ", ";
    if (context.count > 0) oss << "count:" << context.count << ", ";
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
    oss << "processes:" << context.threads << ", ";
    oss << "size:" << context.size / (1 << 20) << "MB, ";
    if (context.verbose) {
        if (!context.debug) oss << "verbose, ";
        else oss << "debug, ";
    }
    if (context.confirm) oss << "confirm, ";
    if (context.all) oss << "show all, ";
    if (context.format != Format::None) {
        oss << "path:/yyyy/";
        if (context.format > Format::Year) oss << "mm/";
        if (context.format > Format::Month) oss << "dd/";
        oss << ", ";
    }
    oss << "pid:" << getpid() << endl;
    if (context.recover) oss << "Out:" << context.dir;
    if (context.force) oss << ", overwrite existing files";
    return oss;
}

Context::Context(): dir("."), count(-1L) {
    offset = 0;
    start = stop = 0;
    magic = 0;
    verbose = false;
    debug = false;
    confirm = false;
    recover = false;
    all = false;
    force = false;
    format = Format::None;
    size = 1 << 24;
    threads = 4;
    sem = (sem_t*)mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    sem_init(sem, 1, 4);
    if (dir.back() == '/') dir.pop_back();
    ifstream known("/etc/mime.types");
    string line, type, extensions, file;
    if (known.is_open())
        while(getline(known, line))
            if (!line.empty() && line.front() != '#') {
                istringstream entry(line);
                getline(entry, type, '/');
                getline(entry, extensions);
                istringstream extension(extensions);
                extension >> file;
                while (extension >> file) mime[type].insert(file);
            }
}

void Context::extensions(string include) {
    istringstream iss(include);
    string name;
    while (getline(iss, name, ',')) this->include.insert(lower(name));
}

void Context::exclutions(string exclude) {
    istringstream iss(exclude);
    string name;
    while (getline(iss, name, ',')) this->exclude.insert(lower(name));
}

ostream& operator<<(ostream& os, const Index* index)
{
    if (Context::debug) os << endl;
    os << "indx/" << index->vnc;
    pdump(index, index->header);
    if (Context::verbose)
        os << "\tfixup: " << outvar(index->fixup) << tab
            << "entries: " << outvar(index->entries) << tab
            << "log sequence: " << outvar(index->logSeq) << tab
            << "vnc: " << outvar(index->vnc) << endl;
    os << index->header;
    if (Context::debug) os << endl;
    const Node* node = reinterpret_cast<const Node*>((char*)index->header + index->header->offset);
    const Node* last = reinterpret_cast<const Node*>((char*)index + index->header->size);
    while (node < last ) {
        os << node;
        node = reinterpret_cast<const Node*>((char*)node + node->size);
    }
    os << endl;
    os << endl;
    return os;
}
