#include <iostream>
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

#include "helper.hpp"
#include "context.hpp"
#include "entry.hpp"
#include "file.hpp"

using namespace std;
using namespace filesystem;

int main(int n, char** argv) {
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
            if (*arg == 'Y') { context.format = Context::Format::Year; continue; }
            if (*arg == 'M') { context.format = Context::Format::Month; continue; }
            if (*arg == 'D') { context.format = Context::Format::Day; continue; }
            if (*arg == 'f') { context.force = true; continue; }
            if (*arg == 'd') { context.dev = string(++arg); break; }
            if (*arg == 'l') { context.first = strtol(++arg, nullptr, 0); break; }
            if (*arg == 'L') { context.last = strtol(++arg, nullptr, 0); break; }
            if (*arg == 'n') { context.count = strtol(++arg, nullptr, 0); break; }
            if (*arg == 'm') { context.magic = strtol(++arg, nullptr, 0); break; }
            if (*arg == 's') { context.show = strtol(++arg, nullptr, 0); break; }
            if (*arg == 'S') { context.size = strtol(++arg, nullptr, 0) * (1 << 20 ); break; }
            if (*arg == 'i') { context.parse(++arg, context.include); break; }
            if (*arg == 'x') { context.parse(++arg, context.exclude); break; }
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
                context.childs = strtol(++arg, nullptr, 0);
                sem_init(context.sem, 1, context.childs);
                break;
            }
        }
    }

    if (help) cout << R"EOF(options:
-h      display this help message
-d      device or file to open, example: /dev/sdc, /dev/sdd1, ./$MFT
-l      device LBA to start the scan from, hex is ok with 0x
-L      device LBA to stop the scan before, hex is ok with 0x
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

    cerr << context;
    
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

    // scan for NTFS boot sector
    cerr << "Searching for MFT entries...\n" << endl;

    LBA lba = context.first;
    
    if (!idev.seekg(lba * context.sector)) {
        cerr << "Device error: " << context.dev << endl
            << "Error: " << strerror(errno) << endl;
        exit(EXIT_FAILURE);
    }

    while (!idev.eof() && context.count--) {
        lba = idev.tellg() / context.sector;
        if (context.last && lba >= context.last) break;
        Entry entry(context);
        idev >> entry;
        if (!entry) continue;
        File file(lba, entry.record(), context);
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
