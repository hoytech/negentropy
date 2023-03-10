// (C) 2023 Doug Hoyte. MIT license

class Negentropy {
    constructor(idSize) {
        if (idSize < 8 || idSize > 32) throw Error("idSize invalid");
        this.idSize = idSize;
        this.items = [];
    }

    addItem(timestamp, id) {
        if (this.sealed) throw Error("already sealed");
        if (id.length > 64 || id.length % 2 !== 0) throw Error("bad length for id");
        id = id.substr(0, this.idSize * 2);

        this.items.push({ timestamp, id: fromHexString(id), idHex: id, });
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
        let id = new Array(this.idSize).fill(0);
        return { timestamp: 0, id, idHex: toHexString(id), };
    }

    initiate() {
        if (!this.sealed) throw Error("not sealed");
        this.isInitiator = true;

        let output = [];
        let state = this._newState();

        this.splitRange(0, this.items.length, this._zeroBound(), { timestamp: Number.MAX_VALUE, id: [], }, state, output);
        return toHexString(output);
    }

    reconcile(query) {
        if (!this.sealed) throw Error("not sealed");
        query = fromHexString(query);
        let haveIds = [], needIds = [];

        let output = [];
        let prevBound = this._zeroBound();
        let prevIndex = 0;
        let state = this._newState();
        let skip = false;

        let doSkip = () => {
            if (!skip) return;
            skip = false;
            output.push(...this.encodeBound(prevBound, state));
            output.push(...this.encodeVarInt(0)); // mode = Skip
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

                let ourXorSet = new Array(this.idSize).fill(0);
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

                let theirElems = {};
                for (let i = 0; i < numElems; i++) {
                    let id = toHexString(this.getBytes(query, this.idSize));
                    theirElems[id] = { offset: i, onBothSides: false, };
                }

                let responseHaveIds = [];
                let responseNeedIndices = [];

                for (let i = lower; i < upper; i++) {
                    let id = this.items[i].idHex;
                    let e = theirElems[id];

                    if (e === undefined) {
                        // ID exists on our side, but not their side
                        if (this.isInitiator) haveIds.push(id);
                        else responseHaveIds.push(id);
                    } else {
                        // ID exists on both sides
                        theirElems[id].onBothSides = true;
                    }
                }

                for (let k of Object.keys(theirElems)) {
                    if (!theirElems[k].onBothSides) {
                        // ID exists on their side, but not our side
                        if (this.isInitiator) needIds.push(k);
                        else responseNeedIndices.push(theirElems[k].offset);
                    }
                }

                if (!this.isInitiator) {
                    doSkip();
                    output.push(...this.encodeBound(currBound, state));
                    output.push(...this.encodeVarInt(3)); // mode = IdListResponse

                    output.push(...this.encodeVarInt(responseHaveIds.length));
                    for (let id of responseHaveIds) output.push(...fromHexString(id));

                    let bitField = this.encodeBitField(responseNeedIndices);
                    output.push(...this.encodeVarInt(bitField.length));
                    output.push(...bitField);
                } else {
                    skip = true;
                }
            } else if (mode === 3) { // IdListResponse
                if (!this.isInitiator) throw Error("unexpected IdListResponse");
                skip = true;

                let numIds = this.decodeVarInt(query);
                for (let i = 0; i < numIds; i++) {
                    needIds.push(toHexString(this.getBytes(query, this.idSize)));
                }

                let bitFieldSize = this.decodeVarInt(query);
                let bitField = this.getBytes(query, bitFieldSize);

                for (let i = lower; i < upper; i++) {
                    if (this.bitFieldLookup(bitField, i - lower)) haveIds.push(this.items[i].idHex);
                }
            } else {
                throw Error("unexpected mode");
            }

            prevIndex = upper;
            prevBound = currBound;
        }

        return [toHexString(output), haveIds, needIds];
    }

    splitRange(lower, upper, lowerBound, upperBound, state, output) {
        let numElems = upper - lower;
        let buckets = 16;

        if (numElems < buckets * 2) {
            output.push(...this.encodeBound(upperBound, state));
            output.push(...this.encodeVarInt(2)); // mode = IdList

            output.push(...this.encodeVarInt(numElems));
            for (let it = lower; it < upper; ++it) output.push(...this.items[it].id);
        } else {
            let itemsPerBucket = Math.floor(numElems / buckets);
            let bucketsWithExtra = numElems % buckets;
            let curr = lower;

            for (let i = 0; i < buckets; i++) {
                let ourXorSet = new Array(this.idSize).fill(0);
                for (let bucketEnd = curr + itemsPerBucket + (i < bucketsWithExtra ? 1 : 0); curr != bucketEnd; curr++) {
                    for (let j = 0; j < this.idSize; j++) ourXorSet[j] ^= this.items[curr].id[j];
                }

                if (i === buckets - 1) output.push(...this.encodeBound(upperBound, state));
                else output.push(...this.encodeMinimalBound(this.items[curr], this.items[curr - 1], state));

                output.push(...this.encodeVarInt(1)); // mode = Fingerprint
                output.push(...ourXorSet);
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
        return buf.splice(0, n);
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
        if (len > this.idSize) throw herr("bound key too long");
        let id = this.getBytes(encoded, len);
        return { timestamp, id, idHex: toHexString(id), };
    }

    // Encoding

    encodeVarInt(n) {
        if (n === 0) return [0];

        let o = [];

        while (n !== 0) {
            o.push(n & 0x7F);
            n >>>= 7;
        }

        o.reverse();

        for (let i = 0; i < o.length - 1; i++) o[i] |= 0x80;

        return o;
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
        let output = [];

        output.push(...this.encodeTimestampOut(key.timestamp, state));
        output.push(...this.encodeVarInt(key.id.length));
        output.push(...key.id);

        return output;
    }

    encodeMinimalBound(curr, prev, state) {
        let output = [];

        output.push(...this.encodeTimestampOut(curr.timestamp, state));

        if (curr.timestamp !== prev.timestamp) {
            output.push(...this.encodeVarInt(0));
        } else {
            let sharedPrefixBytes = 0;

            for (let i = 0; i < this.idSize; i++) {
                if (curr.id[i] !== prev.id[i]) break;
                sharedPrefixBytes++;
            }

            output.push(...this.encodeVarInt(sharedPrefixBytes + 1));
            output.push(...curr.id.slice(0, sharedPrefixBytes + 1));
        }

        return output;
    };

    encodeBitField(inds) {
        if (inds.length === 0) return [];
        let max = Math.max(...inds);

        let bitField = new Array(Math.floor((max + 8) / 8)).fill(0);
        for (let ind of inds) bitField[Math.floor(ind / 8)] |= 1 << (ind % 8);

        return bitField;
    }

    bitFieldLookup(bitField, ind) {
        if (Math.floor((ind + 8) / 8) > bitField.length) return false;
        return !!(bitField[Math.floor(ind / 8)] & 1 << (ind % 8));
    }
}




function fromHexString(hexString) {
    if ((hexString.length % 2) !== 0) throw Error("uneven length of hex string");
    if (hexString.length === 0) return [];
    return hexString.match(/../g).map((byte) => parseInt(byte, 16));
}

function toHexString(buf) {
    return buf.reduce((str, byte) => str + byte.toString(16).padStart(2, '0'), '');
}

function itemCompare(a, b) {
    if (a.timestamp === b.timestamp) {
        if (a.idHex < b.idHex) return -1;
        else if (a.idHex > b.idHex) return 1;
        return 0;
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
