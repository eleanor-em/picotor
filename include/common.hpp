#ifndef PICOTOR_COMMON_HPP
#define PICOTOR_COMMON_HPP
#include <string>
#include <fstream>
#include <cstring>
#include <vector>
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
            bytes_.reserve(HASH_SIZE);
            std::copy(bytes, bytes + HASH_SIZE, bytes_.begin());
        }

        Hash() = default;

        [[nodiscard]] std::string as_hex() const {
            std::ostringstream ss;
            ss << std::hex << std::setfill('0');
            for (auto i : bytes_) {
                ss << std::setw(2) << (int)(uint8_t)(i);
            }
            return ss.str();
        }

        [[nodiscard]] const std::vector<char>& as_bytes() const {
            return bytes_;
        }

        [[nodiscard]] std::string as_byte_string() const {
            return std::string{bytes_.begin(), bytes_.end()};
        }
    private:
        std::vector<char> bytes_;
    };

    struct Address {
        static Address from_str(const char* str) {
            std::stringstream ss{str};
            char ch;
            uint16_t port, a, b, c, d;
            ss >> a >> ch >> b >> ch >> c >> ch >> d >> ch >> port;
            Address addr;
            addr.raw = (a << 24) | (b << 16) | (c << 8) | (d);
            addr.port = port;
            return addr;
        }

        Address(const char *bytes)
            : raw(ntohl(*reinterpret_cast<const uint32_t *>(bytes))),
              port(ntohs(*reinterpret_cast<const uint16_t *>(&bytes[4]))) {}

        [[nodiscard]] std::string as_str() const {
            return ip() + ":" + port_str();
        }

        [[nodiscard]] std::string ip() const {
            std::stringstream ss;
            ss << ((raw >> 24) & 0xff) << "."
               << ((raw >> 16) & 0xff) << "."
               << ((raw >> 8) & 0xff) << "."
               << (raw & 0xff);
            return ss.str();
        }

        [[nodiscard]] std::string port_str() const {
            std::stringstream ss;
            ss << port;
            return ss.str();
        }

        uint32_t raw;
        uint16_t port;

    private:
        Address() = default;
    };

    std::string urlencode(const std::vector<char> &&s);
}
#endif
