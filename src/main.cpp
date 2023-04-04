#include <iostream>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <boost/lockfree/queue.hpp>
#include "torrent.hpp"
#include "httprequest.hpp"
#include "message.hpp"

using namespace std;
const char *tor_file = "../misc/debian.torrent";
const char *peer_id = "-pt0001-0123456789ab";
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

    // Initialise queues
    auto work_queue = make_shared<boost::lockfree::queue<uint32_t>>(tor.pieces());
    auto result_queue = make_shared<boost::lockfree::queue<cmn::CompletePiece>>(tor.pieces());

    // work_queue initially contains every piece
    for (uint32_t i = 0; i < tor.pieces(); ++i) {
        while (!work_queue->push(i));
    }

    vector<shared_ptr<Peer>> peers;
    TorrentContext ctx{io, handshake_data, tor, work_queue, result_queue};
    for (auto& peer_address : response.peers()) {
        peers.emplace_back(make_shared<Peer>(ctx, peer_address));
    }
    std::thread writer_thread{[&]() { write_thread(ctx); }};
    io.run();
}

int main () {
    auto tor = SingleFileTorrent::from_file(tor_file);
    auto response = send_request(tor);
    parse_response(tor, response);
}
