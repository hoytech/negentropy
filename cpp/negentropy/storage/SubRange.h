#pragma once

#include "negentropy.h"



namespace negentropy { namespace storage {


struct SubRange : StorageBase {
    StorageBase &base;
    size_t baseSize;
    size_t subBegin;
    size_t subEnd;

    SubRange(StorageBase &base, const Bound &lowerBound, const Bound &upperBound) : base(base) {
        baseSize = base.size();
        subBegin = findLowerBound(0, baseSize, lowerBound);
        subEnd = findLowerBound(subBegin, baseSize, upperBound);
        if (subEnd != baseSize && Bound(getItem(subEnd)) == upperBound) subEnd++; // instead of upper_bound: OK because items are unique
    }

    uint64_t size() {
        return subEnd - subBegin;
    }

    const Item &getItem(size_t i) {
        if (i >= baseSize) throw negentropy::err("bad index");
        return base.getItem(subBegin + i);
    }

    void iterate(size_t begin, size_t end, std::function<bool(const Item &, size_t)> cb) {
        checkBounds(begin, end);

        base.iterate(subBegin + begin, subBegin + end, cb);
    }

    size_t findLowerBound(size_t begin, size_t end, const Bound &bound) {
        checkBounds(begin, end);

        return base.findLowerBound(subBegin + begin, subBegin + end, bound);
    }

    Fingerprint fingerprint(size_t begin, size_t end) {
        checkBounds(begin, end);

        return base.fingerprint(subBegin + begin, subBegin + end);
    }

  private:
    void checkBounds(size_t begin, size_t end) {
        if (begin > end || end > subEnd) throw negentropy::err("bad range");
    }
};


}}
