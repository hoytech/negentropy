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
        subBegin = lowerBound == Bound(0) ? 0 : base.findLowerBound(0, lowerBound);
        subEnd = upperBound == Bound(MAX_U64) ? baseSize : base.findLowerBound(subBegin, upperBound);
        if (subEnd != baseSize && Bound(base.getItem(subEnd)) == upperBound) subEnd++; // instead of upper_bound: OK because items are unique
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

    size_t findLowerBound(size_t begin, const Bound &bound) {
        checkBounds(begin, subEnd);

        auto ret = base.findLowerBound(subBegin + begin, bound);
        return ret >= subEnd ? size() : ret - subBegin;
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
