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

using namespace std;

class SingleFileTorrent {
public:
    explicit SingleFileTorrent(const string_view &data);
    static SingleFileTorrent from_file(const std::string_view& path) {
        return SingleFileTorrent(cmn::read_file(path));
    }

    [[nodiscard]] const string &announce() const {
        return _announce;
    }

    [[nodiscard]] const string &filename() const {
        return _filename;
    }

    [[nodiscard]] string info_hash() const {
        return _info_hash.as_str();
    }

private:
    string _announce;
    string _filename;
    size_t _piece_length;
    size_t _file_length;
    vector <cmn::Hash> _piece_hashes;
    cmn::Hash _info_hash;
};
#endif //PICOTOR_TORRENT_HPP
