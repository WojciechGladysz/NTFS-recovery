#include <fstream>
#include <filesystem>
#include <vector>
#include <list>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <sys/wait.h>

#include "ntfs.hpp"

using namespace std;
using namespace filesystem;

bool Context::verbose = false;
bool Context::debug = false;
bool Context::confirm = false;

int main(int n, char** argv) {
    uint sectors = 8;
    bool help = false;

    Context context;

    for (int i = 1; i < n; i++) {
        char* arg = argv[i];
        if (*arg == '-') 
        while (*++arg){
            if (*arg == 'R') { context.recover = true; continue; }
            if (*arg == 'c') { context.confirm = true; continue; }
            if (*arg == 'a') { context.all = true; continue; }
            if (*arg == 'X') { context.extra = true; continue; }
            if (*arg == 'h') { help = true; continue; }
            if (*arg == 'v') {
                if (context.verbose) context.debug = true;
                context.verbose = true;
                continue;
            }
            if (*arg == 'Y') { context.format = Format::Year; continue; }
            if (*arg == 'M') { context.format = Format::Month; continue; }
            if (*arg == 'D') { context.format = Format::Day; continue; }
            if (*arg == 'f') { context.force = true; continue; }
            if (*arg == 'd') { context.dev = string(++arg); break; }
            if (*arg == 'l') { context.start = strtol(++arg, nullptr, 0); break; }
            if (*arg == 'L') { context.stop = strtol(++arg, nullptr, 0); break; }
            if (*arg == 'n') { context.count = strtol(++arg, nullptr, 0); break; }
            if (*arg == 'm') { context.magic = strtol(++arg, nullptr, 0); break; }
            if (*arg == 's') { context.show = strtol(++arg, nullptr, 0); break; }
            if (*arg == 'S') { context.size = strtol(++arg, nullptr, 0) * (1 << 20 ); break; }
            if (*arg == 'i') { context.extensions(++arg); break; }
            if (*arg == 'x') { context.exclutions(++arg); break; }
            if (*arg == 'o') {
                if (!*++arg) {
                    cerr << "Empty output directory. Aborting" << endl;
                    exit(EXIT_FAILURE);
                }
                context.dir = arg;
                break;
            }
            if (*arg == 't') {
                sem_destroy(context.sem);
                context.threads = strtol(++arg, nullptr, 0);
                sem_init(context.sem, 1, context.threads);
                break;
            }
        }
    }

    if (help) cout << R"EOF(options:
-h      display this help message
-d      device or file to open, example: /dev/sdc, /dev/sdd1, ./$MFT
-l      device first LBA to start the scan at, hex is ok with 0x
-L      device last LBA to stop the scan at, hex is ok with 0x
-o      recovery target directory/mount point, defaults to current directory
-R      recover data to target directory, otherwise dry run
-f      overwrite target file if exists, if file size is lower than MFT entry it will be overwitten
-n      number of entries scanned, NTFS boot sector, MFT entry or just LBA count
-s      number of entries to show
-m      magic word to search at the beggining of a file to recover, hex is ok with 0x
        max 8 bytes, effective bytes until most significant not 0
-i      allow files with extensions separated with comma (no spaces),
        mime types are OK, example: image, video
-x      exclude files with extensions separated with comma (no spaces)
        mime types are OK, example: image, video
-v      be verbose, if repeated be more verbose with debug info
-Y      file path under target directory will be altered to /yyyy/
-M      file path under target directory will be altered to /yyyy/mm/
-D      file path under target directory will be altered to /yyyy/mm/dd/
        based on file original modifaction time, useful for media files recovery
-X      create extra files ntfs.dirs, ntfs.exts in current directory with scanned
        directory entries and file extensions
-a      show all entries including invalid or skipped
-t      max number of child processes for big files recovery, see option -s
-S      size of file in MB to start a new thread for the file recovery, default 16MB
-c      confirm possible errors

Parsed arguments:
)EOF" << endl;

    cerr << context << endl;
    
    if (help) exit(EXIT_SUCCESS);

    if (context.dev.empty()) {
        cerr << "Device name not provided. Aborting" << endl;
        exit(EXIT_FAILURE);
    }

    ifstream idev(context.dev, ios::in | ios::binary);
    if (!idev.is_open()) {
        cerr << "Can not open device: " << context.dev << endl
            << "Error: " << strerror(errno) << endl;
        exit(EXIT_FAILURE);
    }

    // give up root previlages
    const char* sudoUid = std::getenv("SUDO_UID");
    const char* sudoGid = std::getenv("SUDO_GID");
    if (sudoUid && sudoGid) {
        context.user = strtol(sudoUid, nullptr, 0);
        context.group = strtol(sudoGid, nullptr, 0);
    }

    vector<char> buffer(sectorSize);

    // scan for NTFS boot sector
    cerr << "Searching for MFT entries...\n" << endl;

    LBA lba = context.start;
    
    if (!idev.seekg(lba * sectorSize)) {
        cerr << "Device error: " << context.dev << endl
            << "Error: " << strerror(errno) << endl;
        exit(EXIT_FAILURE);
    }

    while (!idev.eof() && context.count--) {
        lba = idev.tellg() / sectorSize;
        if (context.stop && lba >= context.stop) break;
        buffer.resize(sectorSize);
        if (!idev.read(buffer.data(), sectorSize)) {
            cerr << "Device read error at: " << outvar(lba) << endl;
            exit(EXIT_FAILURE);
        }
        Boot* boot = reinterpret_cast<Boot*>(buffer.data());
        if (*boot) {
            sectors = boot->sectorsPerCluster;
            dump(lba, buffer);
            cout << boot;
            confirm();
            continue;
        }

        Index* index = reinterpret_cast<Index*>(buffer.data());
        if (*index) {
            buffer.resize(sectorSize * sectorsPerCluster);
            size_t more = buffer.size() - sectorSize;
            if (!idev.read(buffer.data() + sectorSize, more)) {
                cerr << "Device read error at: " << hex << lba << endl;
                exit(EXIT_FAILURE);
            }
            cout << '\r' << hex << uppercase << 'x' << lba << '\t';
            Index* index = reinterpret_cast<Index*>(buffer.data());
            dump(lba, buffer);
            cout << index;
            confirm();
            continue;
        }

        Entry* entry = reinterpret_cast<Entry*>(buffer.data());
        if (!*entry) {
            if (context.debug) {
                cout << endl;
                dump(lba, buffer);
                confirm();
            }
            continue;
        }

        auto alloc = entry->alloc;
        if (alloc > (1<<16)) {
            cout << "Not resizing to " << outvar(alloc) << endl;
            dump(lba, buffer);
            cout << endl << "Skipping currupted entry:"
                << hex << uppercase << 'x' << lba << ':' << endl << entry;
            confirm();
            continue;
        }
        if (alloc != buffer.size()) buffer.resize(alloc);
        size_t more = alloc - sectorSize;
        if (more) {
            if (!idev.read(buffer.data() + sectorSize, more)) {
                cerr << "Device read error at: " << hex << lba << endl;
                exit(EXIT_FAILURE);
            }
        }
        auto size = entry->size;
        buffer.resize(size);
        dump(lba, buffer);
        if (context.debug) cout << endl;
        if (context.verbose) {
            entry = reinterpret_cast<Entry*>(buffer.data());
            cout << hex << uppercase << 'x' << lba << ':' << endl << entry;
        }
        File file(lba, buffer, context);
        file.recover();
        waitpid(-1, NULL, WNOHANG);
    }
    idev.close();
    cerr << "\033[2K\r";
    if (context.verbose) {
        cerr << "Wait for child processes... ";
        int id;
        while (id = wait(nullptr), id > 0)
            cerr << id << ',';
    }
    cerr << "done" << endl;
    return 0;
}
