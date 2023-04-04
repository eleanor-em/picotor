#ifndef PICOTOR_COMMON_HPP
#define PICOTOR_COMMON_HPP
#include <string>
#include <fstream>
#include <cstring>
#include <vector>
#include <map>
#include <iomanip>
#include <sha1.hpp>
#include <netinet/in.h>

namespace cmn {
    auto read_file(std::string_view path) -> std::string;
    std::string urlencode(const std::vector<char>& s);
    std::string urlencode(const std::string& s);
    std::vector<char> hex_to_bytes(const std::string& hex);

    const size_t HASH_SIZE = 20; // bytes

    class Hash {
    public:
        static Hash of(const std::string& str);

        // constructors so we can use emplace
        explicit Hash(const std::vector<char>&& vec): bytes_(vec) {}
        explicit Hash(const std::vector<char>& vec): bytes_(vec) {}
        explicit Hash(const char *bytes) {
            bytes_.resize(HASH_SIZE);
            std::copy(bytes, bytes + HASH_SIZE, bytes_.begin());
        }
        explicit Hash(const std::string& str) {
            assert(str.length() == HASH_SIZE);
            bytes_.resize(HASH_SIZE);
            std::copy(str.begin(), str.end(), bytes_.begin());
        }

        Hash() = default;

        [[nodiscard]] std::string as_hex() const;

        [[nodiscard]] const std::vector<char>& as_bytes() const {
            return bytes_;
        }

        bool operator==(const Hash& rhs) const {
            return bytes_ == rhs.bytes_;
        }
    private:
        std::vector<char> bytes_;
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
        Block(uint32_t offset): offset_(offset) {}
        bool filled() const { return filled_; }
        uint32_t offset() const { return offset_; }
        uint32_t index() const { return offset_ / BLOCK_SIZE; }
        void release() { reserved_ = false; }

    private:
        bool filled_ = false;
        bool reserved_ = false;
        char data_[BLOCK_SIZE];
        size_t len_;
        uint32_t piece_;
        uint32_t offset_;

        BlockStatus fill(std::stringstream& ss, size_t len) {
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

        [[nodiscard]] cmn::Hash hash() const { return Hash::of(std::string{data_, size_}); }
        void free() { delete[] data_; }

        char* data() const { return data_; }
        uint32_t index() const { return piece_index_; }
        uint32_t offset() const { return piece_index_ * size_; }
        uint32_t size() const { return size_; }
    private:
        char* data_;
        uint32_t piece_index_;
        uint32_t size_;
    };

    class Piece {
    public:
        Piece(uint32_t index, uint32_t piece_size): index_(index), piece_size_(piece_size) {}

        uint32_t index() const { return index_; }
        uint32_t size() const { return piece_size_; }

        BlockStatus accept(const std::vector<char>& payload) {
            // 8 bytes used for initial data
            uint32_t data_len = payload.size() - 8;
            if (data_len > BLOCK_SIZE) return BlockStatus::TooMuchData;

            uint32_t piece_index;
            uint32_t offset;
            std::stringstream ss;
            ss.write(payload.data(), payload.size());

            ss.read(reinterpret_cast<char*>(&piece_index), 4);
            piece_index = ntohl(piece_index);
            if (piece_index != index_) return BlockStatus::WrongPiece;

            ss.read(reinterpret_cast<char*>(&offset), 4);
            auto block_index = ntohl(offset) / BLOCK_SIZE;
            if (block_index >= blocks_.size()) return BlockStatus::WrongOffset;

            return blocks_[block_index]->fill(ss, data_len);
        }

        std::shared_ptr<Block> next_block() {
            if (blocks_.size() < block_count()) {
                blocks_.push_back(std::make_shared<Block>(static_cast<uint32_t>(blocks_.size()) * BLOCK_SIZE));
                return *(blocks_.end() - 1);
            } else {
                for (auto& block : blocks_) {
                    if (!block->filled_ && !block->reserved_) {
                        block->reserved_ = true;
                        return block;
                    }
                }
            }
            return nullptr;
        }

        bool is_complete() const {
            auto all_filled = std::all_of(blocks_.cbegin(), blocks_.cend(),
                                          [](auto block) { return block->filled(); });
            return blocks_.size() == block_count() && all_filled;
        }

        CompletePiece finalize() {
            char* data = new char[piece_size_];
            char* ptr = data;

            for (const auto& block : blocks_) {
                std::copy(block->data_, block->data_ + block->len_, ptr);
                ptr += block->len_;
            }
            return CompletePiece{data, index_, piece_size_};
        }

    private:
        uint32_t index_;
        uint32_t piece_size_;
        std::vector<std::shared_ptr<Block>> blocks_;

        uint32_t block_count() const {
            if (piece_size_ % BLOCK_SIZE == 0) {
                return piece_size_ / BLOCK_SIZE;
            } else {
                return piece_size_ / BLOCK_SIZE + 1;
            }
        }
    };

    struct Address {
        explicit Address(const char *bytes)
            : raw(ntohl(*reinterpret_cast<const uint32_t *>(bytes))),
              port(ntohs(*reinterpret_cast<const uint16_t *>(&bytes[4]))) {}

        [[nodiscard]] std::string to_string() const { return ip() + ":" + port_str(); }

        [[nodiscard]] std::string ip() const;
        [[nodiscard]] std::string port_str() const;

        uint32_t raw;
        uint16_t port;

    private:
        Address() = default;
    };

    class Bitfield {
    public:
        void copy_from(const std::vector<char>& vec) {
            data_.clear();
            data_.reserve(vec.size() * 8);
            for (char c : vec) {
                for (uint8_t i = 1 << 7; i > 0; i >>= 1) {
                    data_.push_back(c & i);
                }
            }
        }

        bool get(size_t index) {
            if (index >= data_.size()) return false;
            return data_[index];
        }

        size_t size() const { return data_.size(); }

    private:
        std::vector<bool> data_;
    };

}
#endif
