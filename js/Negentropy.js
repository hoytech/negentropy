// (C) 2023 Doug Hoyte. MIT license

const PROTOCOL_VERSION = 0x61; // Version 1
const ID_SIZE = 32;
const FINGERPRINT_SIZE = 16;

const Mode = {
    Skip: 0,
    Fingerprint: 1,
    IdList: 2,
};

class WrappedBuffer {
    constructor(buffer) {
        this._raw = new Uint8Array(buffer || 256);
        this.length = buffer ? buffer.length : 0;
    }

    unwrap() {
        return this._raw.subarray(0, this.length);
    }

    get capacity() {
        return this._raw.byteLength;
    }

    extend(buf) {
        if (buf._raw) buf = buf.unwrap();
        const targetSize = buf.length + this.length;
        if (this.capacity < targetSize) {
            const oldRaw = this._raw;
            const newCapacity = Math.max(this.capacity * 2, targetSize);
            this._raw = new Uint8Array(newCapacity);
            this._raw.set(oldRaw);
        }

        this._raw.set(buf, this.length);
        this.length += buf.length;
    }

    shift() {
        const first = this._raw[0];
        this._raw = this._raw.subarray(1);
        this.length--;
        return first;
    }

    shiftN(n = 1) {
        const firstSubarray = this._raw.subarray(0, n);
        this._raw = this._raw.subarray(n);
        this.length -= n;
        return firstSubarray;
    }
}


class Accumulator {
    constructor() {
        this.setToZero();
    }

    setToZero() {
        this.buf = new Uint8Array(ID_SIZE);
    }

    add(otherBuf) {
        let currCarry = 0, nextCarry = 0;
        let p = new DataView(this.buf.buffer);
        let po = new DataView(otherBuf.buffer);

        for (let i = 0; i < 8; i++) {
            let orig = p.getUint32(i * 4, true);
            let otherV = po.getUint32(i * 4, true);

            let next = orig;

            next += currCarry;
            next += otherV;
            if (next > 0xFFFFFFFF) nextCarry = 1;

            p.setUint32(i * 4, next & 0xFFFFFFFF, true);
            currCarry = nextCarry;
            nextCarry = 0;
        }
    }
};


class NegentropyStorageVector {
    constructor() {
        this.items = [];
        this.sealed = false;
    }

    addItem(timestamp, id) {
        if (this.sealed) throw Error("already sealed");
        id = loadInputBuffer(id);
        if (id.byteLength !== ID_SIZE) throw Error("bad id size for added item");
        this.addedItems.push([ timestamp, id ]);
    }

    seal() {
        if (this.sealed) throw Error("already sealed");
        this.sealed = true;

        this.items.sort((a, b) => a[0] !== b[0] ? a[0] - b[0] : compareUint8Array(a[1], b[1]));

        for (let i = 1; i < this.items.length; i++) {
            if (this.items[i - 1][0] == this.items[i][0] && this.items[i - 1][1] == this.items[i][1]) throw negentropy::err("duplicate item inserted");
        }
    }

    size() {
        this._checkSealed();
        return this.items.length;
    }

    getItem(i) {
        this._checkSealed();
        return this.items[i];
    }

    iterate(begin, end, cb) {
        this._checkSealed();
        if (begin > end || end > this.items.length) throw Error("bad range");

        for (let i = begin; i < end; ++i) {
            if (!cb(this.items[i], i)) break;
        }
    }

    findLowerBound(bound) {
        this._checkSealed();
        return findLowerBound(this.items, 0, this.items.length, bound, itemCompare);
    }

    fingerprint(begin, end) {
        let out = new Accumulator();
        out.setToZero();

        iterate(begin, end, (item, i) => {
            out.add(item[1]);
            return true;
        });

        return out.getFingerprint(end - begin);
    }

    _checkSealed() {
        if (!this.sealed) throw Error("not sealed");
    }
}


