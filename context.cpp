#include <iostream>
#include <fstream>
#include <sstream>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <thread>

#include "helper.hpp"
#include "context.hpp"

bool Context::verbose = false;
bool Context::debug = false;
bool Context::confirm = false;

bool Context::set(options& option, const char* arg)		// return value means data consumed
{
	if (get_if<monostate>(&option)) return false;
	if (!*arg) return true;
	try {
		if (auto param = get_if<LBA*>(&option)) **param = stoull(arg, nullptr, 0);
		else if (auto param = get_if<int64_t*>(&option)) **param = stoll(arg, nullptr, 0);
		else if (auto param = get_if<string*>(&option)) **param = arg;
		else if (auto param = get_if<function<void(Context*,const char*)>>(&option)) (*param)(this, arg);
	}
	catch (exception e) {}
	option = monostate{};
	return true;
}

void Context::parse(size_t n, char** argv)
{
	options option = monostate{};

	for (int i = 1; i < n; i++) {
		char* arg = argv[i];
		if (*arg == '-') {
			option = monostate{};
			while (*++arg){
				// no parameter options
				if (*arg == 'h') help = true;
				else if (*arg == 'R') recover = true;
				else if (*arg == 'u') undel = true;
				else if (*arg == 'c') confirm = true;
				else if (*arg == 'a') all = true;
				else if (*arg == 'd') dirs = true;
				else if (*arg == 'f') force = true;
				else if (*arg == 'X') index = true;
				else if (*arg == 'r') recycle = true;
				else if (*arg == 'Y') format = Context::Format::Year;
				else if (*arg == 'M') format = Context::Format::Month;
				else if (*arg == 'D') format = Context::Format::Day;
				else if (*arg == 'v') if (verbose) debug = true; else verbose = true;
				// options with parameter
				else if (*arg == 'l') option = &first;
				else if (*arg == 'L') option = &last;
				else if (*arg == 'n') option = &shared->count;
				else if (*arg == 's') option = &shared->show;
				else if (*arg == 'S') option = &size;
				else if (*arg == 'm') option = &Context::signature;
				else if (*arg == 'i') option = &Context::addInclude;
				else if (*arg == 'x') option = &Context::addExclude;
				else if (*arg == 't') option = &dir;
				else if (*arg == 'p') option = &Context::setSem;
				if (set(option, arg + 1)) break;
			}
		}
		else if (option.index()) set(option, arg);
		else dev = arg;
	}

	if (help) cerr << argv[0] << R"EOF( [Options] DEV

Parameter:
DEV	device/partition/image/file to open, example: /dev/sdc, /dev/sdd1, ./$MFT

Options:
		no parameter option may be combined under one leading -
		space after option letter may be omitted, parameters are consumed until next space
-h	display this help message and quit, helpfull to see other argument parsed
-l lba	device LBA to start the scan from, hex is ok with 0x
-L lba	device LBA to stop the scan before, hex is ok with 0x
-t dir	recovery target/output directory/mount point, defaults to current directory
-R	recover data to target directory, otherwise dry run
-u	recover deleted files
-f	overwrite target file if exists, files may get overwritten anyway
-n N	number of entries scanned, NTFS boot sector, MFT entry or just LBA count
-s N	number of entries to process
-m N	magic word to search at the beggining of a file to recover, text or hex (with 0x)
	max 8 bytes, effective bytes until most significant not null
-i x,y	only files with extensions separated with comma (no spaces),
-x x,y	exclude files with extensions separated with comma (no spaces)
		mime types are OK, example: image, video, audio
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
-S N	size of a file in MB to start a new thread for the file recovery, default 16MB
-c	stop to confirm some actions

Example:
ntfs.recovery /dev/sdb1				# dry run, scan partition /dev/sdb1
ntfs.recovery /dev/sdc -t recovered -R		# recover files from disk /dev/sdc to recovered dir

)EOF";
	cerr << "Parsed arguments:\n" << *this;

	if (dev.empty())
		cerr << "Give device/file name. For example /dev/sdb, /dev/sdc2" << endl;
	if (dev.empty() || help) exit(EXIT_SUCCESS);
}

