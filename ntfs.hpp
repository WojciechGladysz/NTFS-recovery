#pragma once
#include <iostream>
#include <ctime>
#include <string>
#include <thread>

#include <cstdint>

using VCN = uint64_t;
using namespace std;

enum class Time:uint64_t;
class File;

enum class AttrId: __attribute__ ((packed)) uint32_t;

struct __attribute__ ((packed)) Boot {
    static const uint8_t jmp[];
    uint8_t     jmpCode[3];
    char        oemId[8];
    uint16_t    sector;     // 0x0B
    uint8_t     sectors;            // 0x0D
    char        unused0[7];         // 0x0E, checked when volume is mounted
    uint8_t     mediaId;            // 0x15
    char        unused1[2];         // 0x16
    uint16_t    sectorsPerTrack;    // 0x18
    uint16_t    heads;              // 0x1A

    uint32_t    hidden;             // 0x1C
    uint32_t    unused2[2];         // 0x20
    uint64_t    total;              // 0x28
    uint64_t    start;              // 0x30
    uint64_t    mirror;             // 0x38
    union {
        int8_t  xsize;           // 0x40
        int32_t size;            // 0x40
    };
    uint8_t     record;
    uint8_t     index;
    uint16_t    serial;
    uint8_t     unused3[438];
    uint16_t    endTag;
    operator bool() const;
    uint32_t getSize() const;
    friend ostream& operator<<(ostream& os, const Boot*);
};

struct __attribute__ ((packed)) StandardInfo {
    Time        creatTime;
    Time        changeTime;
    Time        writeTime;
    Time        accessTime;
    uint32_t    attr;
    bool        parse(File*) const;
    friend ostream& operator<<(ostream& os, const StandardInfo*);
};

struct __attribute__ ((packed)) FileName {
    uint16_t    dir:16;
    uint64_t    seq:48;

    Time        creatTime;
    Time        modTime;
    Time        changeTime;
    Time        accessTime;

    uint64_t    alloc;
    uint64_t    size;

    uint32_t    flags;
    uint32_t    reparse;

    uint8_t     length;
    uint8_t     space;
    char16_t    data;
    uint16_t    getDir(string& name) const {
        name = getName();
        return dir;
    }
    string      getName() const;
    bool        parse(File* file) const;
    friend ostream& operator<<(ostream& os, const FileName*);
};

struct __attribute__ ((packed)) Node {
    uint64_t    index;
    uint16_t    size;
    uint16_t    offset;
    uint32_t    flags;
    uint64_t    unused[8];
    uint8_t     length;
    uint8_t     unknown;
    char16_t    name;
    friend ostream& operator<<(ostream& os, const Node*);
};

struct __attribute__ ((packed)) Header {
    uint32_t    offset;
    uint32_t    size;
    uint32_t    allocated;
    uint32_t    flags;
    Node   node[];
    friend ostream& operator<<(ostream& os, const Header);
};

struct __attribute__ ((packed)) IndexRoot {
    AttrId      type;
    uint32_t    collation;
    uint32_t    size;
    uint8_t     clusters;
    uint8_t     unused[3];
    Header      header[];
    bool        parse(File* file) const;
    friend ostream& operator<<(ostream& os, const IndexRoot*);
};

struct __attribute__ ((packed)) Runlist {
    static uint64_t minLcn;
    static uint64_t maxLcn;
    uint16_t    lenSize:4;
    uint16_t    offSize:4;
    union {
        uint64_t    run;
        struct {
            uint32_t    length;
            uint32_t    offset;
        };
    };
    bool    parse(File* file) const;
    friend ostream& operator<<(ostream& os, const Runlist*);
};

struct __attribute__ ((packed)) Resident {
    uint32_t    length;
    uint16_t    offset;
    uint8_t     indexed;
    uint8_t     unused;
    bool        parse(File* file) const;
    friend ostream& operator<<(ostream& os, const Resident*);
};

struct __attribute__ ((packed)) NonResident {
    VCN    first;
    VCN    last;
    uint16_t    runlist;
    uint16_t    compres;
    uint8_t     unused[4];

    uint64_t    size;
    uint64_t    alloc;
    uint64_t    used;
    uint16_t    getRun() const { return runlist; }
    bool        parse(File*) const;
    friend ostream& operator<<(ostream& os, const NonResident*);
};

struct __attribute__ ((packed)) Attr {
    AttrId      type;
    uint16_t    size;  
    uint16_t    unknown;  
    uint8_t     noRes;     
    uint8_t     length;
    uint16_t    offset;
    uint16_t    flags;
    uint16_t    id;
    union {
        Resident    res;
        NonResident nonRes;
    };
    Attr* getNext() const;
    uint16_t getDir(string& name) const;
    Attr* parse(File* file) const;
    friend ostream& operator<<(ostream& os, const Attr*);
};

struct __attribute__ ((packed)) Index {
    char        key[4];
    uint16_t    fixup;
    uint16_t    entries;

    uint64_t    logSeq;
    uint64_t    vnc;

    Header      header[];
    operator bool() const;
};

struct __attribute__ ((packed)) Entry {
    char        key[4];
    uint16_t    updateSeq;
    uint16_t    updateSize;

    uint64_t    logSeq;

    uint16_t    seq;
    uint16_t    ref;
    uint16_t    attr;
#define USE   (1<<0)
#define DIR   (1<<1)
    uint16_t    flags;

    uint32_t    size;
    uint32_t    alloc;

    uint64_t    base;

    uint16_t    next;
    uint16_t    unused;
    uint32_t    rec;

    operator bool() const;
    bool used() const { return flags & USE; }
    bool dir() const { return flags & DIR; }
};

ostream& operator<<(ostream&, const Entry*);
