#include <iomanip>
#include <cstring>
#include <utime.h>
#include <unistd.h>
#include <cerrno>
#include <sys/stat.h>
#include <sys/mman.h>

#include <map>
#include <vector>

#include "attr.hpp"
#include "helper.hpp"
#include "context.hpp"
#include "file.hpp"

const uint64_t NTFS_TO_UNIX_EPOCH = 11644473600ULL; // Difference in seconds
const uint64_t INTERVALS_PER_SECOND = 10000000ULL;  //

using namespace std;

static unordered_map<string, set<string>> mime;
static set<string> exts;

enum class AttrId: uint32_t
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
	{ AttrId::StandardInfo, "$STANDARD INFO" },
		{ AttrId::AttributeList, "$ATTRIBUTE LIST" },
		{ AttrId::FileName, "$FILE NAME" },
		{ AttrId::ObjectId, "$OBJECT ID" },
		{ AttrId::SecurityDescriptor, "$SECURITY DESCRIPTOR" },
		{ AttrId::VolumeName, "$VOLUME NAME" },
		{ AttrId::VolumeInformation, "$VOLUME INFORMATION" },
		{ AttrId::Data, "$DATA" },
		{ AttrId::IndexRoot, "$INDEX ROOT" },
		{ AttrId::IndexAllocation, "$INDEX ALLOCATION" },
		{ AttrId::Bitmap, "$BITMAP" },
		{ AttrId::ReparsePoint, "$REPARSE POINT" },
		{ AttrId::EAInformation, "$EA INFORMATION" },
		{ AttrId::EA, "$EA" },
		{ AttrId::PropertySet, "$PROPERTY SET" },
		{ AttrId::LoggedUtilityStream, "$LOGGED UTILITY STREAM" }
};

time_t convert(const Time time) {
	time_t timestamp = (uint64_t)time/INTERVALS_PER_SECOND - NTFS_TO_UNIX_EPOCH;
	return timestamp;
}

string convert16(char16_t utf16) {
	string utf8;
	utf8.push_back(0xC0 | ((utf16 >> 6) & 0x1F));
	utf8.push_back(0x80 | (utf16 & 0x3F));
	return utf8;
}

string fixName(const char16_t* start, const char16_t* stop)
{
	string name;
	union __attribute__ ((packed)) chrs{
		char16_t    wch;
		struct {
			char    lch;
			char    hch;
		};
	};
	const chrs* wch = reinterpret_cast<const chrs*>(start);
	const chrs* end = reinterpret_cast<const chrs*>(stop);
	while (wch < end) {
		if ((wch->wch & 0xC0) == 0x80);
		else if ((wch->wch & 0xEC) == 0xC8) {
			if (wch->hch) {
				name.push_back(wch->lch);
				name.push_back(wch->hch);
			}
			else if (((wch+1)->wch & 0xC0) == 0x80) {
				name.push_back(wch++->lch);
				name.push_back(wch->lch);
			}
		}
		else if (!wch->hch && !(wch->lch & 0x80)) name.push_back(wch->lch);
		else name.append(convert16(wch->wch));
		wch++;
	}
	return name;
}

ostream& operator<<(ostream& os, const Time time) {
	time_t timestamp = convert(time);
	os << std::put_time(std::localtime(&timestamp), "%Y.%m.%d %H:%M:%S");
	return os;
}

ostream& operator<<(ostream& os, const Info* attr) {
	os << "creation: " << outtime(attr->creatTime) << endl
		<< "change: " << outtime(attr->changeTime) << endl
		<< "write: " << outtime(attr->writeTime) << endl
		<< "access: " << outtime(attr->accessTime) << endl
		<< "attr: " << outvar(attr->attr) << endl;
	return os;
}

