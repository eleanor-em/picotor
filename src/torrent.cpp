#include "torrent.hpp"

SingleFileTorrent::SingleFileTorrent(const string_view &data) {
    const auto dict = get<bencode::dict_view>(bencode::decode_view(data));
    // torrents are *supposed* to have announce strings. in practice, they might not.
    _announce = get<bencode::string_view>(dict.at("announce"));

    // extract info
    const auto info = get<bencode::dict_view>(dict.at("info"));
    _piece_length = get<bencode::integer_view>(info.at("piece length"));
    _file_length = get<bencode::integer_view>(info.at("length"));
    _filename = get<bencode::string_view>(info.at("name"));

    // put piece hashes into a vector
    _piece_hashes.reserve(_file_length / _piece_length + 1);
    const auto piece_hashes = get<bencode::string_view>(info.at("pieces"));
    for (auto it = piece_hashes.cbegin(); it < piece_hashes.cend(); it += cmn::HASH_SIZE) {
        _piece_hashes.emplace_back(it);
    }

    // re-encode info dict, so we can compute its hash
    const auto info_str = bencode::encode(info);
    _info_hash = cmn::Hash::from(info_str);
}
