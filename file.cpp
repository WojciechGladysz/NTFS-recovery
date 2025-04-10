#include <filesystem>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <vector>
#include <utime.h>

#include "helper.hpp"
#include "context.hpp"
#include "entry.hpp"
#include "file.hpp"

using namespace std;

unordered_map<uint64_t, pair<string, uint64_t>> File::dirs;

ostream& operator<<(ostream& os, const Time_t time) {
	os << put_time(localtime(reinterpret_cast<const time_t*>(&time)), "%Y.%m.%d %H:%M:%S");
	return os;
}

/*
 * test file extension belongs to a set of extensions
 * return value depends on weather the match is expected or not
 */
bool File::hit(const set<string>& entries, bool must)		// return means continue checking
{
	if (!valid) return false;								// no further checking
	if (entries.empty()) return true;						// continue other checking
	valid = !must;
	string ext = this->ext;
	lower(ext);
	for (const auto& extension: entries) {
		auto hit = context.mime.find(extension);
		if (hit != context.mime.end())
			if (hit->second.find(ext) != hit->second.end()) {
				valid = must;
				return true;
			}
		if (ext == extension) {
			valid = must;
			return false;
		}
	}
	return false;
}

bool File::empty() const { return !dir && runlist.empty() && !content; }

string File::getType() const {
	string type;
	if (error) type = '!';
	if (!context.undel && !used) type += "not used";
	else if (!valid) type += "not valid";
	else if (empty()) type += "empty";
	else if (!context.force && exists) type += "exist";
	else if (done && context.mask && context.magic != magic) type += "no magic";
	else if (!used) type += dir? "DELETED": "deleted";
	else if (dir) type += "DIR";
	else type += "file";
	return type;
}

void File::mapDir(const Record* record, uint64_t offset) {
	string name;
	uint64_t parent;
	if (dirs.find(record->rec - offset) == dirs.end()) {
		parent = record->getParent(name);
		dirs[record->rec - offset] = make_pair(name, parent);
	}
}

bool File::setPath(const Record* record)
{
	if (!valid && index) return false;
	path = "/";
	bool trash = false;
	uint64_t dir = parent;
	uint64_t last = 0;
	try {
		string name;
		do {
			last = dir;
			tie(name, dir) = dirs.at(dir);
			if (dir == last) break;
			if (name == "$RECYCLE.BIN") trash = true;
			path = "/" + name + path;
		} while (true);
	}
	catch (...) {
		path = "/@" + to_string(last) + path;
		ifstream idev(context.dev, ios::in | ios::binary);
		if (idev.is_open()) {
			int64_t offset = int64_t((last - index) * entry) / context.sector;
			int64_t dir = lba + offset;
			if (idev.seekg(dir * context.sector)) {
				vector<char> buffer(entry);
				idev.read(buffer.data(), buffer.size());
				const Record* parent = reinterpret_cast<const Record*>(buffer.data());
				if (*parent) {
					if (parent->dir())
						mapDir(parent, 0);
					else {
						offset = int64_t((last + (1<<16) - index) * entry) / context.sector;
						dir = lba + offset;
						if (idev.seekg(dir * context.sector)) {
							idev.read(buffer.data(), buffer.size());
							Record* parent = reinterpret_cast<Record*>(buffer.data());
							if (*parent && parent->dir()) mapDir(parent, 1<< 16);
							else {
								error = true;
								return false;
							}
						}
					}
					return setPath(record);
				}
				error = true;
				return false;
			}
		}
	}
	valid = context.recycle || !trash;
	return true;
}

bool File::setBias(const Record* record) const
{
	if (!valid) return false;
	if (!record->rec) return false;
	ifstream idev(context.dev, ios::in | ios::binary);
	if (idev.is_open()) {
		int64_t offset = int64_t(index) * entry / context.sector;
		int64_t first = lba - offset;
		if (idev.seekg(first * context.sector)) {
			vector<char> buffer(entry);
			idev.read(buffer.data(), buffer.size());
			const Record* record = reinterpret_cast<const Record*>(buffer.data());
			File mft(first, record, context);
		}
	}
	return true;
}

