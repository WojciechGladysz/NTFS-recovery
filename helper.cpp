#include <iostream>
#include <iomanip>

#include "helper.hpp"
#include "context.hpp"

using namespace std;

bool ldump(const void* data, uint length, uint start) {
    if (!Context::debug) return false;
    uint end = start + length;
    string txt; // for ASCII display
    const auto align = 32;
    auto offset(0);
    const char* chars = static_cast<const char*>(data);
    for (uint i = start; i < end; i++) {
        uint8_t c = chars[i];
        if (!(offset % align)) cout << setw(4) << dec << offset << 'x' << setw(3) << uppercase << hex << offset << ": ";
        cout << std::hex << std::setw(2) << static_cast<uint16_t>(c) << ' ';
        txt.push_back(isprint(c)? c: ' ');
        offset++;
        if (!(offset % 8) && (offset % align)) {
            cout << '|';
            txt.push_back('|');
        }
        if (!(offset % align)) {
            cout << '\t' << txt << endl;
            txt.clear();
        }
    }
    if (length > align) while (offset % align) {
        offset++;
        cout << "_ _";
        if (!(offset % 8) && (offset % align))
            cout << '|';
        if (!(offset % align))
            cout << '\t' << txt << endl;
    }
    else cout << tab << txt << endl;
    return true;
}

bool pdump(const void* start, const void* end) {
    uint length = reinterpret_cast<const char*>(end) - reinterpret_cast<const char*>(start);
    return ldump(start, length);
}

bool dump(LBA lba, const vector<char>& data) {
    if (!Context::debug) return false;
    cout << "offset: " << outvar(lba) << endl;
    return ldump(data.data(), (uint)data.size()); 
}

string& lower(string& text) {
    for (auto& c: text)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

void confirm() {
    if (!Context::confirm) return;
    cerr << "\tPress a key to continue...";
    cin.get();
}
