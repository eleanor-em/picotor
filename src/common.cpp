//
// Created by eleanor on 20.02.23.
//
#include "common.hpp"
#include <vector>

// https://gist.github.com/klmr/849cbb0c6e872dff0fdcc54787a66103
std::string cmn::read_file(std::string_view path) {
    constexpr auto read_size = std::size_t{4096};
    auto stream = std::ifstream{path.data()};
    stream.exceptions(std::ios_base::badbit);

    auto out = std::string{};
    auto buf = std::string(read_size, '\0');
    while (stream.read(&buf[0], read_size)) {
        out.append(buf, 0, stream.gcount());
    }
    out.append(buf, 0, stream.gcount());
    return out;
}

// http://help.adobe.com/en_US/FlashPlatform/reference/actionscript/3/package.html#encodeURIComponent()
void hexchar(unsigned char c, unsigned char &hex1, unsigned char &hex2) {
    hex1 = c / 16;
    hex2 = c % 16;
    hex1 += hex1 <= 9 ? '0' : 'a' - 10;
    hex2 += hex2 <= 9 ? '0' : 'a' - 10;
}

std::string cmn::urlencode(const std::vector<char> &s) {
    return cmn::urlencode(std::string{s.begin(), s.end()});
}

std::string cmn::urlencode(const std::string &s) {
    const char *str = s.c_str();
    std::vector<char> v(s.size());
    v.clear();
    for (size_t i = 0, l = s.size(); i < l; i++) {
        char c = str[i];
        if ((c >= '0' && c <= '9') ||
            (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            c == '-' || c == '_' || c == '.' || c == '!' || c == '~' ||
            c == '*' || c == '\'' || c == '(' || c == ')') {
            v.push_back(c);
        } else if (c == ' ') {
            v.push_back('+');
        } else {
            v.push_back('%');
            unsigned char d1, d2;
            hexchar(c, d1, d2);
            v.push_back(d1);
            v.push_back(d2);
        }
    }

    return std::string(v.cbegin(), v.cend());
}

// https://stackoverflow.com/questions/17261798/converting-a-hex-string-to-a-byte-array
std::vector<char> cmn::hex_to_bytes(const std::string &hex) {
    std::vector<char> bytes;
    bytes.reserve(hex.length() / 2);

    for (unsigned int i = 0; i < hex.length(); i += 2) {
        auto byteString = hex.substr(i, 2);
        char byte = (char) strtol(byteString.c_str(), NULL, 16);
        bytes.emplace_back(byte);
    }

    return bytes;
}

cmn::Hash cmn::Hash::of(const std::string &str) {
    return cmn::Hash(cmn::hex_to_bytes(SHA1::from_string(str)));
}
