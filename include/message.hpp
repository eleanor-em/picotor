//
// Created by eleanor on 28.02.23.
//
#ifndef PICOTOR_MESSAGE_HPP
#define PICOTOR_MESSAGE_HPP

#include <string>

#include <boost/asio.hpp>

#include <common.hpp>
#include <torrent.hpp>

using namespace std;
namespace ba = boost::asio;
using ba::ip::tcp;

struct Handshake {
    string pstr{"BitTorrent protocol"};
    char extensions[8];
    cmn::Hash info_hash;
    string peer_id;

    Handshake(cmn::Hash hash, const char* id): info_hash(hash), peer_id(id) {}

    explicit Handshake(const vector<char>& data);

    vector<char> serialise() const;
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
        uint32_t piece_index = htonl(piece.index());
        uint32_t block_offset = htonl(block.offset());
        uint32_t block_length = htonl(std::min(piece.size() - block.offset(), BLOCK_SIZE));
        stringstream ss;
        ss.write(reinterpret_cast<const char*>(&piece_index), 4);
        ss.write(reinterpret_cast<const char*>(&block_offset), 4);
        ss.write(reinterpret_cast<const char*>(&block_length), 4);
        auto str = ss.str();
        return Message{Message::Type::Request, vector<char>{str.begin(), str.end()}};
    }

    [[nodiscard]] string to_string() const;
    [[nodiscard]] vector<char> serialize() const;

    optional<Type> type;
    vector<char> payload;

private:
    Message(Message::Type type_, vector<char> payload_): type(type_), payload(payload_) {}
};

#endif //PICOTOR_MESSAGE_HPP
