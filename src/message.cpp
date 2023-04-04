//
// Created by eleanor on 28.02.23.
//
#include <message.hpp>
#include <iostream>

using namespace std;

Handshake::Handshake(const string&& data) {
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

vector<char> Handshake::serialise() const {
    stringstream ss;
    char len = pstr.length();
    ss.write(&len, 1);
    ss << pstr;

    ss.write(extensions, sizeof(extensions));

    auto hash = info_hash.as_bytes();
    ss.write(hash.data(), hash.size());

    ss << peer_id;

    auto str = ss.str();

    return vector<char>{str.begin(), str.end()};
}

Message::Message(const vector<char>& data) {
    type = try_from(data.at(0));
    payload = vector<char>{data.begin() + 1, data.end()};
}

string Message::to_string() const {
    if (type) {
        string result = type_to_string(type.value()) + "{";

        std::stringstream ss;
        switch (type.value()) {
            case Piece:
            case Request:
                uint32_t index;
                uint32_t offset;
                ss.write(payload.data(), payload.size());
                ss.read(reinterpret_cast<char*>(&index), 4);
                index = ntohl(index);
                ss.read(reinterpret_cast<char*>(&offset), 4);
                offset = ntohl(offset);
                result += ::to_string(index) + "," + ::to_string(offset) + "-" + ::to_string(offset + cmn::BLOCK_SIZE - 1);
                break;

            default:
                result += cmn::urlencode(payload);
                break;
        }
        return result + "}";
    } else {
        return "UNKNOWN{" + cmn::urlencode(payload) + "}";
    }
}

vector<char> Message::serialize() const {
    assert(type.has_value());
    uint8_t ty = static_cast<char>(type.value());
    uint32_t len = htonl(payload.size() + 1);

    stringstream ss;
    ss.write(reinterpret_cast<const char*>(&len), 4);
    ss.write(reinterpret_cast<const char*>(&ty), 1);
    ss.write(payload.data(), payload.size());
    auto str = ss.str();
    return vector<char>{str.begin(), str.end()};
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
