//
// Created by eleanor on 20.02.23.
//
#include <iostream>
#include <vector>

#include <common.hpp>

using namespace std;

// https://gist.github.com/klmr/849cbb0c6e872dff0fdcc54787a66103
string cmn::read_file(string_view path) {
    constexpr auto read_size = size_t{4096};
    auto stream = ifstream{path.data()};
    stream.exceptions(ios_base::badbit);

    auto out = string{};
    auto buf = string(read_size, '\0');
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

string cmn::urlencode(const vector<char>& s) {
    return cmn::urlencode(string{s.begin(), s.end()});
}

string cmn::urlencode(const string &s) {
    const char *str = s.c_str();
    vector<char> v;
    v.reserve(s.size());
    for (size_t i = 0, l = s.size(); i < l; i++) {
        char c = str[i];
        if ((c >= '0' && c <= '9') ||
            (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            c == '-' || c == '_' || c == '.' || c == '!' || c == '~' ||
            c == '*' || c == '\'' || c == '(' || c == ')') {
            v.emplace_back(c);
        } else if (c == ' ') {
            v.emplace_back('+');
        } else {
            v.emplace_back('%');
            unsigned char d1, d2;
            hexchar(c, d1, d2);
            v.emplace_back(d1);
            v.emplace_back(d2);
        }
    }

    return string(v.cbegin(), v.cend());
}

// https://stackoverflow.com/questions/17261798/converting-a-hex-string-to-a-byte-array
vector<char> cmn::hex_to_bytes(const string &hex) {
    vector<char> bytes;
    bytes.reserve(hex.length() / 2);

    for (unsigned int i = 0; i < hex.length(); i += 2) {
        auto byteString = hex.substr(i, 2);
        char byte = (char) strtol(byteString.c_str(), NULL, 16);
        bytes.emplace_back(byte);
    }

    return bytes;
}

cmn::Hash cmn::Hash::of(const string &str) {
    return cmn::Hash{cmn::hex_to_bytes(SHA1::from_string(str))};
}

string cmn::Hash::as_hex() const {
    ostringstream ss;
    ss << hex << setfill('0');
    for (auto i : bytes_) {
        ss << setw(2) << (int)(uint8_t)(i);
    }
    return ss.str();
}

string cmn::Address::ip() const {
    stringstream ss;
    ss << ((raw >> 24) & 0xff) << "."
       << ((raw >> 16) & 0xff) << "."
       << ((raw >> 8) & 0xff) << "."
       << (raw & 0xff);
    return ss.str();
}

string cmn::Address::port_str() const {
    stringstream ss;
    ss << port;
    return ss.str();
}
