//
// Created by eleanor on 28.02.23.
//

#ifndef PICOTOR_MESSAGE_HPP
#define PICOTOR_MESSAGE_HPP
#include <common.hpp>
#include <string>
#include <optional>

using namespace std;

struct Handshake {
    string pstr{"BitTorrent protocol"};
    char extensions[8];
    cmn::Hash info_hash;
    string peer_id;

    Handshake(cmn::Hash hash, const char* id): info_hash(hash), peer_id(id) {}

    explicit Handshake(const string& data);

    string serialise() const;
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

    [[nodiscard]] static string type_to_string(Type type);
    [[nodiscard]] static optional<Type> try_from(uint8_t src);

    optional<Type> type;
    string payload;

    explicit Message(const string& data);

    [[nodiscard]] string to_string() const;
};

#endif //PICOTOR_MESSAGE_HPP
