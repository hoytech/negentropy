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

#include "negentropy/encoding.h"
#include "negentropy/types.h"


namespace negentropy {

const uint64_t PROTOCOL_VERSION = 0x61; // Version 1

const uint64_t MAX_U64 = std::numeric_limits<uint64_t>::max();
using err = std::runtime_error;



struct Negentropy {
    uint64_t frameSizeLimit;
    NegentropyStorageBase *storage = nullptr;

    bool isInitiator = false;

    uint64_t lastTimestampIn = 0;
    uint64_t lastTimestampOut = 0;

    Negentropy(uint64_t frameSizeLimit = 0) : frameSizeLimit(frameSizeLimit) {
        if (frameSizeLimit != 0 && frameSizeLimit < 4096) throw negentropy::err("frameSizeLimit too small");
    }

    void setStorage(NegentropyStorageBase *storage_) {
        if (storage) throw negentropy::err("storage already set");
        storage = storage_;
    }

    std::string initiate() {
        if (!storage) throw negentropy::err("storage not installed");
        if (isInitiator) throw negentropy::err("already initiated");
        isInitiator = true;

        std::string output;
        output.push_back(PROTOCOL_VERSION);

        output += splitRange(0, storage->size(), Bound(0), Bound(MAX_U64));

        return output;
    }

    std::string reconcile(std::string_view query) {
        if (isInitiator) throw negentropy::err("initiator not asking for have/need IDs");

        std::vector<std::string> haveIds, needIds;
        return reconcileAux(query, haveIds, needIds);
    }

    std::optional<std::string> reconcile(std::string_view query, std::vector<std::string> &haveIds, std::vector<std::string> &needIds) {
        if (!isInitiator) throw negentropy::err("non-initiator asking for have/need IDs");

        auto output = reconcileAux(query, haveIds, needIds);
        if (output.size() == 1) return std::nullopt;
        return output;
    }

