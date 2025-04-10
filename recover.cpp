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

	Context context;
	context.parse(n, argv);

	ifstream idev(context.dev, ios::in | ios::binary);
	if (!idev.is_open()) {
		cerr << "Can not open device: " << context.dev << endl
			<< "Error: " << strerror(errno) << endl;
		exit(EXIT_FAILURE);
	}

	LBA lba = context.first;
	if (!idev.seekg(lba * context.sector)) {
		cerr << "Seek error: " << context.dev << endl
			<< "Error: " << strerror(errno) << endl;
		exit(EXIT_FAILURE);
	}

	cerr << "Searching for MFT entries...\n" << endl;
	// scan for NTFS boot sector and MFT entries
	while (idev) {
		lba = idev.tellg() / context.sector;
		if (context.stop(lba)) break;
		Entry entry(context);
		idev >> entry;
		if (!entry) continue;
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
