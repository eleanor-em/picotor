//
// Created by eleanor on 20.02.23.
//

#ifndef PICOTOR_TORRENT_HPP
#define PICOTOR_TORRENT_HPP
#include <common.hpp>
#include <string>
#include <vector>
#include <boost/asio.hpp>
#include <boost/lockfree/queue.hpp>
#include "message.hpp"

namespace ba = boost::asio;
using ba::ip::tcp;
using namespace std;

static uint32_t PIPELINE_LIMIT = 5;

class SingleFileTorrent {
public:
    explicit SingleFileTorrent(const string_view &data);
    static SingleFileTorrent from_file(const string_view& path) {
        return SingleFileTorrent(cmn::read_file(path));
    }

    const string& announce() const {
        return announce_;
    }

    const string& filename() const {
        return filename_;
    }

    const cmn::Hash& info_hash() const {
        return info_hash_;
    }

    uint32_t file_length() const {
        return file_length_;
    }

    uint32_t pieces() const {
        return file_length_ / piece_length_;
    }

    uint32_t piece_size() const {
        return piece_length_;
    }

    const cmn::Hash& piece_hash(uint32_t index) const {
        return piece_hashes_[index];
    }

private:
    string announce_;
    string filename_;
    uint32_t piece_length_;
    uint32_t file_length_;
    vector<cmn::Hash> piece_hashes_;
    cmn::Hash info_hash_;
};

class TrackerResponse {
public:
    explicit TrackerResponse(const string_view& data);

    const vector<cmn::Address>& peers() const {
        return peers_;
    }

private:
    uint32_t interval_;
    vector<cmn::Address> peers_;
};

struct TorrentContext {
    ba::io_context& io;
    const vector<char>& handshake;
    const SingleFileTorrent& tor;
    shared_ptr<boost::lockfree::queue<uint32_t>> work_queue;
    shared_ptr<boost::lockfree::queue<cmn::CompletePiece>> result_queue;
};

class Peer: public enable_shared_from_this<Peer> {
public:
    Peer(const TorrentContext& ctx, cmn::Address addr);

private:
    void async_handshake_write(const boost::system::error_code& ec);
    void async_handshake_read(const boost::system::error_code& ec);

    void async_next(const boost::system::error_code& ec);

    void async_read_message();

    void async_read_len(const boost::system::error_code& ec);
    void handle_message();

    void begin_continue_piece();
    void request_block();

    void write_message(const Message& msg) { write_message(msg, [](auto& ec, auto _) {}); }


    void release_block() {
        --active_blocks_;
        if (block_) block_->release();
        block_ = nullptr;
    }

    template <class T>
    void write_message(const Message& msg, const T& handler) {
        ba::async_write(socket_, ba::buffer(msg.serialize()), handler);
    }

    ostream& log() const {
        return cout << "[" << (peer_id_.empty() ? addr_.to_string() : cmn::urlencode(peer_id_)) << "] ";
    }

    const vector<char>& handshake_;
    const SingleFileTorrent& tor_;
    vector<char> recv_buffer_;
    tcp::socket socket_;
    cmn::Address addr_;
    string peer_id_;
    bool choked_ = true;
    cmn::Bitfield available_pieces_;

    shared_ptr<boost::lockfree::queue<uint32_t>> work_queue_;
    shared_ptr<boost::lockfree::queue<cmn::CompletePiece>> result_queue_;
    optional<cmn::Piece> piece_;
    shared_ptr<cmn::Block> block_;
    uint32_t active_blocks_ = 0;
};

void write_thread(const TorrentContext& ctx);

#endif //PICOTOR_TORRENT_HPP
