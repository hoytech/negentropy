// (C) 2023 Doug Hoyte. MIT license

#pragma once

#include <string.h>

#include <string>
#include <string_view>
#include <vector>
#include <deque>
#include <unordered_set>
#include <limits>
#include <algorithm>
#include <stdexcept>

#include <openssl/sha.h>


namespace negentropy {


const uint64_t MAX_U64 = std::numeric_limits<uint64_t>::max();
using err = std::runtime_error;


enum class Mode {
    Skip = 0,
    Fingerprint = 1,
    IdList = 2,
    Deprecated = 3,
    Continuation = 4,
};


struct Item {
    uint64_t timestamp;
    uint64_t idSize;
    char id[32];

    Item(uint64_t timestamp = 0) : timestamp(timestamp), idSize(0) {
        memset(id, '\0', sizeof(id));
    }

    Item(uint64_t timestamp, std::string_view id_) : timestamp(timestamp), idSize(id_.size()) {
        if (id_.size() > 32) throw negentropy::err("id too big");
        memset(id, '\0', sizeof(id));
        memcpy(id, id_.data(), id_.size());
    }

    std::string_view getId() const {
        return std::string_view(id, idSize);
    }

    bool operator==(const Item &other) const {
        return timestamp == other.timestamp && getId() == other.getId();
    }
};

inline bool operator<(const Item &a, const Item &b) {
    return a.timestamp != b.timestamp ? a.timestamp < b.timestamp : a.getId() < b.getId();
};


struct Negentropy {
    uint64_t idSize;
    uint64_t frameSizeLimit;

    struct OutputRange {
        Item start;
        Item end;
        std::string payload;
    };

    std::vector<Item> addedItems;
    std::vector<uint64_t> itemTimestamps;
    std::string itemIds;
    bool sealed = false;
    bool isInitiator = false;
    bool continuationNeeded = false;
    std::deque<OutputRange> pendingOutputs;

    Negentropy(uint64_t idSize = 16, uint64_t frameSizeLimit = 0) : idSize(idSize), frameSizeLimit(frameSizeLimit) {
        if (idSize < 8 || idSize > 32) throw negentropy::err("idSize invalid");
        if (frameSizeLimit != 0 && frameSizeLimit < 4096) throw negentropy::err("frameSizeLimit too small");
    }

    void addItem(uint64_t createdAt, std::string_view id) {
        if (sealed) throw negentropy::err("already sealed");
        if (id.size() < idSize) throw negentropy::err("bad id size");

        addedItems.emplace_back(createdAt, id.substr(0, idSize));
    }

    void seal() {
        if (sealed) throw negentropy::err("already sealed");
        sealed = true;

        std::sort(addedItems.begin(), addedItems.end());

        if (addedItems.size() > 1) {
            for (size_t i = 0; i < addedItems.size() - 1; i++) {
                if (addedItems[i] == addedItems[i + 1]) throw negentropy::err("duplicate item inserted");
            }
        }

        itemTimestamps.reserve(addedItems.size());
        itemIds.reserve(addedItems.size() * idSize);

        for (const auto &item : addedItems) {
            itemTimestamps.push_back(item.timestamp);
            itemIds += item.getId();
        }

        addedItems.clear();
        addedItems.shrink_to_fit();
    }

    std::string initiate() {
        if (!sealed) throw negentropy::err("not sealed");
        isInitiator = true;

        splitRange(0, numItems(), Item(0), Item(MAX_U64), pendingOutputs);

        return buildOutput();
    }

    std::string reconcile(std::string_view query) {
        if (isInitiator) throw negentropy::err("initiator not asking for have/need IDs");
        std::vector<std::string> haveIds, needIds;
        reconcileAux(query, haveIds, needIds);
        return buildOutput();
    }

    std::string reconcile(std::string_view query, std::vector<std::string> &haveIds, std::vector<std::string> &needIds) {
        if (!isInitiator) throw negentropy::err("non-initiator asking for have/need IDs");
        reconcileAux(query, haveIds, needIds);
        return buildOutput();
    }

  private:
    size_t numItems() {
        return itemTimestamps.size();
    }

    std::string_view getItemId(size_t i) {
        return std::string_view(itemIds.data() + (i * idSize), idSize);
    }

    Item getItem(size_t i) {
        return Item(itemTimestamps[i], getItemId(i));
    }

    std::string computeFingerprint(size_t lower, size_t num) {
        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256(reinterpret_cast<unsigned char*>(itemIds.data() + (lower * idSize)), num * idSize, hash);
        return std::string(reinterpret_cast<char*>(hash), idSize);
    }