ostream& operator<<(ostream& os, const Name* attr) {
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
	os << "NAME: " << path;
	if (attr->length <= 16) os << tab; else os << endl;
	pdump(attr->data, attr->data + attr->length);
	os << endl << "directory: " << outvar(attr->dir) << endl
		<< "creation: " << outtime(attr->creatTime) << endl
		<< "modification: " << outtime(attr->modTime) << endl
		<< "entry change: " << outtime(attr->changeTime) << endl
		<< "access: " << outtime(attr->accessTime) << endl
		<< "size: " << outvar(attr->size) << endl
		<< "alloc: " << outvar(attr->alloc) << endl
		<< "flags: " << outvar(attr->flags);
	if (attr->flags & RONLY) os << "/READ ONLY";
	if (attr->flags & HID) os << "/HIDDEN";
	if (attr->flags & SYS) os << "/SYSTEM";
	if (attr->flags & ARCH) os << "/ARCHIVE";
	if (attr->flags & DEV) os << "/DEVICE";
	if (attr->flags & TEMP) os << "/NORMAL";
	if (attr->flags & SPAR) os << "/SPARSE";
	os << endl
		<< "length: " << outchar(attr->length) << endl
		<< "space: " << outchar(attr->space) << endl;
	return os;
}

ostream& operator<<(ostream& os, const Node* attr) {
	if (ldump(attr, attr->size)) os << endl;
	if (Context::debug) {
		os << "index: " << outvar(attr->index) << tab
			<< "size:" << outvar(attr->size) << tab
			<< "name end: " << outvar(attr->end) << tab
			<< "flags: " << outchar(attr->flags);
		if (attr->flags & LAST) os << "/LAST";
		if (attr->flags & SUB)
			os << "/SUB" << '/' << *reinterpret_cast<VCN*>((char*)attr + attr->size - 8);
		os << endl;
	}
	if (!(attr->flags & LAST)) {
		os << dec << attr->index << '/' << attr->getName();
		if (Context::debug) os << endl;
		else os << tab;
	}

	return os;
}

ostream& operator<<(ostream& os, const Header* attr) {
	if (Context::verbose) {
		os << "Header: ";
		if (pdump(attr, attr->node)) os << endl;
		os << "offset: " << outvar(attr->offset) << tab
			<< "size: " << outvar(attr->size) << tab
			<< "allocated: " << outvar(attr->allocated) << tab
			<< "flags: " << outchar(attr->flags);
		if (attr->flags & LARGE) os << "/LARGE";
		os << endl;
	}
	const Node* last = reinterpret_cast<const Node*>((char*)attr + attr->size);
	const Node* node = reinterpret_cast<const Node*>((char*)attr + attr->offset);
	while (node < last) {
		os << node;
		if (node->flags & LAST) break;
		node = reinterpret_cast<const Node*>((char*)node + node->size);
	}
	return os;
}

ostream& operator<<(ostream& os, const Root* attr) {
	os << "Root: " << tab;
	if (pdump(attr, attr->header)) os << endl;
	os << "type: " << outvar((uint64_t)attr->type) << tab
		<< "collation: " << outvar(attr->collation) << tab
		<< "size: " << outvar(attr->size) << tab
		<< "clusters: " << outchar(attr->clusters) << endl;
	os << attr->header;
	return os;
}

ostream& operator<<(ostream& os, const Attr* attr) {
	os << attr->id << "." << attrName[attr->type] << '/' << outpaix((uint)attr->type, attr->size) << tab;
	if (ldump(attr, sizeof(Attr))) os << endl;

	if (attr->length) { // attribute yield alternate data stream
		const char16_t* w_name = reinterpret_cast<const char16_t*>((char*)attr + attr->offset);
		os << "Attribute/" << outpair((uint)attr->length, attr->offset)
			<< ": " <<  fixName(w_name, w_name + attr->length) << tab;
		if (ldump(w_name, sizeof(*w_name) * attr->length)) os << endl;
		else os << tab;
	}

	if (attr->noRes) os << static_cast<const Nonres*>(attr);
	else os <<  static_cast<const Resident*>(attr);

	return os;
}

