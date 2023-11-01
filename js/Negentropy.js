// (C) 2023 Doug Hoyte. MIT license

const PROTOCOL_VERSION_0 = 0x60;
const ID_SIZE = 32;
const FINGERPRINT_SIZE = 16;

const Mode = {
    Skip: 0,
    Fingerprint: 1,
    IdList: 2,
    Continuation: 3,
    UnsupportedProtocolVersion: 4,
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
        throw Error("not impl");
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
    constructor(idSize = 16, frameSizeLimit = 0) {
        if (idSize < 8 || idSize > 32) throw Error("idSize invalid");
        if (frameSizeLimit !== 0 && frameSizeLimit < 4096) throw Error("frameSizeLimit too small");

        this.idSize = idSize;
        this.frameSizeLimit = frameSizeLimit;

        this.addedItems = [];
        this.pendingOutputs = [];

        if (typeof window === 'undefined') { // node.js
            const crypto = require('crypto');
            this.sha256 = async (slice) => new Uint8Array(crypto.createHash('sha256').update(slice).digest());
        } else { // browser
            this.sha256 = async (slice) => new Uint8Array(await crypto.subtle.digest("SHA-256", slice));
        }
    }

    addItem(timestamp, id) {
        if (this.sealed) throw Error("already sealed");
        id = this._loadInputBuffer(id);

        if (id.byteLength > 64 || id.byteLength < this.idSize) throw Error("bad length for id");
        if (id.byteLength > this.idSize) id = id.subarray(0, this.idSize);

        this.addedItems.push([ timestamp, id ]);
    }

    seal() {
        if (this.sealed) throw Error("already sealed");
        this.sealed = true;

        this.addedItems.sort((a, b) => a[0] !== b[0] ? a[0] - b[0] : compareUint8Array(a[1], b[1]));

        if (this.addedItems.length > 1) {
            for (let i = 0; i < this.addedItems.length - 1; i++) {
                if (this.addedItems[i][0] == this.addedItems[i + 1][0] &&
                    compareUint8Array(this.addedItems[i][1], this.addedItems[i + 1][1]) === 0) throw Error("duplicate item inserted");
            }
        }

        this.itemTimestamps = new BigUint64Array(this.addedItems.length);
        this.itemIds = new Uint8Array(this.addedItems.length * this.idSize);

        for (let i = 0; i < this.addedItems.length; i++) {
            let item = this.addedItems[i];
            this.itemTimestamps[i] = BigInt(item[0]);
            this.itemIds.set(item[1], i * this.idSize);
        }

        delete this.addedItems;
    }

    _newState() {
        return {
            lastTimestampIn: 0,
            lastTimestampOut: 0,
        };
    }

    _zeroBound() {
        return { timestamp: 0, id: new Uint8Array(this.idSize) };
    }

    _maxBound() {
        return { timestamp: Number.MAX_VALUE, id: new Uint8Array(0) };
    }

    numItems() {
        return this.itemTimestamps.length;
    }

    getItemTimestamp(i) {
        return Number(this.itemTimestamps[i]);
    }

    getItemId(i) {
        let offset = i * this.idSize;
        return this.itemIds.subarray(offset, offset + this.idSize);
    }

    getItem(i) {
        return { timestamp: this.getItemTimestamp(i), id: this.getItemId(i), };
    }

    async computeFingerprint(lower, num) {
        let offset = lower * this.idSize;
        let slice = this.itemIds.subarray(offset, offset + (num * this.idSize));
        let output = await this.sha256(slice);
        return output.subarray(0, this.idSize);
    }

    async initiate() {
        if (!this.sealed) throw Error("not sealed");
        this.isInitiator = true;
        if (this.didHandshake) throw Error("can't initiate after reconcile");

        await this.splitRange(0, this.numItems(), this._zeroBound(), this._maxBound(), this.pendingOutputs);

        return this.buildOutput(true);
    }

    async reconcile(query) {
        query = new WrappedBuffer(this._loadInputBuffer(query));
        let haveIds = [], needIds = [];

        if (!this.sealed) throw Error("not sealed");
        this.continuationNeeded = false;

        let prevBound = this._zeroBound();
        let prevIndex = 0;
        let state = this._newState();
        let outputs = [];

        if (!this.isInitiator && !this.didHandshake) {
            let protocolVersion = this.getBytes(query, 1)[0];
            if (protocolVersion < 0x60 || protocolVersion > 0x6F) throw Error("invalid negentropy protocol version byte");
            if (protocolVersion !== PROTOCOL_VERSION_0) {
                let o = new WrappedBuffer();
                o.extend(this.encodeBound({ timestamp: PROTOCOL_VERSION_0, id: new Uint8Array([]), }, state));
                o.extend(this.encodeVarInt(Mode.UnsupportedProtocolVersion));
                let ret = o.unwrap();
                if (!this.wantUint8ArrayOutput) ret = uint8ArrayToHex(ret);
                return [ret, haveIds, needIds];
            }
            this.didHandshake = true;
        }

        while (query.length !== 0) {
            let currBound = this.decodeBound(query, state);
            let mode = this.decodeVarInt(query);

            let lower = prevIndex;
            let upper = this.findUpperBound(lower, this.numItems(), currBound);

            if (mode === Mode.Skip) {
                // Do nothing
            } else if (mode === Mode.Fingerprint) {
                let theirFingerprint = this.getBytes(query, this.idSize);
                let ourFingerprint = await this.computeFingerprint(lower, upper - lower);

                if (compareUint8Array(theirFingerprint, ourFingerprint) !== 0) {
                    await this.splitRange(lower, upper, prevBound, currBound, outputs);
                }
            } else if (mode === Mode.IdList) {
                let numIds = this.decodeVarInt(query);

                let theirElems = {}; // stringified Uint8Array -> original Uint8Array
                for (let i = 0; i < numIds; i++) {
                    let e = this.getBytes(query, this.idSize);
                    theirElems[e] = e;
                }

                for (let i = lower; i < upper; i++) {
                    let k = this.getItemId(i);

                    if (!theirElems[k]) {
                        // ID exists on our side, but not their side
                        if (this.isInitiator) haveIds.push(this.wantUint8ArrayOutput ? k : uint8ArrayToHex(k));
                    } else {
                        // ID exists on both sides
                        delete theirElems[k];
                    }
                }

                if (this.isInitiator) {
                    for (let v of Object.values(theirElems)) {
                        needIds.push(this.wantUint8ArrayOutput ? v : uint8ArrayToHex(v));
                    }
                } else {
                    let responseHaveIds = [];

                    let it = lower;
                    let didSplit = false;
                    let splitBound = this._zeroBound();

                    let flushIdListOutput = () => {
                        let payload = this.encodeVarInt(Mode.IdList);

                        payload.extend(this.encodeVarInt(responseHaveIds.length));
                        for (let id of responseHaveIds) payload.extend(id);

                        let nextSplitBound = (it+1) >= upper ? currBound : this.getMinimalBound(this.getItem(it), this.getItem(it+1));

                        outputs.push({
                            start: didSplit ? splitBound : prevBound,
                            end: nextSplitBound,
                            payload: payload,
                        });

                        splitBound = nextSplitBound;
                        didSplit = true;

                        responseHaveIds = [];
                    };

                    for (; it < upper; ++it) {
                        responseHaveIds.push(this.getItemId(it));
                        if (responseHaveIds.length >= 100) flushIdListOutput(); // 100*32 is less than minimum frame size limit of 4k
                    }

                    flushIdListOutput();
                }
            } else if (mode === Mode.Continuation) {
                this.continuationNeeded = true;
            } else if (mode === Mode.UnsupportedProtocolVersion) {
                throw Error("server does not support our negentropy protocol version");
            } else {
                throw Error("unexpected mode");
            }

            prevIndex = upper;
            prevBound = currBound;
        }

        while (outputs.length) {
            this.pendingOutputs.unshift(outputs.pop());
        }

        return [this.buildOutput(false), haveIds, needIds];
    }

    async splitRange(lower, upper, lowerBound, upperBound, outputs) {
        let numElems = upper - lower;
        let buckets = 16;

        if (numElems < buckets * 2) {
            let payload = this.encodeVarInt(Mode.IdList);
            payload.extend(this.encodeVarInt(numElems));
            for (let it = lower; it < upper; ++it) payload.extend(this.getItemId(it));

            outputs.push({
                start: lowerBound,
                end: upperBound,
                payload: payload,
            });
        } else {
            let itemsPerBucket = Math.floor(numElems / buckets);
            let bucketsWithExtra = numElems % buckets;
            let curr = lower;
            let prevBound = this.getItem(curr);

            for (let i = 0; i < buckets; i++) {
                let bucketSize = itemsPerBucket + (i < bucketsWithExtra ? 1 : 0);
                let ourFingerprint = await this.computeFingerprint(curr, bucketSize);
                curr += bucketSize;

                let payload = this.encodeVarInt(Mode.Fingerprint);
                payload.extend(ourFingerprint);

                outputs.push({
                    start: i === 0 ? lowerBound : prevBound,
                    end: curr === upper ? upperBound : this.getMinimalBound(this.getItem(curr - 1), this.getItem(curr)),
                    payload: payload,
                });

                prevBound = outputs[outputs.length - 1].end;
            }

            outputs[outputs.length - 1].end = upperBound;
        }
    }

    buildOutput(initialMessage) {
        let output = new WrappedBuffer();
        let currBound = this._zeroBound();
        let state = this._newState();

        if (initialMessage) {
            if (this.didHandshake) throw Error("already built initial message");
            output.extend([ PROTOCOL_VERSION_0 ]);
            this.didHandshake = true;
        }

        this.pendingOutputs.sort((a,b) => itemCompare(a.start, b.start));

        while (this.pendingOutputs.length) {
            let o = new WrappedBuffer();

            let p = this.pendingOutputs[0];

            let cmp = itemCompare(p.start, currBound);
            // If bounds are out of order or overlapping, finish and resume next time (shouldn't happen because of sort above)
            if (cmp < 0) break;

            if (cmp !== 0) {
                o.extend(this.encodeBound(p.start, state));
                o.extend(this.encodeVarInt(Mode.Skip));
            }

            o.extend(this.encodeBound(p.end, state));
            o.extend(p.payload);

            if (this.frameSizeLimit && output.length + o.length > this.frameSizeLimit - 5) break; // 5 leaves room for Continuation
            output.extend(o);

            currBound = p.end;
            this.pendingOutputs.shift();

        }

        // Server indicates that it has more to send, OR ensure client sends a non-empty message

        if (!this.isInitiator && this.pendingOutputs.length) {
            output.extend(this.encodeBound(this._maxBound(), state));
            output.extend(this.encodeVarInt(Mode.Continuation));
        }

        if (this.isInitiator && output.length === 0 && !this.continuationNeeded) {
            return null;
        }

        let ret = output.unwrap();
        if (!this.wantUint8ArrayOutput) ret = uint8ArrayToHex(ret);
        return ret;
    }

    findUpperBound(first, last, value) {
        let count = last - first;

        while (count > 0) {
            let it = first;
            let step = Math.floor(count / 2);
            it += step;

            if (!(value.timestamp === this.getItemTimestamp(it) ? compareUint8Array(value.id, this.getItemId(it)) < 0 : value.timestamp < this.getItemTimestamp(it))) {
                first = ++it;
                count -= step + 1;
            } else {
                count = step;
            }
        }

        return first;
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

    decodeTimestampIn(encoded, state) {
        let timestamp = this.decodeVarInt(encoded);
        timestamp = timestamp === 0 ? Number.MAX_VALUE : timestamp - 1;
        if (state.lastTimestampIn === Number.MAX_VALUE || timestamp === Number.MAX_VALUE) {
            state.lastTimestampIn = Number.MAX_VALUE;
            return Number.MAX_VALUE;
        }
        timestamp += state.lastTimestampIn;
        state.lastTimestampIn = timestamp;
        return timestamp;
    }

    decodeBound(encoded, state) {
        let timestamp = this.decodeTimestampIn(encoded, state);
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

    encodeTimestampOut(timestamp, state) {
        if (timestamp === Number.MAX_VALUE) {
            state.lastTimestampOut = Number.MAX_VALUE;
            return this.encodeVarInt(0);
        }

        let temp = timestamp;
        timestamp -= state.lastTimestampOut;
        state.lastTimestampOut = temp;
        return this.encodeVarInt(timestamp + 1);
    }

    encodeBound(key, state) {
        let output = new WrappedBuffer();

        output.extend(this.encodeTimestampOut(key.timestamp, state));
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
