//
// Created by eleanor on 20.02.23.
//

#ifndef PICOTOR_TORRENT_HPP
#define PICOTOR_TORRENT_HPP
#include <common.hpp>
#include <string>
#include <vector>
#include <boost/asio.hpp>

namespace ba = boost::asio;
using ba::ip::tcp;
using namespace std;

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

class Peer: public enable_shared_from_this<Peer> {
public:
    Peer(cmn::Address addr, ba::io_context& io, const string& handshake);

private:
    void async_handshake_write(const boost::system::error_code& ec);
    void async_handshake_read(const boost::system::error_code& ec);

    void async_next(const boost::system::error_code& ec);

    void async_read_message();

    void async_read_len(const boost::system::error_code& ec);
    void handle_message(const boost::system::error_code& ec);

    ostream& log() const {
        cout << "[" << (peer_id_.empty() ? addr_.to_string() : cmn::urlencode(peer_id_)) << "] ";
        return cout;
    }

    const string& handshake_;
    string recv_buffer_;
    tcp::socket socket_;
    cmn::Address addr_;
    string peer_id_;
    bool choked_ = true;
};

#endif //PICOTOR_TORRENT_HPP