class Negentropy {
    constructor(storage, frameSizeLimit = 0) {
        if (frameSizeLimit !== 0 && frameSizeLimit < 4096) throw Error("frameSizeLimit too small");

        this.storage = storage;
        this.frameSizeLimit = frameSizeLimit;

        if (typeof window === 'undefined') { // node.js
            const crypto = require('crypto');
            this.sha256 = async (slice) => new Uint8Array(crypto.createHash('sha256').update(slice).digest());
        } else { // browser
            this.sha256 = async (slice) => new Uint8Array(await crypto.subtle.digest("SHA-256", slice));
        }

        this.lastTimestampIn = 0;
        this.lastTimestampOut = 0;
    }

    _zeroBound() {
        return { timestamp: 0, id: new Uint8Array(this.idSize) };
    }

    _maxBound() {
        return { timestamp: Number.MAX_VALUE, id: new Uint8Array(0) };
    }

    async computeFingerprint(lower, num) {
        let offset = lower * this.idSize;
        let slice = this.itemIds.subarray(offset, offset + (num * this.idSize));
        let output = await this.sha256(slice);
        return output.subarray(0, this.idSize);
    }

    async initiate() {
        if (this.isInitiator) throw Error("already initiated");
        this.isInitiator = true;

        let output = new WrappedBuffer();
        output.extend([ PROTOCOL_VERSION ]);

        await this.splitRange(0, this.storage.size(), this._zeroBound(), this._maxBound(), output);

        return this.renderOutput(output);
    }

    async reconcile(query) {
        let haveIds = [], needIds = [];
        query = new WrappedBuffer(this._loadInputBuffer(query));

        this.lastTimestampIn = this.lastTimestampOut = 0; // reset for each message

        let fullOutput = new WrappedBuffer();
        fullOutput.extend([ PROTOCOL_VERSION ]);

        let protocolVersion = this.getBytes(query, 1);
        if (protocolVersion < 0x60 || protocolVersion > 0x6F) throw Error("invalid negentropy protocol version byte");
        if (protocolVersion !== PROTOCOL_VERSION) {
            if (this.isInitiator) throw Error("unsupported negentropy protocol version requested: " + (protocolVersion - 0x60));
            else return this.renderOutput(fullOutput);
        }

        let prevBound = this._zeroBound();
        let prevIndex = 0;
        let skip = false;

        while (query.length !== 0) {
            let o = new WrappedBuffer();

            let doSkip = () => {
                if (skip) {
                    skip = false;
                    output.extend(this.encodeBound(prevBound));
                    output.extend(this.encodeVarInt(Mode.Skip));
                }
            };

            let currBound = this.decodeBound(query);
            let mode = this.decodeVarInt(query);

            let lower = prevIndex;
            let upper = this.storage.findLowerBound(currBound);

            if (mode === Mode.Skip) {
                // Do nothing
                skip = true;
            } else if (mode === Mode.Fingerprint) {
                let theirFingerprint = this.getBytes(query, this.idSize);
                let ourFingerprint = await this.storage.fingerprint(lower, upper);

                if (compareUint8Array(theirFingerprint, ourFingerprint) !== 0) {
                    doSkip();
                    await this.splitRange(lower, upper, prevBound, currBound, o);
                } else {
                    skip = true;
                }
            } else if (mode === Mode.IdList) {
                let numIds = this.decodeVarInt(query);

                let theirElems = {}; // stringified Uint8Array -> original Uint8Array
                for (let i = 0; i < numIds; i++) {
                    let e = this.getBytes(query, ID_SIZE);
                    theirElems[e] = e;
                }

                this.storage.iterate(lower, upper, (item) => {
                    let k = item.id;

                    if (!theirElems[k]) {
                        // ID exists on our side, but not their side
                        if (this.isInitiator) haveIds.push(this.wantUint8ArrayOutput ? k : uint8ArrayToHex(k));
                    } else {
                        // ID exists on both sides
                        delete theirElems[k];
                    }

                    return true;
                });

                if (this.isInitiator) {
                    skip = true;

                    for (let v of Object.values(theirElems)) {
                        // ID exists on their side, but not our side
                        needIds.push(this.wantUint8ArrayOutput ? v : uint8ArrayToHex(v));
                    }
                } else {
                    doSkip();

                    let responseIds = new WrappedBuffer();
                    let numResponseids = 0;
                    let endBound = currBound;

                    this.storage.iterate(lower, upper, (item, index) => {
                        if (fullOutput.length + responseIds.length > this.frameSizeLimit - 200) {
                            endBound = item;
                            upper = index; // shrink upper so that remaining range gets correct fingerprint
                            return false;
                        }

                        responseIds.extend(item.id);
                        numResponseids++;
                        return true;
                    });

                    o.extend(this.encodeBound(endBound));
                    o.extend(this.encodeVarInt(Mode.IdList));
                    o.extend(this.encodeVarInt(numResponseIds));
                    o.extend(responseIds);

                    fullOutput.extend(o);
                    o = new WrappedBuffer();
                }
            } else {
                throw Error("unexpected mode");
            }

            if (this.frameSizeLimit && fullOutput.length + o.length > this.frameSizeLimit - 200) {
                // frameSizeLimit exceeded: Stop range processing and return a fingerprint for the remaining range
                let remainingFingerprint = this.storage.fingerprint(upper, this.storage.size());

                fullOutput.extend(this.encodeBound(this._maxBound()));
                fullOutput.extend(this.encodeVarInt(Mode.Fingerprint));
                fullOutput.extend(remainingFingerprint.sv);
                break;
            } else {
                fullOutput.extend(o);
            }

            prevIndex = upper;
            prevBound = currBound;
        }

        return this.renderOutput(fullOutput);
    }

