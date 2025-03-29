#include <iostream>
#include <cstring>
#include <sys/wait.h>
#include <variant>
#include <functional>

#include "helper.hpp"
#include "context.hpp"
#include "entry.hpp"
#include "file.hpp"

using namespace std;
using namespace filesystem;

int main(int n, char** argv) {
	bool help = false;

	Context context;
	using options = variant<monostate, LBA*, int64_t*, string*, function<void(Context*, const char*)>>;
	options option = monostate{};

	auto set = [&context](options& option, const char* arg) -> bool {	// return value means data consumed
		if (!*arg) return false;
		else if (auto param = get_if<monostate>(&option)) return false;
		else if (auto param = get_if<LBA*>(&option)) **param = strtol(arg, nullptr, 0);
		else if (auto param = get_if<int64_t*>(&option)) **param = strtol(arg, nullptr, 0);
		else if (auto param = get_if<string*>(&option)) **param = arg;
		else if (auto param = get_if<function<void(Context*,const char*)>>(&option)) (*param)(&context, arg);
		option = monostate{};
		return true;
	};

	for (int i = 1; i < n; i++) {
		char* arg = argv[i];
		if (*arg == '-') {
			option = monostate{};
			while (*++arg){
				// no parametar options
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
				// options with parameter
				else if (*arg == 'l') option = &context.first;
				else if (*arg == 'L') option = &context.last;
				else if (*arg == 'n') option = context.count;
				else if (*arg == 's') option = context.show;
				else if (*arg == 'S') option = &context.size;
				else if (*arg == 'm') option = &Context::signature;
				else if (*arg == 'i') option = &Context::addInclude;
				else if (*arg == 'x') option = &Context::addExclude;
				else if (*arg == 't') option = &context.dir;
				else if (*arg == 'v') if (context.verbose) context.debug = true; else context.verbose = true;
				else if (*arg == 'p') option = &Context::setSem;
				if (set(option, arg + 1)) break;
			}
		}
		else if (option.index()) set(option, arg);
		else context.dev = arg;
	}

	if (help) cout << endl << argv[0] << R"EOF( [Options] DEV

Paremeter:
DEV	device/partition/image/file to open, example: /dev/sdc, /dev/sdd1, ./$MFT

Options: space after option letter may be omitted, parameters are consumed until next space
-h	display this help message and quit, helpfull to see other argument parsed
-l lba	device LBA to start the scan from, hex is ok with 0x
-L lba	device LBA to stop the scan before, hex is ok with 0x
-t dir	recovery target/output directory/mount point, defaults to current directory
-R	recover data to target directory, otherwise dry run
-f	overwrite target file if exists, files may get overwritten anyway
-n N	number of entries scanned, NTFS boot sector, MFT entry or just LBA count
-s N	number of entries to process
-m nnn	magic word to search at the beggining of a file to recover, text or hex (with 0x)
	max 8 bytes, effective bytes until most significant not null
-i x,y	allow files with extensions separated by comma (no spaces),
	mime types are OK, example: image, video, audio
-x x,y	exclude files with extensions separated with comma (no spaces)
	mime types are OK, example: image, video
-r	recover files from recycle bin
-v	be verbose, if repeated be more verbose with debug info
-d	show directories
-Y	file path under target directory will be altered to /yyyy/
-M	file path under target directory will be altered to /yyyy/mm/
-D	file path under target directory will be altered to /yyyy/mm/dd/
	based on file original modifaction time, useful for media files recovery
-X	show index allocations
-a	show all entries including invalid or skipped otherwise
-p N	max number of child processes for big file recovery, defaults to hardware capability
-S nn	size of a file in MB to start a new thread for the file recovery, default 16MB
-c	stop to confirm some actions

Example:
	ntfs.recovery /dev/sdb -R
	ntfs.recovery /dev/sdc1 -t recovery -R

Parsed arguments:
)EOF";

	if (context.dev.empty()) {
		cerr << "Give device/file name. For example /dev/sdb, /dev/sdc2" << endl;
		exit(EXIT_SUCCESS);
	}

	cerr << context;

	if (help) exit(EXIT_SUCCESS);

	ifstream idev(context.dev, ios::in | ios::binary);
	if (!idev.is_open()) {
		cerr << "Can not open device: " << context.dev << endl
			<< "Error: " << strerror(errno) << endl;
		exit(EXIT_FAILURE);
	}

	LBA lba = context.first;
	if (!idev.seekg(lba * context.sector)) {
		cerr << "Device error: " << context.dev << endl
			<< "Error: " << strerror(errno) << endl;
		exit(EXIT_FAILURE);
	}

	// scan for NTFS boot sector and MFT entries
	cerr << "Searching for MFT entries...\n" << endl;

	while (idev) {
		lba = idev.tellg() / context.sector;
		if (context.stop(lba)) break;
		Entry entry(context);
		idev >> entry;
		if (!*entry.record()) continue;
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
