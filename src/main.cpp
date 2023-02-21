#include <iostream>
#include <string>
#include "torrent.hpp"
#include "httprequest.hpp"

using namespace std;
const char *filename = "../misc/debian.torrent";
const char *peer_id = "PCT001k1k1k1k1k1k1k1";
const uint16_t port = 6881;

int main () {
    auto tor = SingleFileTorrent::from_file(filename);
    cout << tor.filename() << " (" << tor.info_hash().as_hex() << ") @ " << tor.announce() << "\n";

    stringstream request;
    request << tor.announce() << "?"
            << "info_hash=" << cmn::urlencode(tor.info_hash().as_bytes()) << "&"
            << "peer_id=" << peer_id << "&"
            << "port=" << port << "&"
            << "uploaded=" << "0" << "&"
            << "downloaded=" << "0" << "&"
            << "compact=" << "1" << "&"
            << "left=" << tor.file_length();

    cout << "sending request: " << request.str() << "\n";
    http::Request req{request.str()};
    const auto response = req.send("GET");
    cout << "response: " << string{response.body.begin(), response.body.end()} << "\n";
    return 0;
}