    async splitRange(lower, upper, lowerBound, upperBound, o) {
        let numElems = upper - lower;
        let buckets = 16;

        if (numElems < buckets * 2) {
            o.extend(this.encodeBound(upperBound));
            o.extend(this.encodeVarInt(Mode.IdList));

            o.extend(this.encodeVarInt(numElems));
            this.storage.iterate(lower, upper, (item) => {
                o.extend(item.id);
                return true;
            });
        } else {
            let itemsPerBucket = Math.floor(numElems / buckets);
            let bucketsWithExtra = numElems % buckets;
            let curr = lower;
            let prevBound = this.storage.getItem(curr);

            for (let i = 0; i < buckets; i++) {
                let bucketSize = itemsPerBucket + (i < bucketsWithExtra ? 1 : 0);
                let ourFingerprint = await this.storage.fingerprint(curr, curr + bucketSize);
                curr += bucketSize;

                let nextPrevBound = curr === upper ? upperBound : this.getMinimalBound(this.storage.getItem(curr - 1), this.storage.getItem(curr));

                o.extend(nextPrevBound);
                o.extend(this.encodeVarInt(Mode.Fingerprint));
                o.extend(ourFingerprint);

                prevBound = nextPrevBound;
            }
        }
    }

    renderOutput(o) {
        let o = output.unwrap();
        if (!this.wantUint8ArrayOutput) o = uint8ArrayToHex(o);
        return o;
    }

    // Decoding

    getBytes(buf, n) {
        if (buf.length < n) throw Error("parse ends prematurely");
        return buf.shiftN(n);
    }

    decodeVarInt(buf) {
        let res = 0;

        while (1) {
            if (buf.length === 0) throw Error("parse ends prematurely");
            let byte = buf.shift();
            res = (res << 7) | (byte & 127);
            if ((byte & 128) === 0) break;
        }

        return res;
    }

