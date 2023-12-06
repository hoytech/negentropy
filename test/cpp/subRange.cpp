#include <iostream>

#include <hoytech/error.h>
#include <hoytech/hex.h>

#include "negentropy.h"
#include "negentropy/storage/Vector.h"
#include "negentropy/storage/BTreeMem.h"
#include "negentropy/storage/SubRange.h"



template<typename T>
void testSubRange() {
    T vecBig;
    T vecSmall;

    for (size_t i = 0; i < 1000; i++) {
        vecBig.insert(100 + i, std::string(32, (unsigned char)(i % 256)));
    }

    for (size_t i = 400; i < 600; i++) {
        vecSmall.insert(100 + i, std::string(32, (unsigned char)(i % 256)));
    }

    vecBig.seal();
    vecSmall.seal();

    negentropy::storage::SubRange subRange(vecBig, negentropy::Bound(100 + 400), negentropy::Bound(100 + 600));

    if (vecSmall.size() != subRange.size()) throw hoytech::error("size mismatch");

    if (vecSmall.fingerprint(0, vecSmall.size()).sv() != subRange.fingerprint(0, subRange.size()).sv()) throw hoytech::error("fingerprint mismatch");

    if (vecSmall.getItem(10) != subRange.getItem(10)) throw hoytech::error("getItem mismatch");
    if (vecBig.getItem(400 + 10) != subRange.getItem(10)) throw hoytech::error("getItem mismatch");

    {
        auto lb = subRange.findLowerBound(0, subRange.size(), negentropy::Bound(550));
        auto lb2 = vecSmall.findLowerBound(0, vecSmall.size(), negentropy::Bound(550));
        if (lb != lb2) throw hoytech::error("findLowerBound mismatch");
    }

    {
        auto lb = subRange.findLowerBound(0, subRange.size(), negentropy::Bound(20));
        auto lb2 = vecSmall.findLowerBound(0, vecSmall.size(), negentropy::Bound(20));
        if (lb != lb2) throw hoytech::error("findLowerBound mismatch");
    }

    {
        auto lb = subRange.findLowerBound(0, subRange.size(), negentropy::Bound(5000));
        auto lb2 = vecSmall.findLowerBound(0, vecSmall.size(), negentropy::Bound(5000));
        if (lb != lb2) throw hoytech::error("findLowerBound mismatch");
    }
}


int main() {
    testSubRange<negentropy::storage::Vector>();
    testSubRange<negentropy::storage::BTreeMem>();

    std::cout << "OK" << std::endl;

    return 0;
}
