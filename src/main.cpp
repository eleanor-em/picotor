#include <iostream>
#include <unordered_set>

#include <boost/asio.hpp>
#include <boost/lockfree/queue.hpp>

#include <torrent.hpp>
#include <httprequest.hpp>
#include <message.hpp>
#include <peer.hpp>
#include <result.hpp>

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

void start_run_connections(const SingleFileTorrent& tor, const TrackerResponse& response) {
    const auto handshake = Handshake{tor.info_hash(), peer_id}.serialise();
    boost::asio::io_context io;

    // initialise queues
    auto work_queue = make_shared<boost::lockfree::queue<uint32_t>>(tor.pieces());
    auto result_queue = make_shared<boost::lockfree::queue<Result>>(tor.pieces());

    // work_queue initially contains every piece
    for (uint32_t i = 0; i < tor.pieces(); ++i) {
        while (!work_queue->push(i));
    }

    auto peers = std::make_unique<vector<Peer>>();
    peers->reserve(response.peers().size());
    TorrentContext ctx{io, handshake, tor, work_queue, result_queue, response.peers().size()};
    for (auto& peer_address : response.peers()) {
        peers->emplace_back(ctx, peer_address);
    }

    std::thread monitor{[&ctx, peers = std::move(peers)]() {
        monitor_thread(ctx, std::move(peers));
    }};

    io.run();
}

int main() {
    auto tor = SingleFileTorrent::from_file(tor_file);
    auto response = send_request(tor);
    start_run_connections(tor, response);
}