File::File(LBA lba, const Record* record, Context& context):
	context(context), error(false), pid(-1),
	valid(false), lba(lba), dir(false), size(0), alloc(0),
	content(nullptr), done(false), exists(false)
{
	if (!record || !*record) return;		// based on entry magic/key word "FILE"
	used = record->used();
	if (!use()) return;						// use if not used and recovering deleted files
	entry = record->alloc;
	index = record->rec;
	dir = record->dir();
	const Attr* next = (const Attr*)(record->key + record->attr);
	while (next) next = next->parse(this);	// parse file entry attributes
	if (hit(context.include, true)) hit(context.exclude, false);	// no file extension match not valid
	setPath(record);
	if (!index && !error) {		// this is MFT file own entry. MFT mirror shall be excluded by !error condition
		if (context.verbose && runlist.empty()) {
			cerr << "No runlist in $MFT file" << endl;
			confirm();
		}
		context.mft.first = lba;
		context.bias = lba - runlist[0].list[0].first * context.sectors;
		context.mft.last = runlist[0].list[0].second * context.sectors + context.bias;
		cerr << clean << "New context LBA bias based on last $MFT record: "
			<< outvar(context.bias) << '@' << outvar(lba) << endl;
		dirs.clear();
	}
	if (valid && index)
		if (lba < context.mft.first || lba >= context.mft.last)
			setBias(record);
}

bool File::use() const { return used || context.undel; }

ostream& operator<<(ostream& os, const File& file) {
	if (file.done && !file.context.all) {
		if (file.dir) { if (file.context.recover || !file.context.dirs) return os; }
		else if (!file.use() || !file.valid || file.exists || file.empty()) return os;
	}
	cerr << clean;			// just print file basic info and return to line begin
	os << hex << uppercase << 'x' << file.lba << tab << file.getType();
	if (file.use()) os<< '/' << dec << file.index << tab << file.path << file.name << tab << file.time;
	if (!file.done) {
		cerr.flush();
		if (file.use() && file.valid) os << "... ";		// just print file basic info and return to line begin
		os.flush();
		return os;
	}
	file.context.dec();
	os << tab;
	if (!file.content) {
		if (file.size) {
			os << "size:" << (file.size > kB? file.size/kB: file.size);
			if (file.size > kB) os << 'k';
			if (!file.runlist.empty())
				for (auto entry: file.runlist) {
					os << tab << entry.second.count << ':';
					for (auto run: entry.second.list)
						// os << outpaix(run.first, run.second) << tab;
						os << outpaix(run.first * file.context.sectors, run.second * file.context.sectors) << tab;
					// os << "max:" << outvar(Runlist::maxLcn * file.context.sectors) << tab;
				}
		}
	}
	else os << "size:" << file.size << tab << "resident";

	if (!file.dir || file.context.recover || !file.context.dirs) return os << endl;
	os << tab << ':';
	for (auto& entry: file.entries) {
		os << tab << entry.first;
		if (file.context.verbose) os << '/' << entry.second;
	}
	return os << endl;
}

void File::recover()
{
	cerr << *this;		// just print file basic info and return to line begin
	if (use() && valid && context.recover && size > context.size * MB && !dir) {
		sem_wait(&context.shared->sem);
		pid = fork();
		if (pid < 0) {
			sem_post(&context.shared->sem);
			cerr << "Failed to create read process, error: " << strerror(errno) << endl;
			cerr << "Continue in main: " << name << endl;
		}
		else if (pid) {		// parent process, return
			int sem;
			sem_getvalue(&context.shared->sem, &sem);
			if (context.verbose) cerr << "New child/" << sem << ':' << pid << '/' << index << ',' << endl;
			return;
		}
	}

	if (use() && valid && !empty())
		if (!error || context.undel)
			if (context.recover ^ dir) {
				ifstream idev(context.dev, ios::in | ios::binary);
				if (idev.is_open())
					idev >> *this;
				else {
					error = true;
					cerr << "Can not open device: " << context.dev << endl
						<< "Error: " << strerror(errno) << endl;
					confirm();
				}
			}

	if (!context.recover || context.all) done = true;

	cout << *this;

	if (!pid) {
		sem_post(&context.shared->sem);
		exit(EXIT_SUCCESS);
	}
}