ostream& operator<<(ostream& os, const Resident* attr) {
	if (!Context::debug
			&& attr->type != AttrId::FileName
			&& attr->type != AttrId::IndexRoot
			&& attr->type != AttrId::Data
	   ) return os << endl;
	os << "Resident/" << outpaix(attr->length, attr->offset) << tab;
	pdump(&attr->length, attr->data);
	os << endl;

	const auto data = reinterpret_cast<const char*>(attr) + attr->offset;
	if (attr->type == AttrId::StandardInfo) os << reinterpret_cast<const Info*>(data);
	else if (attr->type == AttrId::FileName) os << reinterpret_cast<const Name*>(data);
	else if (attr->type == AttrId::IndexRoot) os << reinterpret_cast<const Root*>(data);
	else if (Context::debug && attr->type == AttrId::Data && attr->length) {
		os << "File content: " << endl;
		os.write(data, attr->length);
		os << endl << "EOF" << endl;
	}

	return os;
}

ostream& operator<<(ostream& os, const Nonres* attr) {
	os << "Nonresident" << endl;
	if (pdump(&attr->first, attr->data)) os << endl;
	os << "first: " << outvar(attr->first) << tab
		<< "last: " << outvar(attr->last) << tab
		<< "offset: " << outvar(attr->runlist) << tab
		<< "compress: " << outvar(attr->compress) << tab
		<< "size: " << outvar(attr->size) << tab
		<< "alloc: " << outvar(attr->alloc) << tab
		<< "used: " << outvar(attr->used) << endl;
	size_t count = attr->last - attr->first + 1;
	const auto* runlist = reinterpret_cast<const Runlist*>((char*)attr + attr->runlist);

	// output runlist info
	int64_t offset;
	uint64_t length;
	if (!runlist->lenSize) return os << endl;
	os << "runlist: ";
	os.flush();
	bool many = false;
	while (count && runlist->lenSize && runlist->offSize) {
		uint64_t lenMask = ((1LL<<(8*runlist->lenSize))-1LL);
		int64_t shift = runlist->run >> (8*runlist->lenSize);
		auto offMask = ((1LL<<(8*runlist->offSize))-1LL);
		offset = shift & offMask;
		length = runlist->run & (lenMask);
		if (Context::debug && many) os << tab;
		os << outpair(runlist->offSize, runlist->lenSize) << ':' << outpaix(offset, length) 
			<< '/' << dec << length;
		if (ldump(runlist, runlist->lenSize + runlist->offSize + 1)) os << endl;
		else os << tab;
		if (runlist->lenSize > 4 || runlist->offSize > 4) {
			cerr << "corrupted: " << outpair(runlist->offSize, runlist->lenSize) << endl;
			confirm();
			return os;
		}
		runlist = (Runlist*)((char*)runlist + 1 + runlist->lenSize + runlist->offSize);
		many = true;
		count -= length;
	}
	if (!Context::debug) os << endl;

	return os;
}

bool Info::parse(File* file) const {
	file->time = static_cast<Time_t>(convert(changeTime));
	file->access = static_cast<Time_t>(convert(accessTime));
	return true;
}

string Node::getName() const {
	string name;
	const char16_t* end = reinterpret_cast<const char16_t*>(cdata + this->end);
	try { name = fixName(this->name, end); }
	catch (...) { name = "N/A"; }
	return name;
}

string Name::getName() const {
	return fixName(data, data + length);
}

bool Name::parse(File* file) const {
	if (!file) return false;

	string name = getName();
	if (space < 2)
		file->name = name;
	else if (file->name.empty())
		file->name = name;

	file->parent = dir;
	file->valid = !name.empty();

	// find file extension for matching include/exclude parameter
	if (file->name.npos != file->name.find('.')) {
		istringstream ext(file->name);
		while (getline(ext, file->ext, '.'));
	}

	return file->valid;
}

ostream& operator<<(ostream& os, const Runlist* attr) {
	return os << "Runlist::operator<< not implemented" << endl;
}

uint64_t Runlist::minLcn = 0xFFFFFFFFFFFFFFFF;
uint64_t Runlist::maxLcn = 0;