    void reconcileAux(std::string_view query, std::vector<std::string> &haveIds, std::vector<std::string> &needIds) {
        if (!sealed) throw negentropy::err("not sealed");
        continuationNeeded = false;

        Item prevBound;
        size_t prevIndex = 0;
        uint64_t lastTimestampIn = 0;
        std::deque<OutputRange> outputs;

        while (query.size()) {
            auto currBound = decodeBound(query, lastTimestampIn);
            auto mode = Mode(decodeVarInt(query));

            auto lower = prevIndex;
            auto upper = findUpperBound(prevIndex, numItems(), currBound);

            if (mode == Mode::Skip) {
                // Do nothing
            } else if (mode == Mode::Fingerprint) {
                auto theirFingerprint = getBytes(query, idSize);
                auto ourFingerprint = computeFingerprint(lower, upper - lower);

                if (theirFingerprint != ourFingerprint) {
                    splitRange(lower, upper, prevBound, currBound, outputs);
                }
            } else if (mode == Mode::IdList) {
                auto numIds = decodeVarInt(query);

                std::unordered_set<std::string> theirElems;
                for (uint64_t i = 0; i < numIds; i++) {
                    auto e = getBytes(query, idSize);
                    theirElems.insert(e);
                }

                for (auto i = lower; i < upper; ++i) {
                    auto k = std::string(getItemId(i));

                    if (theirElems.find(k) == theirElems.end()) {
                        // ID exists on our side, but not their side
                        if (isInitiator) haveIds.emplace_back(k);
                    } else {
                        // ID exists on both sides
                        theirElems.erase(k);
                    }
                }

                if (isInitiator) {
                    for (const auto &k : theirElems) {
                        // ID exists on their side, but not our side
                        needIds.emplace_back(k);
                    }
                } else {
                    std::vector<std::string> responseHaveIds;

                    auto it = lower;
                    bool didSplit = false;
                    Item splitBound;

                    auto flushIdListOutput = [&]{
                        std::string payload = encodeVarInt(uint64_t(Mode::IdList));

                        payload += encodeVarInt(responseHaveIds.size());
                        for (const auto &id : responseHaveIds) payload += id;

                        auto nextSplitBound = it + 1 >= upper ? currBound : getMinimalBound(getItem(it), getItem(it + 1));

                        outputs.emplace_back(OutputRange({
                            didSplit ? splitBound : prevBound,
                            nextSplitBound,
                            std::move(payload)
                        }));

                        splitBound = nextSplitBound;
                        didSplit = true;

                        responseHaveIds.clear();
                    };

                    for (; it < upper; ++it) {
                        responseHaveIds.emplace_back(getItemId(it));
                        if (responseHaveIds.size() >= 100) flushIdListOutput(); // 100*32 is less than minimum frame size limit of 4k
                    }

                    flushIdListOutput();
                }
            } else if (mode == Mode::Deprecated) {
                throw negentropy::err("other side is speaking old negentropy protocol");
            } else if (mode == Mode::Continuation) {
                continuationNeeded = true;
            } else {
                throw negentropy::err("unexpected mode");
            }

            prevIndex = upper;
            prevBound = currBound;
        }

        while (outputs.size()) {
            pendingOutputs.emplace_front(std::move(outputs.back()));
            outputs.pop_back();
        }
    }

    void splitRange(size_t lower, size_t upper, const Item &lowerBound, const Item &upperBound, std::deque<OutputRange> &outputs) {
        uint64_t numElems = upper - lower;
        const uint64_t buckets = 16;

        if (numElems < buckets * 2) {
            std::string payload = encodeVarInt(uint64_t(Mode::IdList));
            payload += encodeVarInt(numElems);
            for (auto i = lower; i < upper; i++) payload += getItemId(i);

            outputs.emplace_back(OutputRange({
                lowerBound,
                upperBound,
                std::move(payload)
            }));
        } else {
            uint64_t itemsPerBucket = numElems / buckets;
            uint64_t bucketsWithExtra = numElems % buckets;
            auto curr = lower;
            Item prevBound = getItem(curr);

            for (uint64_t i = 0; i < buckets; i++) {
                auto bucketSize = itemsPerBucket + (i < bucketsWithExtra ? 1 : 0);
                auto ourFingerprint = computeFingerprint(curr, bucketSize);
                curr += bucketSize;

                std::string payload = encodeVarInt(uint64_t(Mode::Fingerprint));
                payload += ourFingerprint;

                outputs.emplace_back(OutputRange({
                    i == 0 ? lowerBound : prevBound,
                    curr == upper ? upperBound : getMinimalBound(getItem(curr - 1), getItem(curr)),
                    std::move(payload)
                }));

                prevBound = outputs.back().end;
            }

            outputs.back().end = upperBound;
        }
    }

