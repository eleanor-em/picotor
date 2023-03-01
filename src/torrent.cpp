#include "torrent.hpp"
#include <iostream>
#include <bencode.hpp>
#include <message.hpp>

using namespace std;

SingleFileTorrent::SingleFileTorrent(const string_view &data) {
    const auto dict = get<bencode::dict_view>(bencode::decode_view(data));
    // torrents are *supposed* to have announce strings. in practice, they might not.
    announce_ = get<bencode::string_view>(dict.at("announce"));

    // extract info
    const auto info = get<bencode::dict_view>(dict.at("info"));
    piece_length_ = get<bencode::integer_view>(info.at("piece length"));
    file_length_ = get<bencode::integer_view>(info.at("length"));
    filename_ = get<bencode::string_view>(info.at("name"));

    // put piece hashes into a vector
    piece_hashes_.reserve(file_length_ / piece_length_ + 1);
    const auto piece_hashes = get<bencode::string_view>(info.at("pieces"));
    for (auto it = piece_hashes.cbegin(); it < piece_hashes.cend(); it += cmn::HASH_SIZE) {
        piece_hashes_.emplace_back(it);
    }

    // re-encode info dict, so we can compute its hash. as far as I can tell this bencode lib
    // doesn't have a more efficient way to do it
    const auto info_str = bencode::encode(info);
    info_hash_ = cmn::Hash::of(info_str);
}

TrackerResponse::TrackerResponse(const string_view &data) {
    const auto dict = get<bencode::dict_view>(bencode::decode_view(data));
    interval_ = get<bencode::integer_view>(dict.at("interval"));

    const auto peers = get<bencode::string_view>(dict.at("peers"));
    peers_.reserve(peers.length() / 6);
    for (size_t i = 0; i < peers.length(); i += 6) {
        peers_.emplace_back(cmn::Address{peers.substr(i, i + 6).data()});
    }
}

Peer::Peer(cmn::Address addr, ba::io_context &io, const string &handshake)
        : addr_(addr), socket_(io), handshake_(handshake)
{
    tcp::resolver resolver(io);
    auto endpoints = resolver.resolve(addr_.ip(), addr_.port_str());
    ba::async_connect(socket_, endpoints,
                      [this](auto ec, auto _) { shared_from_this()->async_handshake_write(ec); });
}

void Peer::async_handshake_write(const boost::system::error_code &ec) {
    if (ec.failed()) {
        log() << "error connecting: " << ec.message() << endl;
    } else {
        ba::async_write(socket_, ba::buffer(handshake_),
                        [this](auto ec, auto _) { shared_from_this()->async_handshake_read(ec); });
    }
}

void Peer::async_handshake_read(const boost::system::error_code &ec) {
    if (ec.failed()) {
        log() << "error writing handshake: " << ec.message() << endl;
    } else {
        // handshake we receive back should be the same length as ours
        recv_buffer_.resize(handshake_.length());
        ba::async_read(socket_, ba::buffer(recv_buffer_), [this](auto ec, auto _) { shared_from_this()->async_next(ec); });
    }
}

void Peer::async_next(const boost::system::error_code &ec) {
    if (ec.failed()) {
        log() << "error reading handshake: " << ec.message() << endl;
    } else {
        Handshake result{recv_buffer_};
        peer_id_ = move(result.peer_id);
        log() << "successfully connected (" << addr_.to_string() << ")" << endl;
        async_read_message();
    }
}

void Peer::async_read_message() {
    // first handle_message the message length
    recv_buffer_.resize(sizeof(uint32_t));
    ba::async_read(socket_, ba::buffer(recv_buffer_),
                    [this](auto ec, auto _) { shared_from_this()->async_read_len(ec); });
}

void Peer::async_read_len(const boost::system::error_code &ec) {
    if (ec.failed()) {
    } else {
        uint32_t len = ntohl(*reinterpret_cast<const uint32_t*>(recv_buffer_.c_str()));
        if (len == 0) {
            // zero length means keepalive message
            log() << "keepalive\n";
            async_read_message();
        } else {
            recv_buffer_.resize(len);
            ba::async_read(socket_, ba::buffer(recv_buffer_),
                           [this](auto ec, auto _) { shared_from_this()->handle_message(ec); });
        }
    }
}

void Peer::handle_message(const boost::system::error_code& ec) {
    if (ec.failed()) {
        log() << "error reading message from " << ": " << ec.message() << endl;
    } else {
        Message msg{recv_buffer_};
        log() << "received: " << msg.to_string() << endl;
        if (!msg.type.has_value()) {
            return;
        }
        switch (msg.type.value()) {
            case Message::Choke:
                choked_ = true;
                break;
            case Message::Unchoke:
                choked_ = false;
                break;
        }
        async_read_message();
    }
}