  private:
    std::string reconcileAux(std::string_view query, std::vector<std::string> &haveIds, std::vector<std::string> &needIds) {
        if (!storage) throw negentropy::err("storage not installed");
        lastTimestampIn = lastTimestampOut = 0; // reset for each message

        std::string fullOutput;
        fullOutput.push_back(PROTOCOL_VERSION);

        auto protocolVersion = getByte(query);
        if (protocolVersion < 0x60 || protocolVersion > 0x6F) throw negentropy::err("invalid negentropy protocol version byte");
        if (protocolVersion != PROTOCOL_VERSION) {
            if (isInitiator) throw negentropy::err(std::string("unsupported negentropy protocol version requested") + std::to_string(protocolVersion - 0x60));
            else return fullOutput;
        }

        Bound prevBound;
        size_t prevIndex = 0;
        bool skip = false;

        while (query.size()) {
            std::string o;

            auto doSkip = [&]{
                if (!skip) return;
                skip = false;
                o += encodeBound(prevBound);
                o += encodeVarInt(uint64_t(Mode::Skip));
            };

            auto currBound = decodeBound(query, lastTimestampIn);
            auto mode = Mode(decodeVarInt(query));

            auto lower = prevIndex;
            auto upper = storage->findLowerBound(currBound);

            if (mode == Mode::Skip) {
                skip = true;
            } else if (mode == Mode::Fingerprint) {
                auto theirFingerprint = getBytes(query, FINGERPRINT_SIZE);
                auto ourFingerprint = storage->fingerprint(lower, upper);

                if (theirFingerprint != ourFingerprint.sv()) {
                    doSkip();
                    o += splitRange(lower, upper, prevBound, currBound);
                } else {
                    skip = true;
                }
            } else if (mode == Mode::IdList) {
                auto numIds = decodeVarInt(query);

                std::unordered_set<std::string> theirElems;
                for (uint64_t i = 0; i < numIds; i++) {
                    auto e = getBytes(query, ID_SIZE);
                    theirElems.insert(e);
                }

                storage->iterate(lower, upper, [&](const Item &item, size_t){
                    auto k = std::string(item.getId());

                    if (theirElems.find(k) == theirElems.end()) {
                        // ID exists on our side, but not their side
                        if (isInitiator) haveIds.emplace_back(k);
                    } else {
                        // ID exists on both sides
                        theirElems.erase(k);
                    }

                    return true;
                });

                if (isInitiator) {
                    skip = true;

                    for (const auto &k : theirElems) {
                        // ID exists on their side, but not our side
                        needIds.emplace_back(k);
                    }
                } else {
                    doSkip();

                    std::string responseIds;
                    uint64_t numResponseIds = 0;
                    Bound endBound = currBound;

                    storage->iterate(lower, upper, [&](const Item &item, size_t it){
                        if (fullOutput.size() + responseIds.size() > frameSizeLimit - 200) {
                            endBound = Bound(item);
                            upper = it; // shrink upper so that remaining range has correct fingerprint
                            return false;
                        }

                        responseIds += item.getId();
                        numResponseIds++;
                        return true;
                    });

                    o += encodeBound(endBound);
                    o += encodeVarInt(uint64_t(Mode::IdList));
                    o += encodeVarInt(numResponseIds);
                    o += responseIds;

                    fullOutput += o;
                    o.clear();
                }
            } else {
                throw negentropy::err("unexpected mode");
            }

            if (frameSizeLimit && fullOutput.size() + o.size() > frameSizeLimit - 200) {
                // FSL exceeded: Stop range processing and return a fingerprint for the remaining range
                auto remainingFingerprint = storage->fingerprint(upper, storage->size());

                fullOutput += encodeBound(Bound(MAX_U64));
                fullOutput += encodeVarInt(uint64_t(Mode::Fingerprint));
                fullOutput += remainingFingerprint.sv();
                break;
            } else {
                fullOutput += o;
            }

            prevIndex = upper;
            prevBound = currBound;
        }

        return fullOutput;
    }

    std::string splitRange(size_t lower, size_t upper, const Bound &lowerBound, const Bound &upperBound) {
        std::string o;

        uint64_t numElems = upper - lower;
        const uint64_t buckets = 16;

        if (numElems < buckets * 2) {
            o += encodeBound(upperBound);
            o += encodeVarInt(uint64_t(Mode::IdList));

            o += encodeVarInt(numElems);
            storage->iterate(lower, upper, [&](const Item &item, size_t){
                o += item.getId();
                return true;
            });
        } else {
            uint64_t itemsPerBucket = numElems / buckets;
            uint64_t bucketsWithExtra = numElems % buckets;
            auto curr = lower;
            auto prevBound = Bound(storage->getItem(curr));

            for (uint64_t i = 0; i < buckets; i++) {
                auto bucketSize = itemsPerBucket + (i < bucketsWithExtra ? 1 : 0);
                auto ourFingerprint = storage->fingerprint(curr, curr + bucketSize);
                curr += bucketSize;

                auto nextPrevBound = curr == upper ? upperBound : getMinimalBound(storage->getItem(curr - 1), storage->getItem(curr));

                o += encodeBound(nextPrevBound);
                o += encodeVarInt(uint64_t(Mode::Fingerprint));
                o += ourFingerprint.sv();

                prevBound = nextPrevBound;
            }
        }

        return o;
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

    std::string encodeTimestampOut(uint64_t timestamp) {
        if (timestamp == MAX_U64) {
            lastTimestampOut = MAX_U64;
            return encodeVarInt(0);
        }

        uint64_t temp = timestamp;
        timestamp -= lastTimestampOut;
        lastTimestampOut = temp;
        return encodeVarInt(timestamp + 1);
    };

    std::string encodeBound(const Bound &bound) {
        std::string output;

        output += encodeTimestampOut(bound.item.timestamp);
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
