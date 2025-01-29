#include <string.h>

#include "entry.hpp"
#include "context.hpp"
#include "attr.hpp"
#include "helper.hpp"

const uint8_t Boot::jmp[] = {0xeb, 0x52, 0x90};

struct Header;

struct __attribute__ ((packed)) Index {
    char        key[4];
    uint16_t    fixup;
    uint16_t    entries;

    uint64_t    logSeq;
    uint64_t    vnc;

    Header      header[];
    operator bool() const;
};

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
    os << "boot: " << boot->oemId << endl
        << "bytes per sector: " << outvar(boot->sector) << endl
        << "sectors per cluster: " << outchar(boot->sectors) << endl
        << "media: " << outchar(boot->mediaId) << endl
        << "sectors per track: " << outchar(boot->sectorsPerTrack) << endl
        << "heads: " << outchar(boot->heads) << endl
        << "hidden: " << outvar(boot->hidden) << endl
        << "total: " << outvar(boot->total) << tab << (total/(1LL<<30)) << "G" << endl
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
    return os;
}

ostream& operator<<(ostream& os, const Record* record) {
    uint32_t* endTag = (uint32_t*)(record->key + record->size) - 2;
    os << "Entry: ";
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
        << "flags: " << outvar(record->flags) << endl
        << "size: " << outvar(record->size) << endl
        << "allocated: " << outvar(record->alloc) << endl
        << "base: " << outvar(record->base) << endl
        << "next: " << outvar(record->next) << endl
        << endl
        << "Attributes:\n" << endl;
    const Attr* next = (Attr*)(record->key + record->attr);
    while (next) {
        os << '@' << hex << ((char*)next - record->key) << '.';
        os << next << endl;
        next = next->getNext();
    }
    return os;
}

Entry::Entry(Context& context): context(context) {}

Entry::operator bool() const {
    return !strncmp(reinterpret_cast<const Record*>(data())->key, "FILE", 4);
}

ifstream& operator>>(ifstream& ifs, Entry& entry)
{
    LBA lba = ifs.tellg() / entry.context.sector;
    entry.resize(entry.context.sector);
    if (!ifs.read(entry.data(), entry.size())) {
        cerr << "Device read error at: " << outvar(lba) << endl;
        exit(EXIT_FAILURE);
    }

    Boot* boot = reinterpret_cast<Boot*>(entry.data());
    if (*boot) {
        cout << '\r' << hex << uppercase << 'x' << lba << '\t';
        entry.context.sector = boot->sector;
        entry.context.sectors = boot->sectors;
        dump(lba, entry);
        cout << boot;
        entry.context.dec();
        confirm();
        return ifs;
    }

    Index* index = reinterpret_cast<Index*>(entry.data());
    if (entry.context.index && *index) {
        entry.resize(entry.context.sector * entry.context.sectors);
        size_t more = entry.size() - entry.context.sector;
        if (!ifs.read(entry.data() + entry.context.sector, more)) {
            cerr << "Device read error at: " << hex << lba << endl;
            exit(EXIT_FAILURE);
        }
        cout << '\r' << hex << uppercase << 'x' << lba << '\t';
        Index* index = reinterpret_cast<Index*>(entry.data());
        dump(lba, entry);
        cout << index;
        entry.context.dec();
        confirm();
        return ifs;
    }

    if (!entry) {
        if (entry.context.debug) {
            cout << endl;
            dump(lba, entry);
            confirm();
        }
        return ifs;;
    }

    Record* record = reinterpret_cast<Record*>(entry.data());
    auto alloc = record->alloc;
    if (alloc > (1<<16)) {
        cout << "Not resizing to " << outvar(alloc) << endl;
        dump(lba, entry);
        cout << endl << "Skipping currupted entry:"
            << hex << uppercase << 'x' << lba << ':' << endl << record;
        confirm();
        return ifs;
    }

    if (alloc != entry.size()) entry.resize(alloc);
    size_t more = alloc - entry.context.sector;
    if (more) {
        if (!ifs.read(entry.data() + entry.context.sector, more)) {
            cerr << "Device read error at: " << hex << lba << endl;
            exit(EXIT_FAILURE);
        }
    }

    auto size = record->size;
    entry.resize(size);
    dump(lba, entry);
    if (entry.context.debug) cout << endl;
    if (entry.context.verbose) {
        record = reinterpret_cast<Record*>(entry.data());
        cout << hex << uppercase << 'x' << lba << ':' << endl << record;
    }
    return ifs;
}
