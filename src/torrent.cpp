#include <iostream>
#include <unordered_set>

#include <bencode.hpp>

#include <torrent.hpp>

using namespace std;

SingleFileTorrent::SingleFileTorrent(const string_view &data) {
    const auto dict = get<bencode::dict_view>(bencode::decode_view(data));
    // torrents are *supposed* to have announce strings. in practice, they might not.
    announce_ = get<bencode::string_view>(dict.at("announce"));

    // ensure it's HTTP
    auto start_pos = announce_.find("udp");
    if (start_pos != std::string::npos) {
        announce_.replace(start_pos, 3, "http");
    }

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

TrackerResponse::TrackerResponse(const string_view& data) {
    const auto dict = get<bencode::dict_view>(bencode::decode_view(data));
    interval_ = get<bencode::integer_view>(dict.at("interval"));

    const auto peers = get<bencode::string_view>(dict.at("peers"));
    peers_.reserve(peers.length() / 6);
    for (size_t i = 0; i < peers.length(); i += 6) {
        peers_.emplace_back(cmn::Address{peers.substr(i, i + 6).data()});
    }
}
