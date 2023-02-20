#include <iostream>
#include <string>
#include "torrent.hpp"

using namespace std;
const char *filename = "../misc/debian.torrent";

int main () {
    auto tor = SingleFileTorrent::from_file(filename);
    cout << tor.filename() << " (" << tor.info_hash() << ") @ " << tor.announce() << "\n";
    return 0;
}
