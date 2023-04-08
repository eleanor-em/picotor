#ifndef PICOTOR_COMMON_HPP
#define PICOTOR_COMMON_HPP

#include <chrono>
#include <fstream>
#include <iomanip>
#include <map>
#include <string>
#include <vector>

#include <cstring>

#include <netinet/in.h>

#include <sha1.hpp>

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

    struct Address {
        explicit Address(const char *bytes)
            : raw(ntohl(*reinterpret_cast<const uint32_t *>(bytes))),
              port(ntohs(*reinterpret_cast<const uint16_t *>(&bytes[4]))) {}

        [[nodiscard]] std::string to_string() const { return ip() + ":" + port_str(); }

        [[nodiscard]] std::string ip() const;
        [[nodiscard]] std::string port_str() const;

        uint32_t raw;
        uint16_t port;

        Address(const Address& rhs) = default;
        bool operator==(const Address& rhs) const = default;
        Address& operator=(const Address& rhs) = default;
        Address& operator=(Address&& rhs) = default;

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

namespace std {
    template<>
    struct hash<cmn::Address> {
        std::size_t operator()(const cmn::Address& k) const {
            return hash<uint64_t>()(static_cast<uint64_t>(k.raw) + k.port);
        }
    };
}
#endif
