//
// Created by eleanor on 20.02.23.
//

#ifndef PICOTOR_TORRENT_HPP
#define PICOTOR_TORRENT_HPP
#include <optional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <boost/asio.hpp>

#include <boost/lockfree/queue.hpp>
#include <common.hpp>

namespace ba = boost::asio;
using ba::ip::tcp;

using std::optional;
using std::shared_ptr;
using std::string;
using std::stringstream;
using std::string_view;
using std::vector;

using cmn::Address;
using cmn::Hash;

class SingleFileTorrent {
public:
    explicit SingleFileTorrent(const string_view &data);
    static SingleFileTorrent from_file(const string_view& path) {
        return SingleFileTorrent(cmn::read_file(path));
    }

    [[nodiscard]] const string& announce() const { return announce_; }
    [[nodiscard]] const string& filename() const { return filename_; }
    [[nodiscard]] const Hash& info_hash() const { return info_hash_; }
    [[nodiscard]] uint32_t file_length() const { return file_length_; }
    [[nodiscard]] uint32_t pieces() const { return file_length_ / piece_length_; }
    [[nodiscard]] uint32_t piece_size() const { return piece_length_; }
    [[nodiscard]] const Hash& piece_hash(uint32_t index) const { return piece_hashes_[index]; }

private:
    string announce_;
    string filename_;
    uint32_t piece_length_;
    uint32_t file_length_;
    vector<Hash> piece_hashes_;
    Hash info_hash_;
};

class TrackerResponse {
public:
    explicit TrackerResponse(const string_view& data);

    [[nodiscard]] const vector<Address>& peers() const { return peers_; }

private:
    [[maybe_unused]] uint32_t interval_;
    vector<Address> peers_;
};

const uint32_t BLOCK_SIZE = 1<<14;

enum BlockStatus {
    Ok,
    AlreadyFilled,
    WrongPiece,
    WrongOffset,
    TooMuchData,
};

inline const char* block_status_string(BlockStatus status) {
    switch (status) {
        case Ok:
            return "Ok";
        case AlreadyFilled:
            return "AlreadyFilled";
        case WrongPiece:
            return "WrongPiece";
        case WrongOffset:
            return "WrongOffset";
        case TooMuchData:
            return "TooMuchData";
    }
}

class Block {
public:
    explicit Block(uint32_t offset): offset_(offset), data_{0}, len_(0) {}
    [[nodiscard]] bool filled() const { return filled_; }
    [[nodiscard]] uint32_t offset() const { return offset_; }
    [[nodiscard]] uint32_t index() const { return offset_ / BLOCK_SIZE; }
    void release() { reserved_ = false; }

private:
    bool filled_ = false;
    bool reserved_ = false;
    char data_[BLOCK_SIZE];
    size_t len_;
    uint32_t offset_;

    BlockStatus fill(stringstream& ss, std::streamsize len) {
        if (filled_) return BlockStatus::AlreadyFilled;
        ss.read(data_, len);
        len_ = len;
        filled_ = true;
        return BlockStatus::Ok;
    }

    friend class Piece;
};

class CompletePiece {
public:
    CompletePiece(): data_(nullptr), piece_index_(0), size_(0) {}
    CompletePiece(char* data, uint32_t piece_index, uint32_t size)
            : data_(data), piece_index_(piece_index), size_(size) {}

    [[nodiscard]] cmn::Hash hash() const { return cmn::Hash::of(std::string{data_, size_}); }
    void free() { delete[] data_; }

    [[nodiscard]] char* data() const { return data_; }
    [[nodiscard]] uint32_t index() const { return piece_index_; }
    [[nodiscard]] uint32_t offset() const { return piece_index_ * size_; }
    [[nodiscard]] uint32_t size() const { return size_; }
private:
    char* data_; // TODO: unique_ptr?
    uint32_t piece_index_;
    uint32_t size_;
};

class Piece {
public:
    Piece(uint32_t index, uint32_t piece_size): index_(index), piece_size_(piece_size) {}

    [[nodiscard]] uint32_t index() const { return index_; }
    [[nodiscard]] uint32_t size() const { return piece_size_; }

    [[nodiscard]] std::pair<BlockStatus, uint32_t> accept(const vector<char>& payload) {
        // 8 bytes used for initial data
        const uint32_t data_len = payload.size() - 8;
        stringstream ss;
        ss.write(payload.data(), static_cast<std::streamsize>(payload.size()));

        uint32_t piece_index;
        uint32_t offset;
        ss.read(reinterpret_cast<char*>(&piece_index), 4);
        piece_index = ntohl(piece_index);
        ss.read(reinterpret_cast<char*>(&offset), 4);
        const auto block_index = ntohl(offset) / BLOCK_SIZE;

        if (data_len > BLOCK_SIZE) return std::pair{BlockStatus::TooMuchData, block_index};
        if (piece_index != index_) return std::pair{BlockStatus::WrongPiece, block_index};
        if (block_index >= blocks_.size()) return std::pair{BlockStatus::WrongOffset, block_index};
        return std::pair{blocks_[block_index]->fill(ss, data_len), block_index};
    }

    shared_ptr<Block> next_block() {
        if (blocks_.size() < block_count()) {
            // if there's a block we haven't started yet, give them that
            const auto offset = static_cast<uint32_t>(blocks_.size() * BLOCK_SIZE);
            blocks_.push_back(std::make_shared<Block>(offset));
            const auto block = *(blocks_.end() - 1);
            block->reserved_ = true;
            return block;
        } else {
            // otherwise, look for one that's not reserved
            for (const auto block : blocks_) {
                if (!block->filled_ && !block->reserved_) {
                    block->reserved_ = true;
                    return block;
                }
            }
        }
        return nullptr;
    }

    [[nodiscard]] bool is_complete() const {
        const auto all_filled = std::all_of(blocks_.cbegin(), blocks_.cend(),
                                      [](auto block) { return block->filled(); });
        return blocks_.size() == block_count() && all_filled;
    }

    CompletePiece finalize() {
        if (!finalized_) {
            char *data = new char[piece_size_];
            char *ptr = data;

            for (const auto block: blocks_) {
                std::copy(block->data_, block->data_ + block->len_, ptr);
                ptr += block->len_;
            }

            finalized_ = CompletePiece{data, index_, piece_size_};
        }
        return *finalized_;
    }

private:
    uint32_t index_;
    uint32_t piece_size_;
    vector<shared_ptr<Block>> blocks_;

    // cached result
    optional<CompletePiece> finalized_;

    [[nodiscard]] uint32_t block_count() const {
        if (piece_size_ % BLOCK_SIZE == 0) {
            return piece_size_ / BLOCK_SIZE;
        } else {
            return piece_size_ / BLOCK_SIZE + 1;
        }
    }
};

#include <result.hpp>

struct TorrentContext {
    ba::io_context& io;
    const vector<char>& handshake;
    const SingleFileTorrent& tor;
    shared_ptr<boost::lockfree::queue<uint32_t>> work_queue;
    shared_ptr<boost::lockfree::queue<Result>> result_queue;
    size_t total_peers;
};

#endif //PICOTOR_TORRENT_HPP