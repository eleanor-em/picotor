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
    using std::string;
    using std::string_view;
    using std::vector;

    string read_file(string_view path);
    string urlencode(const vector<char>& s);
    string urlencode(const string& s);
    vector<char> hex_to_bytes(const string& hex);

    inline void push_bytes(vector<char>* vec, uint32_t data) {
        vec->push_back(static_cast<char>(data >> 0));
        vec->push_back(static_cast<char>(data >> 8));
        vec->push_back(static_cast<char>(data >> 16));
        vec->push_back(static_cast<char>(data >> 24));
    }

    const size_t HASH_SIZE = 20; // bytes

    class Hash {
    public:
        static Hash of(const std::string& str);

        // constructors so we can use emplace
        explicit Hash(const vector<char>&& vec): bytes_(vec) { assert(vec.size() == HASH_SIZE); }
        explicit Hash(const vector<char>& vec): bytes_(vec) { assert(vec.size() == HASH_SIZE); }
        explicit Hash(const char *bytes) {
            bytes_.resize(HASH_SIZE);
            std::copy(bytes, bytes + HASH_SIZE, bytes_.begin());
        }
        explicit Hash(const string& str) {
            assert(str.length() == HASH_SIZE);
            bytes_.resize(HASH_SIZE);
            std::copy(str.begin(), str.end(), bytes_.begin());
        }

        Hash() { bytes_.resize(HASH_SIZE); }

        [[nodiscard]] string as_hex() const;
        [[nodiscard]] const vector<char>& as_bytes() const { return bytes_; }

        bool operator==(const Hash& rhs) const = default;
    private:
        vector<char> bytes_;
    };

    struct Address {
        explicit Address(const char *bytes)
            : raw(ntohl(*reinterpret_cast<const uint32_t *>(bytes))),
              port(ntohs(*reinterpret_cast<const uint16_t *>(&bytes[4]))) {}

        [[nodiscard]] string to_string() const { return ip() + ":" + port_str(); }

        [[nodiscard]] string ip() const;
        [[nodiscard]] string port_str() const;

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
        void copy_from(const vector<char>& vec) {
            data_.clear();
            data_.reserve(vec.size() * 8);
            for (char c : vec) {
                for (uint8_t i = 1 << 7; i > 0; i >>= 1) {
                    data_.push_back(c & i);
                }
            }
        }

        [[nodiscard]] bool get(size_t index) const {
            if (index >= data_.size()) return false;
            return data_[index];
        }

        [[nodiscard]] size_t size() const { return data_.size(); }

    private:
        vector<bool> data_;
    };
}

namespace std {
    using cmn::Address;

    template<>
    struct hash<Address> {
        size_t operator()(const Address& k) const {
            return hash<uint64_t>()(static_cast<uint64_t>(k.raw) + k.port);
        }
    };
}
#endif
