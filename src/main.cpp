#include <iostream>
#include <string>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include "torrent.hpp"
#include "httprequest.hpp"
#include "message.hpp"

using namespace std;
const char *tor_file = "../misc/debian.torrent";
const char *peer_id = "-pt0001-0123456789ab";
const char *resp_file = "../misc/response.bc";
const uint16_t port = 6881;

TrackerResponse send_request(const SingleFileTorrent& tor) {
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
    return TrackerResponse{string{response.body.begin(), response.body.end()}};
}

void parse_response(const SingleFileTorrent& tor, const TrackerResponse& response) {
    const auto handshake = Handshake{tor.info_hash(), peer_id};
    const auto handshake_data = handshake.serialise();
    boost::asio::io_context io;
    vector<shared_ptr<Peer>> peers;
    for (const auto& addr : response.peers()) {
        peers.emplace_back(make_shared<Peer>(addr, io, handshake_data));
    }
    io.run();
}

int main () {
    auto tor = SingleFileTorrent::from_file(tor_file);
    auto response = send_request(tor);
    parse_response(tor, response);
}
