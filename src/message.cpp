//
// Created by eleanor on 28.02.23.
//
#include <message.hpp>
#include <iostream>

using namespace std;

Handshake::Handshake(const string &data) {
    stringstream ss{data};

    // handle_message protocol description
    uint8_t len;
    ss >> len;
    char desc[len + 1];
    desc[len] = 0;
    ss.read(desc, len);
    if (desc != pstr) {
        cout << "NOTE: unexpected protocol string: " << desc << endl;
    }

    // handle_message extensions
    ss.read(extensions, sizeof(extensions));

    // handle_message infohash
    char hash[cmn::HASH_SIZE];
    ss.read(hash, sizeof(hash));
    info_hash = cmn::Hash{hash};

    // handle_message peer id
    char c = ss.get();
    while (!ss.eof()) {
        peer_id += c;
        c = ss.get();
    }
}

string Handshake::serialise() const {
    stringstream ss;
    char len = pstr.length();
    ss.write(&len, 1);
    ss << pstr;

    ss.write(extensions, sizeof(extensions));

    auto hash = info_hash.as_byte_string();
    ss.write(hash.c_str(), hash.length());

    ss << peer_id;

    return ss.str();
}

Message::Message(const string& data) {
    type = try_from(data.at(0));
    payload = data.substr(1);
}

string Message::to_string() const {
    if (type.has_value()) {
        return type_to_string(type.value()) + "{" + cmn::urlencode(payload) + "}";
    } else {
        return "UNKNOWN{" + cmn::urlencode(payload) + "}";
    }
}

optional<Message::Type> Message::try_from(uint8_t src) {
    if (src <= Message::Type::Cancel || src == Message::Type::Extension) {
        return static_cast<Message::Type>(src);
    } else {
        cout << "\t" << cmn::urlencode(::to_string((uint16_t) src)) << endl;
        return nullopt;
    }
}

string Message::type_to_string(Message::Type type) {
    switch (type) {
        case Choke:
            return "Choke";
        case Unchoke:
            return "Unchoke";
        case Interested:
            return "Interested";
        case NotInterested:
            return "NotInterested";
        case Have:
            return "Have";
        case Bitfield:
            return "BitField";
        case Request:
            return "Request";
        case Piece:
            return "Piece";
        case Cancel:
            return "Cancel";
        case Extension:
            return "Extension";
    }
}
