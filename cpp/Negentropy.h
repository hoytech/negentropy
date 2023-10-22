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
#include <optional>
#include <bit>

#include <openssl/sha.h>


namespace negentropy {

const uint64_t PROTOCOL_VERSION_0 = 0x60;

const uint64_t MAX_U64 = std::numeric_limits<uint64_t>::max();
const size_t ID_SIZE = 32;
const size_t FINGERPRINT_SIZE = 16;
using err = std::runtime_error;




inline uint8_t getByte(std::string_view &encoded) {
    if (encoded.size() < 1) throw negentropy::err("parse ends prematurely");
    uint8_t output = encoded[0];
    encoded = encoded.substr(1);
    return output;
}

inline std::string getBytes(std::string_view &encoded, size_t n) {
    if (encoded.size() < n) throw negentropy::err("parse ends prematurely");
    auto res = encoded.substr(0, n);
    encoded = encoded.substr(n);
    return std::string(res);
};

inline uint64_t decodeVarInt(std::string_view &encoded) {
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

inline std::string encodeVarInt(uint64_t n) {
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




enum class Mode {
    Skip = 0,
    Fingerprint = 1,
    IdList = 2,
    Continuation = 3,
    UnsupportedProtocolVersion = 4,
};


struct Item {
    uint64_t timestamp;
    char id[ID_SIZE];

    explicit Item(uint64_t timestamp = 0) : timestamp(timestamp) {
        memset(id, '\0', sizeof(id));
    }

    explicit Item(uint64_t timestamp, std::string_view id_) : timestamp(timestamp) {
        if (id_.size() != sizeof(id)) throw negentropy::err("bad id size for Item");
        memcpy(id, id_.data(), sizeof(id));
    }

    std::string_view getId() const {
        return std::string_view(id, sizeof(id));
    }

    bool operator==(const Item &other) const {
        return timestamp == other.timestamp && getId() == other.getId();
    }
};

inline bool operator<(const Item &a, const Item &b) {
    return a.timestamp != b.timestamp ? a.timestamp < b.timestamp : a.getId() < b.getId();
};


struct Bound {
    Item item;
    size_t idLen;

    explicit Bound(uint64_t timestamp = 0, std::string_view id = "") : item(timestamp), idLen(id.size()) {
        if (idLen > ID_SIZE) throw negentropy::err("bad id size for Bound");
        memcpy(item.id, id.data(), idLen);
    }

    explicit Bound(const Item &item_) : item(item_), idLen(ID_SIZE) {}

    bool operator==(const Bound &other) const {
        return item == other.item;
    }
};

inline bool operator<(const Bound &a, const Bound &b) {
    return a.item < b.item;
};


struct Fingerprint {
    uint8_t buf[FINGERPRINT_SIZE];

    std::string_view sv() const {
        return std::string_view(reinterpret_cast<const char*>(buf), sizeof(buf));
    }
};

struct Accumulator {
    char buf[ID_SIZE];

    void setToZero() {
        memset(buf, '\0', sizeof(buf));
    }

    void add(const Item &item) {
        add(item.id);
    }

    void add(const Accumulator &acc) {
        add(acc.buf);
    }

    void add(const char *otherBuf) {
        uint64_t currCarry = 0, nextCarry = 0;
        uint64_t *p = reinterpret_cast<uint64_t*>(buf);
        const uint64_t *po = reinterpret_cast<const uint64_t*>(otherBuf);

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
        for (size_t i = 0; i < sizeof(buf); i++) {
            buf[i] = ~buf[i];
        }

        Accumulator zero;
        zero.setToZero();
        zero.buf[0] = 1;
        add(zero.buf);
    }

    std::string_view sv() const {
        return std::string_view(reinterpret_cast<const char*>(buf), sizeof(buf));
    }

    Fingerprint getFingerprint(uint64_t n) {
        std::string input;
        input += sv();
        input += encodeVarInt(n);

        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256(reinterpret_cast<unsigned char*>(input.data()), input.size(), hash);

        Fingerprint out;
        memcpy(out.buf, hash, FINGERPRINT_SIZE);

        return out;
    }
};


struct NegentropyStorageBase {
    virtual uint64_t size() = 0;

    virtual const Item &getItem(size_t i) = 0;

    virtual void iterate(size_t begin, size_t end, std::function<void(const Item &)> cb) = 0;

    virtual size_t upperBound(const Bound &value) = 0;

    virtual Fingerprint fingerprint(size_t begin, size_t end) = 0;
};



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

    size_t upperBound(const Bound &bound) {
        checkSealed();
        return std::upper_bound(items.begin(), items.end(), bound.item) - items.begin();
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




struct Negentropy {
    uint64_t frameSizeLimit;

    struct OutputRange {
        Bound start;
        Bound end;
        std::string payload;
    };

    NegentropyStorageBase *storage = nullptr;
    bool isInitiator = false;
    bool didHandshake = false;
    bool continuationNeeded = false;
    std::deque<OutputRange> pendingOutputs;

    Negentropy(uint64_t frameSizeLimit = 0) : frameSizeLimit(frameSizeLimit) {
        if (frameSizeLimit != 0 && frameSizeLimit < 4096) throw negentropy::err("frameSizeLimit too small");
    }

    void setStorage(NegentropyStorageBase *storage_) {
        if (storage) throw negentropy::err("storage already set");
        storage = storage_;
    }

    std::string initiate() {
        if (!storage) throw negentropy::err("storage not installed");
        if (didHandshake) throw negentropy::err("can't initiate after reconcile");
        isInitiator = true;

        splitRange(0, storage->size(), Bound(0), Bound(MAX_U64), pendingOutputs);

        auto output = std::move(buildOutput(true).value());
        return output;
    }

    std::string reconcile(std::string_view query) {
        if (isInitiator) throw negentropy::err("initiator not asking for have/need IDs");

        if (!didHandshake) {
            auto protocolVersion = getByte(query);
            if (protocolVersion < 0x60 || protocolVersion > 0x6F) throw negentropy::err("invalid negentropy protocol version byte");
            if (protocolVersion != PROTOCOL_VERSION_0) {
                std::string o;
                uint64_t lastTimestampOut = 0;
                o += encodeBound(Bound(PROTOCOL_VERSION_0), lastTimestampOut);
                o += encodeVarInt(uint64_t(Mode::UnsupportedProtocolVersion));
                return o;
            }
            didHandshake = true;
        }

        std::vector<std::string> haveIds, needIds;
        reconcileAux(query, haveIds, needIds);

        auto output = std::move(buildOutput(false).value());
        return output;
    }

    std::optional<std::string> reconcile(std::string_view query, std::vector<std::string> &haveIds, std::vector<std::string> &needIds) {
        if (!isInitiator) throw negentropy::err("non-initiator asking for have/need IDs");

        reconcileAux(query, haveIds, needIds);

        return buildOutput(false);
    }

  private:
    void reconcileAux(std::string_view query, std::vector<std::string> &haveIds, std::vector<std::string> &needIds) {
        if (!storage) throw negentropy::err("storage not installed");
        continuationNeeded = false;

        Bound prevBound;
        size_t prevIndex = 0;
        uint64_t lastTimestampIn = 0;
        std::deque<OutputRange> outputs;

        while (query.size()) {
            auto currBound = decodeBound(query, lastTimestampIn);
            auto mode = Mode(decodeVarInt(query));

            auto lower = prevIndex;
            auto upper = storage->upperBound(currBound);

            if (mode == Mode::Skip) {
                // Do nothing
            } else if (mode == Mode::Fingerprint) {
                auto theirFingerprint = getBytes(query, FINGERPRINT_SIZE);
                auto ourFingerprint = storage->fingerprint(lower, upper);

                if (theirFingerprint != ourFingerprint.sv()) {
                    splitRange(lower, upper, prevBound, currBound, outputs);
                }
            } else if (mode == Mode::IdList) {
                auto numIds = decodeVarInt(query);

                std::unordered_set<std::string> theirElems;
                for (uint64_t i = 0; i < numIds; i++) {
                    auto e = getBytes(query, ID_SIZE);
                    theirElems.insert(e);
                }

                storage->iterate(lower, upper, [&](const Item &item){
                    auto k = std::string(item.getId());

                    if (theirElems.find(k) == theirElems.end()) {
                        // ID exists on our side, but not their side
                        if (isInitiator) haveIds.emplace_back(k);
                    } else {
                        // ID exists on both sides
                        theirElems.erase(k);
                    }
                });

                if (isInitiator) {
                    for (const auto &k : theirElems) {
                        // ID exists on their side, but not our side
                        needIds.emplace_back(k);
                    }
                } else {
                    std::vector<std::string> responseHaveIds;

                    auto it = lower;
                    bool didSplit = false;
                    Bound splitBound;

                    auto flushIdListOutput = [&]{
                        std::string payload = encodeVarInt(uint64_t(Mode::IdList));

                        payload += encodeVarInt(responseHaveIds.size());
                        for (const auto &id : responseHaveIds) payload += id;

                        auto nextSplitBound = it + 1 >= upper ? currBound : getMinimalBound(storage->getItem(it), storage->getItem(it + 1));

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
                        responseHaveIds.emplace_back(storage->getItem(it).getId());
                        if (responseHaveIds.size() >= 100) flushIdListOutput(); // 100*32 is less than minimum frame size limit of 4k
                    }

                    flushIdListOutput();
                }
            } else if (mode == Mode::Continuation) {
                continuationNeeded = true;
            } else if (mode == Mode::UnsupportedProtocolVersion) {
                throw negentropy::err("server does not support our negentropy protocol version");
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

    void splitRange(size_t lower, size_t upper, const Bound &lowerBound, const Bound &upperBound, std::deque<OutputRange> &outputs) {
        uint64_t numElems = upper - lower;
        const uint64_t buckets = 16;

        if (numElems < buckets * 2) {
            std::string payload = encodeVarInt(uint64_t(Mode::IdList));
            payload += encodeVarInt(numElems);
            storage->iterate(lower, upper, [&](const Item &item){
                payload += item.getId();
            });

            outputs.emplace_back(OutputRange({
                lowerBound,
                upperBound,
                std::move(payload)
            }));
        } else {
            uint64_t itemsPerBucket = numElems / buckets;
            uint64_t bucketsWithExtra = numElems % buckets;
            auto curr = lower;
            auto prevBound = Bound(storage->getItem(curr));

            for (uint64_t i = 0; i < buckets; i++) {
                auto bucketSize = itemsPerBucket + (i < bucketsWithExtra ? 1 : 0);
                auto ourFingerprint = storage->fingerprint(curr, curr + bucketSize);
                curr += bucketSize;

                std::string payload = encodeVarInt(uint64_t(Mode::Fingerprint));
                payload += ourFingerprint.sv();

                outputs.emplace_back(OutputRange({
                    i == 0 ? lowerBound : prevBound,
                    curr == upper ? upperBound : getMinimalBound(storage->getItem(curr - 1), storage->getItem(curr)),
                    std::move(payload)
                }));

                prevBound = outputs.back().end;
            }

            outputs.back().end = upperBound;
        }
    }

    std::optional<std::string> buildOutput(bool initialMessage) {
        std::string output;
        Bound currBound;
        uint64_t lastTimestampOut = 0;

        if (initialMessage) {
            if (didHandshake) throw negentropy::err("already built initial message");
            didHandshake = true;
            output.push_back(PROTOCOL_VERSION_0);
        }

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

        if (!isInitiator && pendingOutputs.size()) {
            output += encodeBound(Bound(MAX_U64), lastTimestampOut);
            output += encodeVarInt(uint64_t(Mode::Continuation));
        }

        if (isInitiator && output.size() == 0 && !continuationNeeded) {
            return std::nullopt;
        }

        return output;
    }


    // Decoding


    uint64_t decodeTimestampIn(std::string_view &encoded, uint64_t &lastTimestampIn) {
        uint64_t timestamp = decodeVarInt(encoded);
        timestamp = timestamp == 0 ? MAX_U64 : timestamp - 1;
        timestamp += lastTimestampIn;
        if (timestamp < lastTimestampIn) timestamp = MAX_U64; // saturate
        lastTimestampIn = timestamp;
        return timestamp;
    }

    Bound decodeBound(std::string_view &encoded, uint64_t &lastTimestampIn) {
        auto timestamp = decodeTimestampIn(encoded, lastTimestampIn);
        auto len = decodeVarInt(encoded);
        return Bound(timestamp, getBytes(encoded, len));
    }


    // Encoding

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

    std::string encodeBound(const Bound &bound, uint64_t &lastTimestampOut) {
        std::string output;

        output += encodeTimestampOut(bound.item.timestamp, lastTimestampOut);
        output += encodeVarInt(bound.idLen);
        output += bound.item.getId().substr(0, bound.idLen);

        return output;
    };

    Bound getMinimalBound(const Item &prev, const Item &curr) {
        if (curr.timestamp != prev.timestamp) {
            return Bound(curr.timestamp, "");
        } else {
            uint64_t sharedPrefixBytes = 0;
            auto currKey = curr.getId();
            auto prevKey = prev.getId();

            for (uint64_t i = 0; i < ID_SIZE; i++) {
                if (currKey[i] != prevKey[i]) break;
                sharedPrefixBytes++;
            }

            return Bound(curr.timestamp, currKey.substr(0, sharedPrefixBytes + 1));
        }
    }
};


}


using Negentropy = negentropy::Negentropy;
using NegentropyStorageVector = negentropy::NegentropyStorageVector;
