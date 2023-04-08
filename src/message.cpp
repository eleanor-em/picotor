//
// Created by eleanor on 28.02.23.
//
#include <iostream>

#include <message.hpp>

using std::cout;
using std::endl;
using std::nullopt;

using cmn::Hash;
using cmn::HASH_SIZE;

Handshake::Handshake(const vector<char>& data): extensions{0} {
    stringstream ss{string{data.begin(), data.end()}};

    // handle_message protocol description
    uint8_t len;
    ss >> len;
    char desc[len + 1];
    desc[len] = 0;
    ss.read(desc, len);
    if (desc != protocol_str) {
        cout << "NOTE: unexpected protocol string: " << desc << endl;
    }

    // handle_message extensions
    ss.read(extensions, sizeof(extensions));

    // handle_message info hash
    char hash[HASH_SIZE];
    ss.read(hash, sizeof(hash));
    info_hash = Hash{hash};

    // handle_message peer id
    auto c = ss.get();
    while (!ss.eof()) {
        peer_id += static_cast<char>(c);
        c = ss.get();
    }
}

vector<char> Handshake::serialise() const {
    stringstream ss;
    char len = static_cast<char>(protocol_str.length());
    ss.write(&len, 1);
    ss << protocol_str;

    ss.write(extensions, sizeof(extensions));

    auto hash = info_hash.as_bytes();
    ss.write(hash.data(), static_cast<std::streamsize>(hash.size()));

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
        string result = type_to_string(*type) + "{";

        stringstream ss;
        switch (*type) {
            case Piece:
            case Request:
                uint32_t index;
                uint32_t offset;
                ss.write(payload.data(), static_cast<std::streamsize>(payload.size()));
                ss.read(reinterpret_cast<char*>(&index), 4);
                index = ntohl(index);
                ss.read(reinterpret_cast<char*>(&offset), 4);
                offset = ntohl(offset);
                result += std::to_string(index) + ","
                        + std::to_string(offset) + "-"
                        + std::to_string(offset + BLOCK_SIZE - 1);
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
    uint8_t ty = static_cast<char>(*type);
    uint32_t len = htonl(payload.size() + 1);

    stringstream ss;
    ss.write(reinterpret_cast<const char*>(&len), 4);
    ss.write(reinterpret_cast<const char*>(&ty), 1);
    ss.write(payload.data(), static_cast<std::streamsize>(payload.size()));
    auto str = ss.str();
    return vector<char>{str.begin(), str.end()};
}

optional<Message::Type> Message::try_from(uint8_t src) {
    if (src <= Message::Type::Cancel || src == Message::Type::Extension) {
        return static_cast<Message::Type>(src);
    } else {
        cout << "\t" << cmn::urlencode(std::to_string((uint16_t) src)) << endl;
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