vector<pair<VCN, VCN>> Runlist::parse(File* file, size_t count) const {
	vector<pair<VCN, VCN>> runlist;
	uint64_t length;
	int64_t offset;
	uint64_t first = 0;
	uint64_t last = 0;
	const Runlist* attr = this;
	while (count && attr->lenSize && attr->offSize) {
		if (attr->lenSize > 4 || attr->offSize > 4) {
			if (file) file->error = true;
			return runlist;
			cerr << *file << endl;
			confirm("Runlist corrupted");
		}
		uint64_t lenMask = ((1LL<<(8*attr->lenSize))-1LL);
		int64_t shift = attr->run >> (8*attr->lenSize);
		auto offMask = ((1LL<<(8*attr->offSize))-1LL);
		length = attr->run & lenMask;
		offset = shift & offMask;
		if (offset & (1LL<<(8*attr->offSize-1))) offset |= (0xFFFFFFFFFFFFFFFF << (8*attr->offSize));
		first += offset;
		last = first + length;
		if (first < minLcn) minLcn = first;
		if (last > maxLcn) maxLcn = last;
		runlist.push_back(make_pair(first, last));
		count -= last - first;
		attr = (Runlist*)((char*)attr + 1 + attr->lenSize + attr->offSize);
	}
	return runlist;
}

const Name* Attr::getName() const {
	if (type == AttrId::FileName) return reinterpret_cast<const Name*>(static_cast<const Resident*>(this)->data);
	else return nullptr;
}

bool Root::parse(File* file) const {
	return header->parse(file);
}

bool Node::parse(File *file) const {
	string entry;
	const char16_t* stop = reinterpret_cast<const char16_t*>(cdata + end);
	entry = getName();
	file->entries.emplace_back(index, entry);
	return true;
}

bool Header::parse(File* file) const {
	string name;
	const Node* node = reinterpret_cast<const Node*>((char*)this + offset);
	const Node* last = reinterpret_cast<const Node*>((char*)this + size);
	while (node < last && !(node->flags & LAST)) {
		node->parse(file);
		node = reinterpret_cast<const Node*>((const char*)node + node->size);
	}
	return true;
}

const Attr* Attr::parse(File* file) const {
	if (noRes) static_cast<const Nonres*>(this)->parse(file);
	else static_cast<const Resident*>(this)->parse(file);
	return getNext();
}

bool Resident::parse(File* file) const {
	void* data = reinterpret_cast<void*>((char*)this + offset);
	if (type == AttrId::StandardInfo) return reinterpret_cast<const Info*>(data)->parse(file);
	else if (type == AttrId::FileName) return reinterpret_cast<const Name*>(data)->parse(file);
	else if (type == AttrId::IndexRoot) return reinterpret_cast<const Root*>(data)->parse(file);
	else if (type == AttrId::Data && !length) {
		file->size = length;
		return file->content = reinterpret_cast<char*>(data);
	}
	return false;
}

bool Nonres::parse(File* file) const {
	if (!file) return false;
	if ((type == AttrId::Data && !length)   // no alternate data stream
			|| (type == AttrId::IndexAllocation)) {
		file->size = used & 0xFFFFFFFFFFFF;
		file->alloc = alloc & 0xFFFFFFFFFFFF;
		file->runlist.emplace(first, Run());
		size_t count = last - first + 1;
		file->runlist[first].count = count;
		auto attr = reinterpret_cast<const Runlist*>((char*)this + runlist);
		auto runlist = attr->parse(file, count);
		auto& list = file->runlist[first].list;
		list.insert(list.end(), runlist.begin(), runlist.end());
	}
	return true;
}

const Attr* Attr::getNext() const {
	if (!size) return nullptr;
	Attr* next = (Attr*)((char*)this + size);
	if (next->type == AttrId::End) return nullptr;
	if ((uint16_t)next->type && next->size)
		return next;
	cerr << "Next attribute corrupted: " << outpair((uint16_t)next->type, next->size) << endl;
	confirm();
	return nullptr;
}
