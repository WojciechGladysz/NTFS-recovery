#include <iostream>
#include <cstring>
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
	bool pend = false;				// next argument shall be target directory

	for (int i = 1; i < n; i++) {
		char* arg = argv[i];
		if (*arg == '-') {
			pend = false;
			while (*++arg){
				if (*arg == 'h') help = true;
				else if (*arg == 'R') context.recover = true;
				else if (*arg == 'c') context.confirm = true;
				else if (*arg == 'a') context.all = true;
				else if (*arg == 'd') context.dirs = true;
				else if (*arg == 'f') context.force = true;
				else if (*arg == 'X') context.index = true;
				else if (*arg == 'r') context.recycle = true;
				else if (*arg == 'Y') context.format = Context::Format::Year;
				else if (*arg == 'M') context.format = Context::Format::Month;
				else if (*arg == 'D') context.format = Context::Format::Day;
				else if (*arg == 'v') if (context.verbose) context.debug = true; else context.verbose = true;
				else if (*arg == 'l') { context.first = strtol(++arg, nullptr, 0); break; }
				else if (*arg == 'L') { context.last = strtol(++arg, nullptr, 0); break; }
				else if (*arg == 'n') { *context.count = strtol(++arg, nullptr, 0); break; }
				else if (*arg == 'm') { context.signature(++arg); break; }
				else if (*arg == 's') { *context.show = strtol(++arg, nullptr, 0); break; }
				else if (*arg == 'S') { context.size = strtol(++arg, nullptr, 0) * kB; break; }
				else if (*arg == 'i') { context.parse(++arg, context.include); break; }
				else if (*arg == 'x') { context.parse(++arg, context.exclude); break; }
				else if (*arg == 't') {
					if (!*++arg) pend = true;
					else context.dir = arg;
					break;
				}
				else if (*arg == 'p') {
					sem_destroy(context.sem);
					context.childs = strtol(++arg, nullptr, 0);
					sem_init(context.sem, 1, context.childs);
					break;
				}
			}
		}
		else if (pend) context.dir = arg;
		else context.dev = arg;
	}

	if (help) cout << endl << argv[0] << R"EOF( [Options] dev

Paremeter:
dev		device/partition/file to open, example: /dev/sdc, /dev/sdd1, ./$MFT

Options:
-h		display this help message and quit, helpfull to see other argument parsed
-l		device LBA to start the scan from, hex is ok with 0x
-L		device LBA to stop the scan before, hex is ok with 0x
-t		recovery target/output directory/mount point, defaults to current directory
-R		recover data to target directory, otherwise dry run
-f		overwrite target file if exists, if file size is lower than MFT entry it will be overwitten
-n		number of entries scanned, NTFS boot sector, MFT entry or just LBA count
-s		number of entries to process/show
-m		magic word to search at the beggining of a file to recover, hex is ok with 0x
		max 8 bytes, effective bytes until most significant not null
-i		allow files with extensions separated with comma (no spaces),
		mime types are OK, example: image, video, audio
-x		exclude files with extensions separated with comma (no spaces)
		mime types are OK, example: image, video
-r		recover files from recycle bin
-v		be verbose, if repeated be more verbose with debug info
-d		show directories
-Y		file path under target directory will be altered to /yyyy/
-M		file path under target directory will be altered to /yyyy/mm/
-D		file path under target directory will be altered to /yyyy/mm/dd/
		based on file original modifaction time, useful for media files recovery
-X		show index allocations
-a		show all entries including invalid or skipped
-p		max number of child processes for big files recovery, default 4
-S		size of file in MB to start a new thread for the file recovery, default 16MB
-c		stop to confirm some actions

Parsed arguments:
)EOF";

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

	// scan for NTFS boot sector and MFT entries
	cerr << "Searching for MFT entries...\n" << endl;

	LBA lba = context.first;

	if (!idev.seekg(lba * context.sector)) {
		cerr << "Device error: " << context.dev << endl
			<< "Error: " << strerror(errno) << endl;
		exit(EXIT_FAILURE);
	}

	while (idev) {
		lba = idev.tellg() / context.sector;
		if (context.stop(lba)) break;
		Entry entry(context);
		idev >> entry;
		if (!entry.record()) continue;
		File file(lba, entry.record(), context);
		file.recover();
		waitpid(-1, NULL, WNOHANG);
	}

	idev.close();
	cerr << "\nWait for child processes... " << endl;
	int id;
	while (id = wait(NULL), id > -1) cerr << "pid " << id << " done, ";
	return 0;
}
