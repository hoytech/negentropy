#pragma once

#include "negentropy.h"



namespace negentropy {

struct NegentropyStorageVector : NegentropyStorageBase {
    std::vector<Item> items;
    bool sealed = false;

    void addItem(uint64_t createdAt, std::string_view id) {
        if (sealed) throw negentropy::err("already sealed");
        if (id.size() != ID_SIZE) throw negentropy::err("bad id size for added item");

        items.emplace_back(createdAt, id.substr(0, ID_SIZE));
    }

    void seal() {
        if (sealed) throw negentropy::err("already sealed");
        sealed = true;

        std::sort(items.begin(), items.end());

        for (size_t i = 1; i < items.size(); i++) {
            if (items[i - 1] == items[i]) throw negentropy::err("duplicate item inserted");
        }
    }

    void checkSealed() {
        if (!sealed) throw negentropy::err("not sealed");
    }

    uint64_t size() {
        checkSealed();
        return items.size();
    }

    const Item &getItem(size_t i) {
        return items.at(i);
    }

    void iterate(size_t begin, size_t end, std::function<void(const Item &)> cb) {
        checkSealed();
        if (begin > end || end > items.size()) throw negentropy::err("bad range");

        for (auto i = begin; i < end; ++i) cb(items[i]);
    }

    size_t findLowerBound(const Bound &bound) {
        checkSealed();
        return std::lower_bound(items.begin(), items.end(), bound.item) - items.begin();
    }

    Fingerprint fingerprint(size_t begin, size_t end) {
        checkSealed();
        if (begin > end || end > items.size()) throw negentropy::err("bad range");

        Accumulator out;
        out.setToZero();

        for (size_t i = begin; i < end; ++i) {
            out.add(items[i]);
        }

        return out.getFingerprint(end - begin);
    }
};


}
