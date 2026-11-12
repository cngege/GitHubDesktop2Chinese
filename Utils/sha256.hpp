// SHA-256 实现（FIPS 180-4）：自包含，无第三方依赖。
//
// 用途：自更新下载完成后校验二进制完整性。
// 不使用 OpenSSL 的 SHA256_Init/Update/Final：那组接口在 OpenSSL 3.x 已标记弃用，
// 而 Windows 预编译库与 macOS/Linux 系统库的 OpenSSL 版本并不一致，
// 自带实现可让三端行为完全一致且无弃用告警。
//
// 参考测试向量见 tests/test_platform_logic.cpp（CI 在三个平台都会跑）。
#ifndef UTILS_SHA256_HPP
#define UTILS_SHA256_HPP

#include <cstddef>
#include <cstdint>
#include <string>

namespace sha256 {

inline constexpr std::size_t kDigestLength = 32;

// 轮常量
inline constexpr std::uint32_t kK[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2 };

inline std::uint32_t rotr(std::uint32_t x, int n) {
    return (x >> n) | (x << (32 - n));
}

inline std::string to_hex(const unsigned char* data, std::size_t len) {
    static const char* hex_digits = "0123456789abcdef";
    std::string out;
    out.reserve(len * 2);
    for (std::size_t i = 0; i < len; ++i) {
        out.push_back(hex_digits[data[i] >> 4]);
        out.push_back(hex_digits[data[i] & 0x0f]);
    }
    return out;
}

class Sha256 {
public:
    Sha256() { reset(); }

    void reset() {
        h_[0] = 0x6a09e667; h_[1] = 0xbb67ae85; h_[2] = 0x3c6ef372; h_[3] = 0xa54ff53a;
        h_[4] = 0x510e527f; h_[5] = 0x9b05688c; h_[6] = 0x1f83d9ab; h_[7] = 0x5be0cd19;
        total_len_ = 0;
        buf_len_ = 0;
    }

    void update(const unsigned char* data, std::size_t len) {
        total_len_ += len;
        while (len > 0) {
            const std::size_t take = (64 - buf_len_ < len) ? (64 - buf_len_) : len;
            for (std::size_t i = 0; i < take; ++i) {
                buf_[buf_len_ + i] = data[i];
            }
            buf_len_ += take;
            data += take;
            len -= take;
            if (buf_len_ == 64) {
                transform(buf_);
                buf_len_ = 0;
            }
        }
    }

    void update(const std::string& data) {
        update(reinterpret_cast<const unsigned char*>(data.data()), data.size());
    }

    // 补齐并返回小写十六进制摘要；调用后对象状态不可继续使用
    std::string final_hex() {
        const std::uint64_t bit_len = total_len_ * 8;

        const unsigned char pad_byte = 0x80;
        update(&pad_byte, 1);

        const unsigned char zero_byte = 0x00;
        while (buf_len_ != 56) {
            update(&zero_byte, 1);
        }

        unsigned char len_bytes[8];
        for (int i = 0; i < 8; ++i) {
            len_bytes[7 - i] = static_cast<unsigned char>((bit_len >> (8 * i)) & 0xff);
        }
        update(len_bytes, 8);

        unsigned char digest[kDigestLength];
        for (int i = 0; i < 8; ++i) {
            digest[4 * i + 0] = static_cast<unsigned char>((h_[i] >> 24) & 0xff);
            digest[4 * i + 1] = static_cast<unsigned char>((h_[i] >> 16) & 0xff);
            digest[4 * i + 2] = static_cast<unsigned char>((h_[i] >> 8) & 0xff);
            digest[4 * i + 3] = static_cast<unsigned char>(h_[i] & 0xff);
        }
        return to_hex(digest, kDigestLength);
    }

private:
    void transform(const unsigned char* block) {
        std::uint32_t w[64];
        for (int i = 0; i < 16; ++i) {
            w[i] = (static_cast<std::uint32_t>(block[4 * i + 0]) << 24)
                 | (static_cast<std::uint32_t>(block[4 * i + 1]) << 16)
                 | (static_cast<std::uint32_t>(block[4 * i + 2]) << 8)
                 | (static_cast<std::uint32_t>(block[4 * i + 3]));
        }
        for (int i = 16; i < 64; ++i) {
            const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }

        std::uint32_t a = h_[0], b = h_[1], c = h_[2], d = h_[3];
        std::uint32_t e = h_[4], f = h_[5], g = h_[6], hh = h_[7];

        for (int i = 0; i < 64; ++i) {
            const std::uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            const std::uint32_t ch = (e & f) ^ ((~e) & g);
            const std::uint32_t t1 = hh + s1 + ch + kK[i] + w[i];
            const std::uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t t2 = s0 + maj;

            hh = g; g = f; f = e; e = d + t1;
            d = c; c = b; b = a; a = t1 + t2;
        }

        h_[0] += a; h_[1] += b; h_[2] += c; h_[3] += d;
        h_[4] += e; h_[5] += f; h_[6] += g; h_[7] += hh;
    }

    std::uint32_t h_[8];
    unsigned char buf_[64];
    std::size_t buf_len_ = 0;
    std::uint64_t total_len_ = 0;
};

// 计算字符串的 SHA-256（小写十六进制）
inline std::string hex(const std::string& data) {
    Sha256 h;
    h.update(data);
    return h.final_hex();
}

// 提取文本中第一个长度为 64 的十六进制串（sha256sum 风格 "<hash>  <file>" 也能解析）
inline bool is_hex(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

inline bool extract_hash(const std::string& text, std::string& out) {
    std::size_t i = 0;
    while (i < text.size()) {
        if (is_hex(text[i])) {
            std::size_t j = i;
            while (j < text.size() && is_hex(text[j])) ++j;
            if (j - i == 64) {
                out.assign(text, i, 64);
                // 统一小写
                for (char& c : out) {
                    if (c >= 'A' && c <= 'F') c = static_cast<char>(c - 'A' + 'a');
                }
                return true;
            }
            i = j;
        }
        else {
            ++i;
        }
    }
    return false;
}

} // namespace sha256

#endif // UTILS_SHA256_HPP
