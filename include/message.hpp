//
// Created by eleanor on 28.02.23.
//
#ifndef PICOTOR_MESSAGE_HPP
#define PICOTOR_MESSAGE_HPP

#include <string>

#include <boost/asio.hpp>

#include <common.hpp>
#include <torrent.hpp>
#include <utility>

namespace ba = boost::asio;
using ba::ip::tcp;
using std::optional;
using std::string;
using std::vector;
using cmn::Hash;

struct Handshake {
    string protocol_str{"BitTorrent protocol"};
    char extensions[8];
    Hash info_hash;
    string peer_id;

    Handshake(cmn::Hash hash, const char* id): info_hash(std::move(hash)), peer_id(id), extensions{0} {}

    explicit Handshake(const vector<char>& data);

    [[nodiscard]] vector<char> serialise() const;
};

struct Message {
    enum Type {
        Choke,
        Unchoke,
        Interested,
        NotInterested,
        Have,
        Bitfield,
        Request,
        Piece,
        Cancel,
        Extension = 20,
    };

    explicit Message(const vector<char>& data);

    [[nodiscard]] static string type_to_string(Type type);
    [[nodiscard]] static optional<Type> try_from(uint8_t src);

    [[nodiscard]] static Message interested() {
        return Message{Message::Type::Interested, vector<char>{}};
    }
    [[nodiscard]] static Message request(const class::Piece& piece, const Block& block) {
        vector<char> data;
        data.reserve(12);
        cmn::push_bytes(&data, htonl(piece.index()));
        cmn::push_bytes(&data, htonl(block.offset()));
        cmn::push_bytes(&data, htonl(std::min(piece.size() - block.offset(), BLOCK_SIZE)));
        return Message{Message::Type::Request, data};
    }

    [[nodiscard]] string to_string() const;
    [[nodiscard]] vector<char> serialize() const;

    optional<Type> type;
    vector<char> payload;

private:
    Message(Message::Type type_, vector<char> payload_): type(type_), payload(std::move(payload_)) {}
};

#endif //PICOTOR_MESSAGE_HPP