ostream& operator<<(ostream& oss, const Context& context) {
	oss << "Device:" << context.dev << ", "
		<< "LBA:" << outvar(context.first);
	if (context.last) oss << " >> " << outvar(context.last);
	oss << ", ";
	if (context.undel) oss << "include deleted, ";
	if (context.shared->count > 0) oss << "count:" << context.shared->count << ", ";
	if (context.shared->show > 0) oss << "process:" << context.shared->show << ", ";
	if (context.all) oss << "show all, ";
	else if (context.dirs) oss << "show dirs, ";
	if (context.index) oss << "show indx, ";
	if (context.magic) {
		oss << "magic:" << hex << uppercase << 'x' << context.magic << '/';
		cerr.write(&context.cmagic, sizeof(context.magic)) << ", ";
	}
	if (!context.include.empty()) {
		oss << "only[";
		for (auto extension: context.include) oss << extension << ",";
		oss << "\b] ";
	}
	if (!context.exclude.empty()) {
		oss << "exclude[";
		for (auto extension: context.exclude) oss << extension << ",";
		oss << "\b] ";
	}
	if (context.childs != thread::hardware_concurrency()) oss << "child:" << context.childs << ", ";
	if (context.size != 16) oss << "big:" << dec << context.size << "MB, ";
	if (context.verbose) {
		if (context.debug) oss << "debug, ";
		else oss << "verbose, ";
	}
	if (context.confirm) oss << "confirm, ";

	if (context.format != Context::Format::None) {
		oss << "path:/yyyy/";
		if (context.format > Context::Format::Year) oss << "mm/";
		if (context.format > Context::Format::Month) oss << "dd/";
		oss << ", ";
	}

	if (context.recycle) oss << "include recycle bin, ";
	oss << "pid:" << getpid() << endl;
	if (context.recover) {
		oss << "RECOVER to target dir: " << context.dir;
		if (context.force) oss << ", overwrite existing files";
		oss << endl;
	}
	if (context.last && context.first > context.last) cerr << "End LBA lower that start LBA" << endl;
	if (!context.help)
		if (context.recover || context.confirm) {
			cerr << endl << "\tPress enter to confirm...";
			cin.get();
		}
	return oss << endl;
}

void Context::signature(const char* arg) {
	try {
		magic = stoll(arg, nullptr, 0);
	}
	catch (...) {
		if (!magic) {
			string key(arg);
			key.copy(&cmagic, sizeof(magic));
		}
	}
	uint64_t temp = magic;
	mask = 0;
	while (temp) {
		mask =  (mask << 8) + 0xFF;
		temp >>= 8;
	}
}

Context::Context(): dir("."), sector(512), sectors(8) {
	first = last = bias = mft.first = mft.last = 0;
	mft.size = 1024;
	magic = mask = 0;
	verbose = debug = confirm = recover = undel = all = force = index = recycle = dirs = help = false;
	format = Context::Format::None;
	size = 16;     // 16MB
	childs = thread::hardware_concurrency()?:4;
	shared = (Shared*)mmap(NULL, sizeof(Shared), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	sem_init(&shared->sem, 1, 4);
	shared->count = -1L;
	shared->show = -1L;
	while (dir.back() == '/') dir.pop_back();
	ifstream mime("/etc/mime.types");
	string line, type, extensions, file;
	if (mime.is_open())
		while(getline(mime, line))
			if (!line.empty() && line.front() != '#') {
				istringstream entry(line);
				getline(entry, type, '/');
				getline(entry, extensions);
				istringstream extension(extensions);
				extension >> file;
				while (extension >> file) this->mime[type].insert(file);
			}
}

void Context::parse(const string& types, std::set<string>& set) {
	istringstream iss(types);
	string name;
	while (getline(iss, name, ',')) set.insert(lower(name));
}
