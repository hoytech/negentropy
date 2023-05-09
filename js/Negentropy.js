// (C) 2023 Doug Hoyte. MIT license

class WrappedBuffer {
    constructor(buffer) {
        this._raw = new Uint8Array(buffer) || new Uint8Array(1024);
        this.length = buffer ? buffer.length : 0;
        this._expansionRate = 2;
    }

    unwrap() {
        return this._raw.subarray(0, this.length)
    }

    get capacity() {
        return this._raw.byteLength
    }

    extend(buf) {
        const targetSize = buf.length + this.length;
        if (this.capacity < targetSize) {
            const oldRaw = this._raw;
            const newCapacity = Math.max(
                Math.floor(this.capacity * this._expansionRate),
                targetSize
            );
            this._raw = new Uint8Array(newCapacity);
            this._raw.set(oldRaw);
        }

        this._raw.set(buf, this.length);
        this.length += buf.length;
        return this;
    }

    shift() {
        const first = this._raw[0]
        this._raw = this._raw.subarray(1)
        this.length -= 1
        return first
    }

    splice(n = 1) {
        const firstSubarray = this._raw.subarray(0, n)
        this._raw = this._raw.subarray(n)
        this.length -= n
        return firstSubarray
    }
}

class TrieNode {
    constructor() {
        this.children = new Array(256).fill(null);
        this.value = null;
        this.isTail = false;
    }
}

class HashMap {
    constructor() {
        this._root = new TrieNode();
    }

    set(id, value) {
        let node = this._root;
        for (let byte of id) {
            if (!node.children[byte]) {
                node.children[byte] = new TrieNode();
            }
            node = node.children[byte];
        };
        node.value = value;
        node.isTail = true;
    }

    get(keyUint8Array) {
        let node = this._root;
        for (let element of keyUint8Array) {
            if (!node.children[element]) {
                return null; // return null if the key is not in the trie
            }
            node = node.children[element];
        }
        return node.value;
    }

    *[Symbol.iterator]() {
        const key = [];
        function* walk(node) {
            for (let i = 0; i < 256; i++) {
                if (!node.children[i]) continue;
                key.push(i);
                if (node.children[i].isTail) {
                    yield [new Uint8Array(key), node.children[i].value];
                }
                yield* walk(node.children[i]);
                key.pop();
            }
        }
        yield* walk(this._root);
    }
}

class Negentropy {
    constructor(idSize) {
        if (idSize < 8 || idSize > 32) throw Error("idSize invalid");
        this.idSize = idSize;
        this.items = [];
    }

    addItem(timestamp, id) {
        if (this.sealed) throw Error("already sealed");
        if (id.byteLength > 64 || id.byteLength % 2 !== 0) throw Error("bad length for id");
        id = id.subarray(0, this.idSize * 2);

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

    initiate() {
        if (!this.sealed) throw Error("not sealed");
        this.isInitiator = true;

        let output = new WrappedBuffer();
        let state = this._newState();

        this.splitRange(0, this.items.length, this._zeroBound(), this._maxBound(), state, output);
        return output.unwrap();
    }

    reconcile(query) {
        if (!this.sealed) throw Error("not sealed");
        query = new WrappedBuffer(query);
        let haveIds = [], needIds = [];

        let output = new WrappedBuffer();
        let prevBound = this._zeroBound();
        let prevIndex = 0;
        let state = this._newState();
        let skip = false;

        let doSkip = () => {
            if (!skip) return;
            skip = false;
            this.encodeBound(prevBound, state, output);
            output.extend(this.encodeVarInt(0)); // mode = Skip
        };

        while (query.length !== 0) {
            let currBound = this.decodeBound(query, state);
            let mode = this.decodeVarInt(query); // 0 = Skip, 1 = Fingerprint, 2 = IdList, 3 = IdListResponse

            let lower = prevIndex;
            let upper = findUpperBound(this.items, lower, this.items.length, currBound, itemCompare);

            if (mode === 0) { // Skip
                skip = true;
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
                    doSkip();
                    this.splitRange(lower, upper, prevBound, currBound, state, output);
                } else {
                    skip = true;
                }
            } else if (mode === 2) { // IdList
                let numElems = this.decodeVarInt(query);

                let theirElems = new HashMap();
                for (let i = 0; i < numElems; i++) {
                    let id = this.getBytes(query, this.idSize);
                    theirElems.set(id, { offset: i, onBothSides: false });
                }

                let responseHaveIds = [];
                let responseNeedIndices = [];

                for (let i = lower; i < upper; i++) {
                    let id = this.items[i].id;
                    let e = theirElems.get(id);

                    if (!e) {
                        // ID exists on our side, but not their side
                        if (this.isInitiator) haveIds.push(id);
                        else responseHaveIds.push(id);
                    } else {
                        // ID exists on both sides
                        e.onBothSides = true;
                    }
                }

                for (let [k, v] of theirElems) {
                    if (!v.onBothSides) {
                        // ID exists on their side, but not our side
                        if (this.isInitiator) needIds.push(k);
                        else responseNeedIndices.push(v.offset);
                    }
                }

                if (!this.isInitiator) {
                    doSkip();
                    this.encodeBound(currBound, state, output);
                    output.extend(this.encodeVarInt(3)); // mode = IdListResponse

                    output.extend(this.encodeVarInt(responseHaveIds.length));
                    for (let id of responseHaveIds) output.extend(id);

                    let bitField = this.encodeBitField(responseNeedIndices);
                    output.extend(this.encodeVarInt(bitField.length));
                    output.extend(bitField);
                } else {
                    skip = true;
                }
            } else if (mode === 3) { // IdListResponse
                if (!this.isInitiator) throw Error("unexpected IdListResponse");
                skip = true;

                let numIds = this.decodeVarInt(query);
                for (let i = 0; i < numIds; i++) {
                    needIds.push(this.getBytes(query, this.idSize));
                }

                let bitFieldSize = this.decodeVarInt(query);
                let bitField = this.getBytes(query, bitFieldSize);

                for (let i = lower; i < upper; i++) {
                    if (this.bitFieldLookup(bitField, i - lower)) haveIds.push(this.items[i].id);
                }
            } else {
                throw Error("unexpected mode");
            }

            prevIndex = upper;
            prevBound = currBound;
        }

        return [output.unwrap(), haveIds, needIds];
    }

