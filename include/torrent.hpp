//
// Created by eleanor on 20.02.23.
//

#ifndef PICOTOR_TORRENT_HPP
#define PICOTOR_TORRENT_HPP
#include <common.hpp>
#include <bencode.hpp>
#include <iostream>
#include <filesystem>
#include <string>
#include <vector>

#include <boost/asio.hpp>
#include <boost/thread/thread.hpp>
#include <boost/bind/bind.hpp>

namespace ba = boost::asio;
using namespace std;
using ba::ip::tcp;

class SingleFileTorrent {
public:
    explicit SingleFileTorrent(const string_view &data);
    static SingleFileTorrent from_file(const string_view& path) {
        return SingleFileTorrent(cmn::read_file(path));
    }

    [[nodiscard]] const string &announce() const {
        return announce_;
    }

    [[nodiscard]] const string &filename() const {
        return filename_;
    }

    [[nodiscard]] const cmn::Hash& info_hash() const {
        return info_hash_;
    }

    [[nodiscard]] size_t file_length() const {
        return file_length_;
    }

private:
    string announce_;
    string filename_;
    size_t piece_length_;
    size_t file_length_;
    vector <cmn::Hash> piece_hashes_;
    cmn::Hash info_hash_;
};

class TrackerResponse {
public:
    explicit TrackerResponse(const string_view& data);

    const std::vector<cmn::Address>& peers() const {
        return peers_;
    }
private:
    uint32_t interval_;
    std::vector<cmn::Address> peers_;
};

struct Handshake {
    std::string pstr{"BitTorrent protocol"};
    char extensions[8];
    cmn::Hash info_hash;
    std::string peer_id;

    Handshake(cmn::Hash hash, const char* id): info_hash(hash), peer_id(id) {}

    explicit Handshake(const std::string& data) {
        std::stringstream ss{data};
        uint8_t len;
        ss >> len;
        char desc[len + 1];
        desc[len] = 0;
        ss.read(desc, len);

        ss.read(extensions, sizeof(extensions));

        char hash[cmn::HASH_SIZE];
        ss.read(hash, sizeof(hash));

        if (desc != pstr) {
            std::cout << "NOTE: unexpected protocol string: " << desc << std::endl;
        }
        info_hash = cmn::Hash{hash};

        char c = ss.get();
        while (!ss.eof()) {
            peer_id += c;
            c = ss.get();
        }
    }

    std::string serialise() const {
        std::stringstream ss;
        char len = pstr.length();
        ss.write(&len, 1);
        ss << pstr;

        for (size_t i = 0; i < sizeof(extensions); ++i) {
            ss.write(extensions + i, 1);
        }

        auto hash = info_hash.as_byte_string();
        ss.write(hash.c_str(), hash.length());
        ss << peer_id;
        return ss.str();
    }
};

class HandshakePeer: public std::enable_shared_from_this<HandshakePeer> {
public:
    HandshakePeer(cmn::Address addr, ba::io_context& io, const std::string& handshake)
        : addr_(addr), socket_(io), handshake_(handshake)
    {
        tcp::resolver resolver(io);
        auto endpoints = resolver.resolve(addr_.ip(), addr_.port_str());
        ba::async_connect(socket_, endpoints,
                           [this](auto ec, auto _) { shared_from_this()->handshake_write(ec); });
    }

private:
    void handshake_write(const boost::system::error_code& ec) const {
        if (ec.failed()) {
            std::cout << "error connecting to " << addr_.as_str() << ": " << ec.message() << std::endl;
        } else {
            ba::async_write(socket_, ba::buffer(handshake_),
                                     [this](auto ec, auto _) { shared_from_this()->handshake_read(ec); });
        }
    }

    void handshake_read(const boost::system::error_code& ec) const {
        if (ec.failed()) {
            std::cout << "error writing handshake to " << addr_.as_str() << ": " << ec.message() << std::endl;
        } else {
            // handshake we receive back should be the same length as ours
            recv_buffer_.resize(handshake_.length());
            ba::async_read(socket_, ba::buffer(recv_buffer_), [this](auto ec, auto _) { shared_from_this()->next(ec); });
        }
    }

    void next(const boost::system::error_code& ec) const {
        if (ec.failed()) {
            std::cout << "error reading handshake from " << addr_.as_str() << ": " << ec.message() << std::endl;
        } else {
            Handshake result{recv_buffer_};
            std::cout << "successfully connected to " << cmn::urlencode(result.peer_id) << " (" << addr_.as_str() << ")" << std::endl;
        }
    }

    const std::string& handshake_;
    mutable std::string recv_buffer_;
    mutable tcp::socket socket_;
    cmn::Address addr_;
};
#endif //PICOTOR_TORRENT_HPP