    std::string buildOutput() {
        std::string output;
        Item currBound;
        uint64_t lastTimestampOut = 0;

        std::sort(pendingOutputs.begin(), pendingOutputs.end(), [](const auto &a, const auto &b){ return a.start < b.start; });

        while (pendingOutputs.size()) {
            std::string o;

            auto &p = pendingOutputs.front();

            // If bounds are out of order or overlapping, finish and resume next time (shouldn't happen because of sort above)
            if (p.start < currBound) break;

            if (currBound != p.start) {
                o += encodeBound(p.start, lastTimestampOut);
                o += encodeVarInt(uint64_t(Mode::Skip));
            }

            o += encodeBound(p.end, lastTimestampOut);
            o += p.payload;

            if (frameSizeLimit && output.size() + o.size() > frameSizeLimit - 5) break; // 5 leaves room for Continuation
            output += o;

            currBound = p.end;
            pendingOutputs.pop_front();
        }

        // Server indicates that it has more to send, OR ensure client sends a non-empty message

        if ((!isInitiator && pendingOutputs.size()) || (isInitiator && output.size() == 0 && continuationNeeded)) {
            output += encodeBound(Item(MAX_U64), lastTimestampOut);
            output += encodeVarInt(uint64_t(Mode::Continuation));
        }

        return output;
    }

    size_t findUpperBound(size_t first, size_t last, const Item &value) {
        size_t count = last - first;

        while (count > 0) {
            size_t it = first;
            size_t step = count / 2;
            it += step;

            if (value.timestamp == itemTimestamps[it] ? value.getId() < getItemId(it) : value.timestamp < itemTimestamps[it]) {
                count = step;
            } else {
                first = ++it;
                count -= step + 1;
            }
        }

        return first;
    }


    // Decoding

    std::string getBytes(std::string_view &encoded, size_t n) {
        if (encoded.size() < n) throw negentropy::err("parse ends prematurely");
        auto res = encoded.substr(0, n);
        encoded = encoded.substr(n);
        return std::string(res);
    };

    uint64_t decodeVarInt(std::string_view &encoded) {
        uint64_t res = 0;

        while (1) {
            if (encoded.size() == 0) throw negentropy::err("premature end of varint");
            uint64_t byte = encoded[0];
            encoded = encoded.substr(1);
            res = (res << 7) | (byte & 0b0111'1111);
            if ((byte & 0b1000'0000) == 0) break;
        }

        return res;
    }

    uint64_t decodeTimestampIn(std::string_view &encoded, uint64_t &lastTimestampIn) {
        uint64_t timestamp = decodeVarInt(encoded);
        timestamp = timestamp == 0 ? MAX_U64 : timestamp - 1;
        timestamp += lastTimestampIn;
        if (timestamp < lastTimestampIn) timestamp = MAX_U64; // saturate
        lastTimestampIn = timestamp;
        return timestamp;
    }

    Item decodeBound(std::string_view &encoded, uint64_t &lastTimestampIn) {
        auto timestamp = decodeTimestampIn(encoded, lastTimestampIn);
        auto len = decodeVarInt(encoded);
        return Item(timestamp, getBytes(encoded, len));
    }


    // Encoding

    std::string encodeVarInt(uint64_t n) {
        if (n == 0) return std::string(1, '\0');

        std::string o;

        while (n) {
            o.push_back(static_cast<unsigned char>(n & 0x7F));
            n >>= 7;
        }

        std::reverse(o.begin(), o.end());

        for (size_t i = 0; i < o.size() - 1; i++) {
            o[i] |= 0x80;
        }

        return o;
    }

    std::string encodeTimestampOut(uint64_t timestamp, uint64_t &lastTimestampOut) {
        if (timestamp == MAX_U64) {
            lastTimestampOut = MAX_U64;
            return encodeVarInt(0);
        }

        uint64_t temp = timestamp;
        timestamp -= lastTimestampOut;
        lastTimestampOut = temp;
        return encodeVarInt(timestamp + 1);
    };

    std::string encodeBound(const Item &bound, uint64_t &lastTimestampOut) {
        std::string output;

        output += encodeTimestampOut(bound.timestamp, lastTimestampOut);
        output += encodeVarInt(bound.idSize);
        output += bound.getId();

        return output;
    };

    Item getMinimalBound(const Item &prev, const Item &curr) {
        if (curr.timestamp != prev.timestamp) {
            return Item(curr.timestamp, "");
        } else {
            uint64_t sharedPrefixBytes = 0;
            auto currKey = curr.getId();
            auto prevKey = prev.getId();

            for (uint64_t i = 0; i < idSize; i++) {
                if (currKey[i] != prevKey[i]) break;
                sharedPrefixBytes++;
            }

            return Item(curr.timestamp, currKey.substr(0, sharedPrefixBytes + 1));
        }
    }
};


}


using Negentropy = negentropy::Negentropy;