    splitRange(lower, upper, _lowerBound, upperBound, state, output) {
        let numElems = upper - lower;
        let buckets = 16;

        if (numElems < buckets * 2) {
            this.encodeBound(upperBound, state, output);
            output.extend(this.encodeVarInt(2)); // mode = IdList

            output.extend(this.encodeVarInt(numElems));
            for (let it = lower; it < upper; ++it) output.extend(this.items[it].id);
        } else {
            let itemsPerBucket = Math.floor(numElems / buckets);
            let bucketsWithExtra = numElems % buckets;
            let curr = lower;

            for (let i = 0; i < buckets; i++) {
                let ourXorSet = new Uint8Array(this.idSize)
                for (let bucketEnd = curr + itemsPerBucket + (i < bucketsWithExtra ? 1 : 0); curr != bucketEnd; curr++) {
                    for (let j = 0; j < this.idSize; j++) ourXorSet[j] ^= this.items[curr].id[j];
                }

                if (i === buckets - 1) this.encodeBound(upperBound, state, output);
                else this.encodeMinimalBound(this.items[curr], this.items[curr - 1], state, output);

                output.extend(this.encodeVarInt(1)); // mode = Fingerprint
                output.extend(ourXorSet);
            }
        }
    }

    // Decoding

    getByte(buf) {
        if (buf.length === 0) throw Error("parse ends prematurely");
        return buf.shift();
    }

    getBytes(buf, n) {
        if (buf.length < n) throw Error("parse ends prematurely");
        return buf.splice(n);
    }

    decodeVarInt(buf) {
        let res = 0;

        while (1) {
            let byte = this.getByte(buf);
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
        if (n === 0) return new Uint8Array([0]);

        let o = [];

        while (n !== 0) {
            o.push(n & 127);
            n >>>= 7;
        }

        o.reverse();

        for (let i = 0; i < o.length - 1; i++) o[i] |= 128;

        return new Uint8Array(o);
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

    encodeBound(key, state, output) {
        output.extend(this.encodeTimestampOut(key.timestamp, state));
        output.extend(this.encodeVarInt(key.id.length));
        output.extend(key.id);
    }

    encodeMinimalBound(curr, prev, state, output) {
        output.extend(this.encodeTimestampOut(curr.timestamp, state));

        if (curr.timestamp !== prev.timestamp) {
            output.extend(this.encodeVarInt(0));
        } else {
            let sharedPrefixBytes = 0;

            for (let i = 0; i < this.idSize; i++) {
                if (curr.id[i] !== prev.id[i]) break;
                sharedPrefixBytes++;
            }

            output.extend(this.encodeVarInt(sharedPrefixBytes + 1));
            output.extend(curr.id.subarray(0, sharedPrefixBytes + 1));
        }
    };

    encodeBitField(inds) {
        if (inds.length === 0) return [];
        let max = Math.max(...inds);

        let bitField = new Uint8Array(Math.floor((max + 8) / 8));
        for (let ind of inds) bitField[Math.floor(ind / 8)] |= 1 << (ind % 8);

        return bitField;
    }

    bitFieldLookup(bitField, ind) {
        if (Math.floor((ind + 8) / 8) > bitField.length) return false;
        return !!(bitField[Math.floor(ind / 8)] & 1 << (ind % 8));
    }
}




/**
 * @param {Item} a
 * @param {Item} b
 *
 * @returns {number}
 */
function itemCompare(a, b) {
    if (a.timestamp === b.timestamp) {
        return compareUint8Array(a.id, b.id)
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

/**
 * @param {Uint8Array} a
 * @param {Uint8Array} b
 */
function compareUint8Array(a, b) {
    for (let i = 0; i < a.byteLength; i++) {
        if (a[i] < b[i]) {
            return -1
        }

        if (a[i] > b[i]) {
            return 1
        }
    }

    if (a.byteLength > b.byteLength) {
        return 1
    }

    if (a.byteLength < b.byteLength) {
        return -1
    }

    return 0
}

module.exports = Negentropy;
