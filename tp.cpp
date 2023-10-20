#include <iostream>
#include <cstring>
#include <string_view>
#include <hoytech/hex.h>

#include <bit>
#include <algorithm>

struct Fingerprint {
    uint8_t buf[32];

    void setToZero() {
        memset(buf, '\0', sizeof(buf));
    }

    void add(const Fingerprint &other) {
        uint64_t currCarry = 0, nextCarry = 0;
        uint64_t *p = reinterpret_cast<uint64_t*>(buf);
        const uint64_t *po = reinterpret_cast<const uint64_t*>(other.buf);

        auto byteswap = [](uint64_t &n) {
            uint8_t *first = reinterpret_cast<uint8_t*>(&n);
            uint8_t *last = first + 8;
            std::reverse(first, last);
        };

        for (size_t i = 0; i < 4; i++) {
            uint64_t orig = p[i];
            uint64_t otherV = po[i];

            if constexpr (std::endian::native == std::endian::big) {
                byteswap(orig);
                byteswap(otherV);
            } else {
                static_assert(std::endian::native == std::endian::little);
            }

            uint64_t next = orig;

            next += currCarry;
            if (next < orig) nextCarry = 1;

            next += otherV;
            if (next < otherV) nextCarry = 1;

            if constexpr (std::endian::native == std::endian::big) {
                byteswap(next);
            }

            p[i] = next;
            currCarry = nextCarry;
            nextCarry = 0;
        }
    }

    void negate() {
        for (size_t i = 0; i < 32; i++) {
            buf[i] = ~buf[i];
        }

        Fingerprint zero;
        zero.setToZero();
        zero.buf[0] = 1;
        add(zero);
    }

    std::string_view sv() const {
        return std::string_view(reinterpret_cast<const char*>(buf), 32);
    }
};



int main() {
    Fingerprint f1, f2;
    f1.setToZero();
    f2.setToZero();

    f1.buf[0] = 20;
    f1.negate();
    std::cout << hoytech::to_hex(f1.sv()) << std::endl;

    f2.buf[0] = 30;
    std::cout << hoytech::to_hex(f2.sv()) << std::endl;

    f2.add(f1);

    std::cout << hoytech::to_hex(f2.sv()) << std::endl;

    return 0;
}