ifstream& operator>>(ifstream& ifs, File& file)
{
	string full;
	utimbuf times;
	vector<char> buffer(file.context.sector * file.context.sectors);
	streamsize chunk, bytes = file.size;
	if (!file.runlist.empty())
		for (auto entry: file.runlist)
			for (auto run: entry.second.list) {
				int64_t first = run.first * file.context.sectors;
				first += file.context.bias;
				if (first < 0) {
					cerr << clean << "Runlist LBA negative: " << first
						<< ". Try scanning disk device not partition or partition not a file" << endl;
					file.error = true;
					confirm();
					return ifs;
				}
				if (!ifs.seekg(first * file.context.sector))
					if (file.context.verbose) cerr << "Error seeking to lba: " << outpaix(first, ifs.tellg()) << ", error: " << strerror(errno) << endl;
				auto lcn = run.first;
				size_t i = 0;
				for (; lcn < run.second && i < entry.second.count; lcn++, i++) {
					auto chunk = bytes/buffer.size()? buffer.size(): bytes % buffer.size();
					if (!ifs.read(buffer.data(), chunk)) {
						if (file.context.verbose) cerr << "Error reading: "
							<< outpaix(lcn, ifs.tellg()) << ", error: " << strerror(errno) << endl;
						file.error = true;
						goto out;
					}
					bytes -= chunk;
					if (!file.dir) {
						if (!file.ofs.is_open()) {
							file.magic = *reinterpret_cast<uint64_t*>(buffer.data()) & file.context.mask;
							if (file.context.magic && file.magic != file.context.magic) {
								if (Context::verbose) {
									cerr << "No magic/x" << hex << file.context.mask << ':'
										<< outpaix(file.magic, file.context.magic) << ',';
									cerr.write(&file.cmagic, sizeof(file.magic)) << '/';
									cerr.write(&file.context.cmagic, sizeof(file.context.magic)) << endl;
								}
								file.valid = false;
								goto out;
							}
							if (!file.context.shared->show) return ifs;
							if (!file.open()) {
								if (!file.done) file.error = false;
								return ifs;
							}
						}
						file.ofs.write(buffer.data(), chunk);
					}
					else {
						const Index* index = reinterpret_cast<const Index*>(buffer.data());
						if (*index)
							index->header->parse(&file);
						else
							file.error = true;
						if (!*index && file.context.dirs) {
							if (file.context.confirm) cerr << file << endl;
							confirm("Bad INDX cluster");
						}	
					}
				}
			}
	else if (file.content) {
		file.magic = *reinterpret_cast<const uint16_t*>(file.content);
		if (file.context.magic && file.context.magic != (file.magic & file.context.mask))
			file.valid = false;
		else if (!file.open()) return ifs;
		else file.ofs.write(file.content, file.size);
	}
	else		// empty file
	{
		file.done = true;
		return ifs;
	}
	file.done = true;
out:
	if (file.ofs.is_open()) file.ofs.close();
	else return ifs;

	full = file.context.dir + file.path + file.name;
	if (file.error && !file.context.undel) unlink(full.c_str());
	else {
		times = {(time_t)file.access, (time_t)file.time};
		if (utime(full.c_str(), &times)) {
			cerr << "Failed to update file time modification: " << full << ", error: " << strerror(errno) << endl;
			confirm();
		}
	}
	return ifs;
}

void File::mangle() {
	if (context.format == Context::Format::None) return;
	tm* te = localtime(reinterpret_cast<const time_t*>(&time));
	ostringstream path;
	path << '/' << te->tm_year + 1900 << '/';
	if (context.format > Context::Format::Year) {
		path << setfill('0') << setw(2) << (te->tm_mon + 1) << '/';
		if (context.format > Context::Format::Month)
			path << setfill('0')<< setw(2) << (te->tm_mday) << '/' ;
	}
	this->path = path.str();
}

bool File::open()
{
	bool magic = true;
	string target(context.dir);
	mangle();
	target.append(path);
	string full = target + name;
	if (filesystem::exists(full)) {		// if file exists...
		struct stat info;
		stat(full.c_str(), &info);
		if (context.verbose) cerr << "File exists: " << full;
		if (context.magic) {
			union {
				uint64_t	magic;
				char		cmagic;
			} key;
			ifstream file(full, ios::in | ios::binary);
			if (!file.is_open()) cerr << "Can not open existing file to read magic: " << full;
			file.read(&key.cmagic, sizeof(key));
			key.magic &= context.mask;
			if (key.magic != context.magic) magic = false;
		}
		if (magic && (time_t) time >= info.st_mtime		// check existing file time & size against MFT record
				&& size >= info.st_size && !context.force)
		{
			if (context.verbose) cerr << ", and its data seems OK. Skipping" << endl;
			done = exists = true;
			return false;
		}
		if (context.verbose) cerr << ", but will be overwritten" << endl;
	}
	else if (!filesystem::exists(target)) {
		if (context.verbose) cerr << "Creating file target directory: " << target << endl;
		if (!filesystem::create_directories(target)) {
			if (errno != 17) {
				cerr << "Failed to create directory: " << target
					<< ", error/" << (int)errno << ": " << strerror(errno) << endl;
				confirm();
				return false;
			}
		}
		else if (!filesystem::is_directory(target)) {
			cerr << "The path exists and is not a directory: " << target << endl;
			confirm();
			return false;
		}
	}

	ofstream file(full, ios::out | ios::binary);
	if (!file.is_open()) {
		cerr << "Can not open file for write: " << full
			<< ", error: " << strerror(errno) << endl;
		confirm();
		return false;
	}
	if (context.verbose) cerr << "File opened for write: " << full << endl;

	ofs = std::move(file);
	return true;
}
