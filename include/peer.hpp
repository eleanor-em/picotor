//
// Created by Eleanor McMurtry on 05.04.23.
//
#ifndef PICOTOR_PEER_HPP
#define PICOTOR_PEER_HPP

#include <unordered_set>

#include <boost/asio.hpp>
#include <boost/lockfree/queue.hpp>

#include <message.hpp>
#include <torrent.hpp>

class Peer {
public:
    Peer(const TorrentContext& ctx, cmn::Address addr);

private:
    // handshake_write -> handshake_read -> next
    // -> read_message -> read_len -> handle_message
    // -> read_message -> ...
    void async_handshake_write(const boost::system::error_code& ec);
    void async_handshake_read(const boost::system::error_code& ec);
    void async_next(const boost::system::error_code& ec);
    void async_read_message();
    void async_read_len(const boost::system::error_code& ec);
    void async_handle_message();

    void async_download();
    void async_write_message(const Message& msg) {
        async_write_message(msg, [](auto& ec, auto _) {});
    }
    template <class T>
    void async_write_message(const Message& msg, const T& handler) {
        ba::async_write(socket_, ba::buffer(msg.serialize()), handler);
    }

    void handle_piece(const Message& msg);

    void release_block(uint32_t index) {
        auto block = blocks_.find(index);
        if (block != blocks_.end()) {
            block->second->release();
            blocks_.erase(block);
        }
    }

    void release_piece() {
        if (piece_) {
            while (!ctx_.work_queue->push(piece_->index()));
            piece_.reset();
        }
    }

    void close() {
        closed_ = true;
        release_piece();
        socket_.close();
        while (!ctx_.result_queue->push(ResultPeerDropped{addr_}));
    }

    ostream& log() const {
        return cout << "[" << (peer_id_.empty() ? addr_.to_string() : cmn::urlencode(peer_id_)) << "] ";
    }

    // parameters
    const uint32_t PIPELINE_LIMIT = 5;
    const uint32_t TIMEOUT_MS = 2000;
    const cmn::Address addr_;
    const TorrentContext& ctx_;

    // networking data
    vector<char> recv_buffer_;
    tcp::socket socket_;
    string peer_id_;

    // protocol data
    bool choked_ = true;
    cmn::Bitfield available_pieces_;
    optional<Piece> piece_;
    unordered_map<uint32_t, shared_ptr<Block>> blocks_;

    // state
    bool closed_ = false;
};

void monitor_thread(const TorrentContext& ctx, const unique_ptr<vector<Peer>>&& peers);

#endif //PICOTOR_PEER_HPP
