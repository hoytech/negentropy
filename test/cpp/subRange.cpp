#include <iostream>
#include <set>

#include <openssl/sha.h>

#include <hoytech/error.h>
#include <hoytech/hex.h>

#include "negentropy.h"
#include "negentropy/storage/Vector.h"
#include "negentropy/storage/BTreeMem.h"
#include "negentropy/storage/SubRange.h"



std::string sha256(std::string_view input) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(input.data()), input.size(), hash);
    return std::string((const char*)&hash[0], SHA256_DIGEST_LENGTH);
}

std::string uintToId(uint64_t id) {
    return sha256(std::string((char*)&id, 8));
}


template<typename T>
void testSubRange() {
    T vecBig;
    T vecSmall;

    for (size_t i = 0; i < 1000; i++) {
        vecBig.insert(100 + i, uintToId(i));
    }

    for (size_t i = 400; i < 600; i++) {
        vecSmall.insert(100 + i, uintToId(i));
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



template<typename T>
void testSync(bool emptySide1, bool emptySide2) {
    T vecBig;
    T vecSmall;

    std::set<std::string> expectedHave;
    std::set<std::string> expectedNeed;

    size_t const lowerLimit = 20'000;
    size_t const upperLimit = 90'000;

    for (size_t i = lowerLimit; i < upperLimit; i++) {
        auto id = uintToId(i);
        if (emptySide1 || i % 15'000 == 0) {
            if (i >= lowerLimit && i < upperLimit) expectedNeed.insert(id);
            continue;
        }
        vecSmall.insert(100 + i, id);
    }

    for (size_t i = 0; i < 100'000; i++) {
        auto id = uintToId(i);
        if (emptySide2 || i % 22'000 == 0) {
            if (i >= lowerLimit && i < upperLimit) expectedHave.insert(id);
            continue;
        }
        vecBig.insert(100 + i, id);
    }

    // Get rid of common

    std::set<std::string> commonItems;

    for (const auto &item : expectedHave) {
        if (expectedNeed.contains(item)) commonItems.insert(item);
    }

    for (const auto &item : commonItems) {
        expectedHave.erase(item);
        expectedNeed.erase(item);
    }


    vecBig.seal();
    vecSmall.seal();

    negentropy::storage::SubRange subRange(vecBig, negentropy::Bound(100 + lowerLimit), negentropy::Bound(100 + upperLimit));


    auto ne1 = Negentropy(vecSmall, 20'000);
    auto ne2 = Negentropy(subRange, 20'000);

    std::string msg = ne1.initiate();

    while (true) {
        msg = ne2.reconcile(msg);

        std::vector<std::string> have, need;
        auto newMsg = ne1.reconcile(msg, have, need);

        for (const auto &item : have) {
            if (!expectedHave.contains(item)) throw hoytech::error("unexpected have: ", hoytech::to_hex(item));
            expectedHave.erase(item);
        }

        for (const auto &item : need) {
            if (!expectedNeed.contains(item)) throw hoytech::error("unexpected need: ", hoytech::to_hex(item));
            expectedNeed.erase(item);
        }

        if (!newMsg) break;
        else std::swap(msg, *newMsg);
    }

    if (expectedHave.size()) throw hoytech::error("missed have");
    if (expectedNeed.size()) throw hoytech::error("missed need");
}




int main() {
    testSubRange<negentropy::storage::Vector>();
    testSubRange<negentropy::storage::BTreeMem>();

    testSync<negentropy::storage::Vector>(false, false);
    testSync<negentropy::storage::Vector>(true, false);
    testSync<negentropy::storage::Vector>(false, true);

    std::cout << "OK" << std::endl;

    return 0;
}
