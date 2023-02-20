#ifndef PICOTOR_COMMON_HPP
#define PICOTOR_COMMON_HPP
#include <string>
#include <fstream>
#include <cstring>
#include <iomanip>
#include <sha1.hpp>

namespace cmn {
// https://gist.github.com/klmr/849cbb0c6e872dff0fdcc54787a66103
    auto read_file(std::string_view path) -> std::string;

    const size_t HASH_SIZE = 20; // bytes

    struct Hash {
        static Hash from(const std::string& str) {
            SHA1 hash;
            hash.update(str);
            return Hash(str.c_str());
        }

        // constructor so I can use emplace
        explicit Hash(const char *ptr) {
            std::memcpy(data, ptr, HASH_SIZE);
        }

        Hash() = default;

        [[nodiscard]] std::string as_str() const {
            std::ostringstream ss;
            ss << std::hex << std::setfill('0');
            for (auto i : data) {
                ss << std::setw(2) << (int) i;
            }
            return ss.str();
        }

        uint8_t data[HASH_SIZE]{};
    };
}
#endif