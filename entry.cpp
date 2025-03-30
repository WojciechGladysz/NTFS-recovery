#include <string.h>

#include "entry.hpp"
#include "context.hpp"
#include "attr.hpp"
#include "helper.hpp"

const uint8_t Boot::jmp[] = {0xEB, 0x52, 0x90};

Boot::operator bool() const {
	return !memcmp(jmpCode, jmp, 3)
		&& !strncmp(oemId, "NTFS", 4)
		&& endTag == 0xAA55;
}

uint32_t Boot::getSize() const {
	uint32_t size = size;
	if (xsize < 0) size = 1 << (-xsize);
	return size;
}

ostream& operator<<(ostream& os, const Boot* boot) {
	uint64_t total = boot->total*boot->sector;
	os << "Boot: " << boot->oemId << endl
		<< "bytes per sector: " << outvar(boot->sector) << endl
		<< "sectors per cluster: " << outchar(boot->sectors) << endl
		<< "media: " << outchar(boot->mediaId) << endl
		<< "sectors per track: " << outchar(boot->sectorsPerTrack) << endl
		<< "heads: " << outchar(boot->heads) << endl
		<< "hidden: " << outvar(boot->hidden) << endl
		<< "total: " << outvar(boot->total) << tab << (total/(1LL<<30)) << "G" << endl
		<< "MFT: " << outvar(boot->start) << endl
		<< "mirror: " << outvar(boot->mirror) << endl
		<< "size: " << outvar(boot->getSize()) << endl
		<< "record: " << outchar(boot->record) << endl
		<< "serial: " << outvar(boot->serial) << endl;
	return os;
};

Index::operator bool() const {
	return !strncmp(key, "INDX", 4);
}

ostream& operator<<(ostream& os, const Index* index)
{
	os << "indx/" << index->vnc << tab;
	if (Context::verbose) {
		if (pdump(index, index->header)) os << endl;
		os << "fixup: " << outvar(index->fixup) << tab
			<< "entries: " << outvar(index->entries) << tab
			<< "log sequence: " << outvar(index->logSeq) << tab
			<< "vnc: " << outvar(index->vnc) << endl;
	}
	return os << index->header;
}

Record::operator bool() const {
	// uint32_t* endTag = (uint32_t*)(key + size) - 2;
	// return !strncmp(key, "FILE", 4) && *endTag == 0xFFFFFFFF;
	return !strncmp(key, "FILE", 4);
}

uint64_t Record::getParent(string& name) const {
	uint16_t dir = 0;
	const Name* attr = getName();
	if (attr) {
		dir = attr->dir;
		name = attr->getName();
	}
	return dir;
}

const Name* Record::getName() const {
	auto* attr = reinterpret_cast<const Attr*>((char*)this + this->attr); // pointer to record first attribute
	const Name* result = nullptr;		// pointer to first FileName attribute data with preferred name space
	while (attr) {
		auto* name = attr->getName();
		if (name) {
			if (name->space < 2)
				result = name;
			else if (!result)
				result = name;
		}
		attr = attr->getNext();
	}
	return result;
}

ostream& operator<<(ostream& os, const Record* record) {
	uint32_t* endTag = (uint32_t*)(record->key + record->size) - 2;
	os << "Record: ";
	if (!record->used())
		return os << "not used" << endl;
	if (*endTag != 0xFFFFFFFF) {
		os << "BAD END TAG: " << record->size << '/' << record->alloc << '/' << *endTag << endl;
		confirm();
		return os;
	}
	os
		<< (record->dir()? "DIR": "file")
		<< '/'
		<< dec << record->rec << endl;
	// os << "key: "; os.write(record->key, 4); os << endl;
	os << "update sequence: " << outchar(record->updateSeq) << endl
		<< "update size: " << outchar(record->updateSize) << endl
		<< "log sequence nr: " << outvar(record->logSeq) << endl
		<< "sequence nr: " << outvar(record->seq) << endl
		<< "links: " << outvar(record->ref) << endl
		<< "attributes: " << outvar(record->attr) << endl
		<< "flags: " << outvar(record->flags);
	if (record->flags & USE) os << "/USED";
	if (record->flags & DIR) os << "/DIR";
	os << endl
		<< "size: " << outvar(record->size) << endl
		<< "allocated: " << outvar(record->alloc) << endl
		<< "base: " << outvar(record->base) << endl
		<< "next: " << outvar(record->next) << endl
		<< endl
		<< "Attributes:\n" << endl;
	const Attr* next = reinterpret_cast<const Attr*>(record->key + record->attr);
	while (next) {
		if (Context::debug) os << '@' << hex << (void*)((char*)next - record->key) << ": ";
		os << next << endl;
		next = next->getNext();
	}
	return os;
}

Entry::Entry(Context& context): context(context) {}

ifstream& operator>>(ifstream& ifs, Entry& entry)
{
	LBA lba = ifs.tellg() / entry.context.sector;
	entry.resize(entry.context.sector);
	if (!ifs.read(entry.data(), entry.size())) {
		cerr << "Device read error at: " << outvar(lba) << endl;
		exit(EXIT_FAILURE);
	}

	const Boot* boot = reinterpret_cast<Boot*>(entry.data());
	if (!entry.context.recover && entry.context.noExt() && *boot) {
		cerr << clean << hex << uppercase << 'x' << lba << tab;
		entry.context.sector = boot->sector;
		entry.context.sectors = boot->sectors;
		if (dump(lba, entry)) cerr << endl << endl;
		cerr << boot << endl;
		entry.context.dec();
		confirm();
		return ifs;
	}

	const Index* index = reinterpret_cast<const Index*>(entry.data());
	// if (!entry.context.recover && entry.context.noExt() && entry.context.index && *index) {
	if (!entry.context.recover && entry.context.index && *index) {
		entry.resize(entry.context.sector * entry.context.sectors);
		size_t more = entry.size() - entry.context.sector;
		if (!ifs.read(entry.data() + entry.context.sector, more)) {
			cerr << "Device read error at: " << hex << lba << endl;
			exit(EXIT_FAILURE);
		}
		cerr << clean << hex << uppercase << 'x' << lba << tab;
		const Index* index = reinterpret_cast<const Index*>(entry.data());
		if (dump(lba, entry)) cerr << endl << endl;
		cerr << index << endl;
		entry.context.dec();
		confirm();
		return ifs;
	}

	const Record* record = reinterpret_cast<Record*>(entry.data());
	if (!*record) {
		if (entry.context.debug) {
			cerr << endl << hex << uppercase << 'x' << lba << tab;
			dump(lba, entry);
			cerr << endl;
			confirm();
		}
		return ifs;;
	}

	auto alloc = record->alloc;
	if (alloc > (1<<16)) {
		cerr << endl << "Not resizing to " << outvar(alloc) << endl;
		dump(lba, entry);
		cerr << endl << "Skipping currupted entry:"
			<< hex << uppercase << 'x' << lba << ':' << endl << record;
		confirm();
		return ifs;
	}

	if (alloc != entry.size()) entry.resize(alloc);
	size_t more = alloc - entry.context.sector;
	if (more) {
		if (!ifs.read(entry.data() + entry.context.sector, more)) {
			cerr << endl << "Device read error at: " << hex << lba << endl;
			exit(EXIT_FAILURE);
		}
	}

	entry.resize(record->size);
	if (dump(lba, entry)) cout << endl;
	if (entry.context.verbose) {
		record = reinterpret_cast<Record*>(entry.data());
		cout << endl << hex << uppercase << 'x' << lba << tab << record;
	}
	return ifs;
}
