#pragma once
#include <iostream>
#include <ctime>
#include <string>
#include <thread>

#include <cstdint>

using VCN = uint64_t;
using namespace std;

enum class Time: uint64_t;
class File;

enum class AttrId: __attribute__ ((packed)) uint32_t;

struct __attribute__ ((packed)) Info {
    Time        creatTime;
    Time        changeTime;
    Time        writeTime;
    Time        accessTime;
    uint32_t    attr;
    bool        parse(File*) const;
    friend ostream& operator<<(ostream& os, const Info*);
};

struct __attribute__ ((packed)) Name {
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
    friend ostream& operator<<(ostream& os, const Name*);
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

struct __attribute__ ((packed)) Root {
    AttrId      type;
    uint32_t    collation;
    uint32_t    size;
    uint8_t     clusters;
    uint8_t     unused[3];
    Header      header[];
    bool        parse(File* file) const;
    friend ostream& operator<<(ostream& os, const Root*);
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

struct __attribute__ ((packed)) Nonres {
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
    friend ostream& operator<<(ostream& os, const Nonres*);
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
        Nonres      nonres;
    };
    Attr* getNext() const;
    uint16_t getDir(string& name) const;
    Attr* parse(File* file) const;
    friend ostream& operator<<(ostream& os, const Attr*);
};

ostream& operator<<(ostream& os, const Time time);
time_t convert(const Time time);
