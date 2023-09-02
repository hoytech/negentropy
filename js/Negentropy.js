// (C) 2023 Doug Hoyte. MIT license


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


class Negentropy {
    constructor(idSize = 16, frameSizeLimit = 0) {
        if (idSize < 8 || idSize > 32) throw Error("idSize invalid");
        if (frameSizeLimit !== 0 && frameSizeLimit < 4096) throw Error("frameSizeLimit too small");

        this.idSize = idSize;
        this.frameSizeLimit = frameSizeLimit;

        this.items = [];
        this.pendingOutputs = [];
    }

    addItem(timestamp, id) {
        if (this.sealed) throw Error("already sealed");
        id = this._loadInput(id);

        if (id.byteLength > 64 || id.byteLength < this.idSize) throw Error("bad length for id");
        if (id.byteLength > this.idSize) id = id.subarray(0, this.idSize);

        this.items.push({ timestamp, id });
    }

    seal() {
        if (this.sealed) throw Error("already sealed");

        this.items.sort(itemCompare);
        this.sealed = true;
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

    _loadInput(inp) {
        if (typeof(inp) === 'string') inp = hexToUint8Array(inp);
        else if (__proto__ !== Uint8Array.prototype) inp = new Uint8Array(inp); // node Buffer?
        return inp;
    }

    initiate() {
        if (!this.sealed) throw Error("not sealed");
        this.isInitiator = true;

        this.splitRange(0, this.items.length, this._zeroBound(), this._maxBound(), this.pendingOutputs);

        return this.buildOutput();
    }

    reconcile(query) {
        query = new WrappedBuffer(this._loadInput(query));
        let haveIds = [], needIds = [];

        if (!this.sealed) throw Error("not sealed");
        this.continuationNeeded = false;

        let prevBound = this._zeroBound();
        let prevIndex = 0;
        let state = this._newState();
        let outputs = [];

        while (query.length !== 0) {
            let currBound = this.decodeBound(query, state);
            let mode = this.decodeVarInt(query); // 0 = Skip, 1 = Fingerprint, 2 = IdList, 3 = deprecated, 4 = Continuation

            let lower = prevIndex;
            let upper = findUpperBound(this.items, lower, this.items.length, currBound, itemCompare);

            if (mode === 0) { // Skip
                // Do nothing
            } else if (mode === 1) { // Fingerprint
                let theirXorSet = this.getBytes(query, this.idSize);

                let ourXorSet = new Uint8Array(this.idSize);
                for (let i = lower; i < upper; ++i) {
                    let item = this.items[i];
                    for (let j = 0; j < this.idSize; j++) ourXorSet[j] ^= item.id[j];
                }

                let matches = true;
                for (let i = 0; i < this.idSize; i++) {
                    if (theirXorSet[i] !== ourXorSet[i]) {
                        matches = false;
                        break;
                    }
                }

                if (!matches) {
                    this.splitRange(lower, upper, prevBound, currBound, outputs);
                }
            } else if (mode === 2) { // IdList
                let numIds = this.decodeVarInt(query);

                let theirElems = {}; // stringified Uint8Array -> original Uint8Array
                for (let i = 0; i < numIds; i++) {
                    let e = this.getBytes(query, this.idSize);
                    theirElems[e] = e;
                }

                for (let i = lower; i < upper; i++) {
                    let k = this.items[i].id;

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
                        let payload = this.encodeVarInt(2); // mode = IdList

                        payload.extend(this.encodeVarInt(responseHaveIds.length));
                        for (let id of responseHaveIds) payload.extend(id);

                        let nextSplitBound = (it+1) >= upper ? currBound : this.getMinimalBound(this.items[it], this.items[it+1]);

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
                        responseHaveIds.push(this.items[it].id);
                        if (responseHaveIds.length >= 100) flushIdListOutput(); // 100*32 is less than minimum frame size limit of 4k
                    }

                    flushIdListOutput();
                }
            } else if (mode === 3) { // Deprecated
                throw Error("other side is speaking old negentropy protocol");
            } else if (mode === 4) { // Continuation
                this.continuationNeeded = true;
            } else {
                throw Error("unexpected mode");
            }

            prevIndex = upper;
            prevBound = currBound;
        }

        while (outputs.length) {
            this.pendingOutputs.unshift(outputs.pop());
        }

        return [this.buildOutput(), haveIds, needIds];
    }

