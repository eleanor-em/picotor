#ifndef PICOTOR_COMMON_HPP
#define PICOTOR_COMMON_HPP
#include <string>
#include <fstream>
#include <cstring>
#include <vector>
#include <iomanip>
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
        explicit Hash(const std::vector<char>&& vec): _bytes(vec) {}
        explicit Hash(const std::vector<char>& vec): _bytes(vec) {}
        explicit Hash(const char *bytes) {
            _bytes.reserve(HASH_SIZE);
            std::copy(bytes, bytes + HASH_SIZE, _bytes.begin());
        }

        Hash() = default;

        [[nodiscard]] std::string as_hex() const {
            std::ostringstream ss;
            ss << std::hex << std::setfill('0');
            int foo = 0;
            for (auto i : _bytes) {
                ss << std::setw(2) << (int)(uint8_t)(i);
            }
            return ss.str();
        }

        [[nodiscard]] const std::vector<char>& as_bytes() const {
            return _bytes;
        }
    private:
        std::vector<char> _bytes;
    };
}
#endif