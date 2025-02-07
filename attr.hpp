#pragma once
#include <iostream>
#include <ctime>
#include <thread>
#include <cstdint>

#include <string>
#include <vector>

using VCN = uint64_t;
using namespace std;

enum class Time: uint64_t;
class File;

enum class AttrId: uint32_t;

struct __attribute__ ((packed)) Info {
    Time        creatTime;
    Time        changeTime;
    Time        writeTime;
    Time        accessTime;
    uint32_t    attr;
    bool        parse(File*) const;
};

struct __attribute__ ((packed)) Name {
    uint64_t    dir:32;
    uint64_t    unknown:32;

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
    char16_t    data[];
    string      getName() const;
    bool        parse(File* file) const;
    friend ostream& operator<<(ostream&, const Name*);
};

struct __attribute__ ((packed)) Node {
    uint64_t    index:48;
    uint16_t    unknown1:16;

    uint16_t    size;
    uint16_t    end;
#define SUB     (1 << 0)
#define LAST    (1 << 1)
    uint8_t     flags;
    uint8_t     padding[3];

    union {
        uint64_t    data[8];
        char        cdata[1];
    };

    uint16_t    length;
    char16_t    name[];
    bool        parse(File*) const;
    string      getName() const;
    friend ostream& operator<<(ostream&, const Node*);
};

struct __attribute__ ((packed)) Header {
    uint32_t    offset;
    uint32_t    size;

    uint32_t    allocated;
#define LARGE   (1 << 0)
    uint8_t     flags;
    uint8_t     padding[3];

    Node        node[];
    bool        parse(File*) const;
    friend ostream& operator<<(ostream&, const Header*);
};

struct __attribute__ ((packed)) Root {
    AttrId      type;
    uint32_t    collation;
    uint32_t    size;
    uint8_t     clusters;
    uint8_t     padding[3];
    Header      header[];
    bool        parse(File* file) const;
    friend ostream& operator<<(ostream&, const Root*);
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
    vector<pair<VCN, VCN>> parse(File* file) const;
    friend ostream& operator<<(ostream&, const Runlist*);
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

    const Attr* getNext() const;
    uint16_t getDir(string& name) const;
    const Attr* parse(File* file) const;
    const Name* getName() const;
    friend ostream& operator<<(ostream&, const Attr*);
};

struct __attribute__ ((packed)) Resident:Attr {
    uint32_t    length;
    uint16_t    offset;
    uint8_t     indexed;
    uint8_t     padding;

    char        data[];
    bool        parse(File* file) const;
    friend ostream& operator<<(ostream&, const Resident*);
};

struct __attribute__ ((packed)) Nonres:Attr {
    VCN         first;
    VCN         last;

    uint16_t    runlist;
    uint16_t    compress;
    uint8_t     padding[4];

    uint64_t    size;
    uint64_t    alloc;
    uint64_t    used;
    char        data[];

    bool        parse(File*) const;
    friend ostream& operator<<(ostream&, const Nonres*);
};

ostream& operator<<(ostream& os, const Time time);
time_t convert(const Time time);