    splitRange(lower, upper, lowerBound, upperBound, outputs) {
        let numElems = upper - lower;
        let buckets = 16;

        if (numElems < buckets * 2) {
            let payload = this.encodeVarInt(2); // mode = IdList
            payload.extend(this.encodeVarInt(numElems));
            for (let it = lower; it < upper; ++it) payload.extend(this.items[it].id);

            outputs.push({ start: lowerBound, end: upperBound, payload: payload, });
        } else {
            let itemsPerBucket = Math.floor(numElems / buckets);
            let bucketsWithExtra = numElems % buckets;
            let curr = lower;
            let prevBound = this.items[curr];

            for (let i = 0; i < buckets; i++) {
                let ourXorSet = new Uint8Array(this.idSize)
                for (let bucketEnd = curr + itemsPerBucket + (i < bucketsWithExtra ? 1 : 0); curr != bucketEnd; curr++) {
                    for (let j = 0; j < this.idSize; j++) ourXorSet[j] ^= this.items[curr].id[j];
                }

                let payload = this.encodeVarInt(1); // mode = Fingerprint
                payload.extend(ourXorSet);

                outputs.push({
                    start: i === 0 ? lowerBound : prevBound,
                    end: curr === this.items.length ? upperBound : this.getMinimalBound(this.items[curr - 1], this.items[curr]),
                    payload: payload,
                });

                prevBound = outputs[outputs.length - 1].end;
            }

            outputs[outputs.length - 1].end = upperBound;
        }
    }

    buildOutput() {
        let output = new WrappedBuffer();
        let currBound = this._zeroBound();
        let state = this._newState();

        this.pendingOutputs.sort((a,b) => itemCompare(a.start, b.start));

        while (this.pendingOutputs.length) {
            let o = new WrappedBuffer();

            let p = this.pendingOutputs[0];

            let cmp = itemCompare(p.start, currBound);
            // When bounds are out of order or overlapping, finish and resume next time (shouldn't happen because of sort above)
            if (cmp < 0) break;

            if (cmp !== 0) {
                o.extend(this.encodeBound(p.start, state));
                o.extend(this.encodeVarInt(0)); // mode = Skip
            }

            o.extend(this.encodeBound(p.end, state));
            o.extend(p.payload);

            if (this.frameSizeLimit && output.length + o.length > this.frameSizeLimit - 5) break; // 5 leaves room for Continuation
            output.extend(o);

            currBound = p.end;
            this.pendingOutputs.shift();

        }

        // Server indicates that it has more to send, OR ensure client sends a non-empty message

        if ((!this.isInitiator && this.pendingOutputs.length) || (this.isInitiator && output.length == 0 && this.continuationNeeded)) {
            output.extend(this.encodeBound(this._maxBound(), state));
            output.extend(this.encodeVarInt(4)); // mode = Continue
        }

        let ret = output.unwrap();
        if (!this.wantUint8ArrayOutput) ret = uint8ArrayToHex(ret);
        return ret;
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


function binarySearch(arr, first, last, cmp) {
    let count = last - first;

    while (count > 0) {
        let it = first;
        let step = Math.floor(count / 2);
        it += step;

        if (cmp(arr[it])) {
            first = ++it;
            count -= step + 1;
        } else {
            count = step;
        }
    }

    return first;
}

function findLowerBound(arr, first, last, value, cmp) {
    return binarySearch(arr, first, last, (a) => cmp(a, value) < 0);
}

function findUpperBound(arr, first, last, value, cmp) {
    return binarySearch(arr, first, last, (a) => cmp(value, a) >= 0);
}


module.exports = Negentropy;
