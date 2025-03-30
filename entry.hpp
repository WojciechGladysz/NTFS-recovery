#include <fstream>
#include <cstdint>
#include <vector>

#include "attr.hpp"

class Context;

struct __attribute__ ((packed)) Boot {
	static const uint8_t jmp[];
	uint8_t		jmpCode[3];
	char		oemId[8];
	uint16_t	sector;				// 0x0B
	uint8_t		sectors;			// 0x0D
	char		unused0[7];			// 0x0E, checked when volume is mounted
	uint8_t		mediaId;			// 0x15
	char		unused1[2];			// 0x16
	uint16_t	sectorsPerTrack;	// 0x18
	uint16_t	heads;				// 0x1A

	uint32_t	hidden;				// 0x1C
	uint32_t	unused2[2];			// 0x20
	uint64_t	total;				// 0x28
	uint64_t	start;				// 0x30
	uint64_t	mirror;				// 0x38
	union {
		int8_t	xsize;				// 0x40
		int32_t size;				// 0x40
	};
	uint8_t		record;
	uint8_t		index;
	uint16_t	serial;
	uint8_t		unused3[438];
	uint16_t	endTag;

	operator bool() const;
	uint32_t getSize() const;
};

struct __attribute__ ((packed)) Index {
	char		key[4];
	uint16_t	fixup;
	uint16_t	entries;

	uint64_t	logSeq;
	uint64_t	vnc;

	Header		header[];
	operator bool() const;
	friend ostream& operator<<(ostream&, const Index*);
};

struct __attribute__ ((packed)) Record {
	char		key[4];
	uint16_t	updateSeq;
	uint16_t	updateSize;

	uint64_t	logSeq;

	uint16_t	seq;
	uint16_t	ref;
	uint16_t	attr;
#define USE		(1<<0)
#define DIR		(1<<1)
	uint16_t	flags;

	uint32_t	size;
	uint32_t	alloc;

	uint64_t	base;

	uint16_t	next;
	uint16_t	unused;
	uint32_t	rec;

	bool used() const { return flags & USE; }
	bool dir() const { return flags & DIR; }
	const Name* getName() const;
	uint64_t getParent(string& name) const;
	operator bool() const;
};

class Entry:std::vector<char> {
	Context& context;
	public:
	Entry(Context&);
	const Record* record() const { return reinterpret_cast<const Record*>(data()); }
	friend ifstream& operator>>(ifstream& ifs, Entry& entry);
};

ostream& operator<<(ostream&, const Entry*);
