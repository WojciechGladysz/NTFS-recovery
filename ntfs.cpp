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

#include "helper.hpp"
#include "context.hpp"
#include "file.hpp"

using namespace std;

static unordered_map<string, set<string>> mime;
static set<string> exts;

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

bool StandardInfo::parse(File* file) const {
    file->time = static_cast<Time_t>(convert(changeTime));
    file->access = static_cast<Time_t>(convert(accessTime));
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

bool FileName::parse(File* file) const {
    string path = "/";
    uint16_t dir = this->dir;
    try {
        string name;
        uint16_t last = 0;
        do {
            last = dir;
            tie(name, dir) = file->dirs.at(dir);
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
                if (!file->context.exts.is_open()) file->context.exts.open("ntfs.exts", ios::binary);
                if (file->context.exts.is_open()) {
                    file->context.exts << file->ext << ',';
                    file->context.exts.flush();
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
    os.flush();
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
            tie(name, dir) = File::dirs.at(dir);
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
    uint64_t total = boot->total*boot->sector;
    os << "boot: " << boot->oemId << endl
        << "bytes per sector: " << outvar(boot->sector) << endl
        << "sectors per cluster: " << outchar(boot->sectors) << endl
        << "media: " << outchar(boot->mediaId) << endl
        << "sectors per track: " << outchar(boot->sectorsPerTrack) << endl
        << "heads: " << outchar(boot->heads) << endl
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

Index::operator bool() const {
    return !strncmp(key, "INDX", 4);
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

Entry::operator bool() const {
    return !strncmp(key, "FILE", 4);
}

ostream& operator<<(ostream& os, const Entry* entry) {
    uint32_t* endTag = (uint32_t*)(entry->key + entry->size) - 2;
    os << "Entry: ";
    if (!entry->used())
        return os << "not used" << endl;
    if (*endTag != 0xFFFFFFFF) {
        os << "BAD END TAG: " << entry->size << '/' << entry->alloc << '/' << *endTag << endl;
        confirm();
        return os;
    }
    os
        << (entry->dir()? "DIR": "file")
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