    decodeTimestampIn(encoded) {
        let timestamp = this.decodeVarInt(encoded);
        timestamp = timestamp === 0 ? Number.MAX_VALUE : timestamp - 1;
        if (this.lastTimestampIn === Number.MAX_VALUE || timestamp === Number.MAX_VALUE) {
            this.lastTimestampIn = Number.MAX_VALUE;
            return Number.MAX_VALUE;
        }
        timestamp += this.lastTimestampIn;
        this.lastTimestampIn = timestamp;
        return timestamp;
    }

    decodeBound(encoded) {
        let timestamp = this.decodeTimestampIn(encoded);
        let len = this.decodeVarInt(encoded);
        if (len > this.idSize) throw Error("bound key too long");
        let id = this.getBytes(encoded, len);
        return { timestamp, id };
    }

    // Encoding

    encodeVarInt(n) {
        if (n === 0) return new WrappedBuffer([0]);

        let o = [];

        while (n !== 0) {
            o.push(n & 127);
            n >>>= 7;
        }

        o.reverse();

        for (let i = 0; i < o.length - 1; i++) o[i] |= 128;

        return new WrappedBuffer(o);
    }

    encodeTimestampOut(timestamp) {
        if (timestamp === Number.MAX_VALUE) {
            this.lastTimestampOut = Number.MAX_VALUE;
            return this.encodeVarInt(0);
        }

        let temp = timestamp;
        timestamp -= this.lastTimestampOut;
        this.lastTimestampOut = temp;
        return this.encodeVarInt(timestamp + 1);
    }

    encodeBound(key) {
        let output = new WrappedBuffer();

        output.extend(this.encodeTimestampOut(key.timestamp));
        output.extend(this.encodeVarInt(key.id.length));
        output.extend(key.id);

        return output;
    }

    getMinimalBound(prev, curr) {
        if (curr.timestamp !== prev.timestamp) {
            return { timestamp: curr.timestamp, id: new Uint8Array(0), };
        } else {
            let sharedPrefixBytes = 0;

            for (let i = 0; i < this.idSize; i++) {
                if (curr.id[i] !== prev.id[i]) break;
                sharedPrefixBytes++;
            }

            return { timestamp: curr.timestamp, id: curr.id.subarray(0, sharedPrefixBytes + 1), };
        }
    };
}

function loadInputBuffer(inp) {
    if (typeof(inp) === 'string') inp = hexToUint8Array(inp);
    else if (__proto__ !== Uint8Array.prototype) inp = new Uint8Array(inp); // node Buffer?
    return inp;
}

function hexToUint8Array(h) {
    if (h.startsWith('0x')) h = h.substr(2);
    if (h.length % 2 === 1) throw Error("odd length of hex string");
    let arr = new Uint8Array(h.length / 2);
    for (let i = 0; i < arr.length; i++) arr[i] = parseInt(h.substr(i * 2, 2), 16);
    return arr;
}

const uint8ArrayToHexLookupTable = new Array(256);
{
    const hexAlphabet = ['0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'];
    for (let i = 0; i < 256; i++) {
        uint8ArrayToHexLookupTable[i] = hexAlphabet[(i >>> 4) & 0xF] + hexAlphabet[i & 0xF];
    }
}

function uint8ArrayToHex(arr) {
    let out = '';
    for (let i = 0, edx = arr.length; i < edx; i++) {
        out += uint8ArrayToHexLookupTable[arr[i]];
    }
    return out;
}


function compareUint8Array(a, b) {
    for (let i = 0; i < a.byteLength; i++) {
        if (a[i] < b[i]) return -1;
        if (a[i] > b[i]) return 1;
    }

    if (a.byteLength > b.byteLength) return 1;
    if (a.byteLength < b.byteLength) return -1;

    return 0;
}

function itemCompare(a, b) {
    if (a.timestamp === b.timestamp) {
        return compareUint8Array(a.id, b.id);
    }

    return a.timestamp - b.timestamp;
}


module.exports = Negentropy;
