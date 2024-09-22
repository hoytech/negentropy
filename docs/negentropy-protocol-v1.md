## Negentropy Protocol V1

This document specifies version 1 of the Negentropy set reconciliation protocol. Specifically, it defines the message semantics and low-level wire protocol. For more details on how the protocol functions, see the [negentropy project page](https://github.com/hoytech/negentropy). For a high-level introduction, see the [Range-Based Set Reconciliation](https://logperiodic.com/rbsr.html) article.

### Preparation

There are two protocol participants: The client and the server. The client creates the initial message and transmit to the server. The server always replies with its own message. The client continues querying the server until it is satisifed, and then terminates the protocol. Messages in either direction have the same format. 

Both participants have a collection of items, each of which has a 64-bit numeric timestamp and a 256-bit ID. Each participant starts by sorting their items according to timestamp, ascending. If two timestamps are equivalent, then items are sorted lexically by ID, ascending by first differing byte. Items may not use the max uint64 value (`2**64 - 1`) as a timestamp: it is reserved as a special "infinity" value.

The goal of the protocol is for the client to learn the set of IDs that it has and the server does not, and the set of items that the server has and it does not.

### `Varint`

Varints (variable-sized unsigned integers) are represented as base-128 digits, most significant digit first, with as few digits as possible. Bit eight (the high bit) is set on each byte except the last.

    Varint := <Digit+128>* <Digit>

### `Id`

IDs are represented as byte-strings of length `32`:

    Id := Byte{32}

### `Message`

A reconciliation message is a protocol version byte followed by an ordered list of ranges:

    Message := <protocolVersion (Byte)> <Range>*

The current protocol version is 1, represented by the byte `0x61`. Protocol version 2 will be `0x62`, and so forth. If a server receives a message with a protocol version that it cannot handle, it should reply with a single byte containing the highest protocol version it supports, allowing the client to downgrade and retry its message.

Each Range corresponds to a portion of the timestamp/ID space. The first Range starts at timestamp 0 and an ID of all 0 bytes. Ranges are always adjacent and contiguous (no gaps). If the last Range doesn't end at the special infinity value, an implicit `Skip` to infinity Range is appended. This means that the list of Ranges always covers the full timestamp/ID space.

### `Range`

A Range consists of an upper bound, a mode, and a payload:

    Range := <upperBound (Bound)> <mode (Varint)> <payload (Skip | Fingerprint | IdList)>

The contents of the payload is determined by mode:

* If `mode = 0`, then payload is `Skip`, meaning the sender does not wish to process this Range further. This payload is simply empty:

      Skip :=

* If `mode = 1`, then payload is a `Fingerprint`, which is a [digest](#fingerprint-algorithm) of all the IDs the message sender has within the Range:

      Fingerprint := Byte{16}

* If `mode = 2`, the payload is `IdList`, a variable-length list of all IDs the message sender has within the Range:

      IdList := <length (Varint)> <ids (Id)>*


### `Bound`

Each Range is specified by an *inclusive* lower bound, followed by an *exclusive* upper bound. As defined above, each Range only includes an upper bound: the lower bound of a Range is the upper bound of the previous Range, or 0 timestamp/all 0 ID for the first Range.

Each encoded Bound consists of an encoded timestamp and a variable-length disambiguating prefix of an ID (in case multiple items have the same timestamp):

    Bound := <encodedTimestamp (Varint)> <length (Varint)> <idPrefix (Byte)>*

* The timestamp is encoded specially. The infinity timestamp is encoded as `0`. All other values are encoded as `1 + offset`, where offset is the difference between this timestamp and the previously encoded timestamp. The initial offset starts at `0` and resets at the beginning of each message.

  Offsets are always non-negative since the upper bound's timestamp is greater than or equal to the lower bound's timestamp, ranges in a message are always encoded in ascending order, and ranges never overlap.

* The size of `idPrefix` is encoded in `length`, and can be between `0` and `32` bytes, inclusive. This allows implementations to use the shortest possible prefix needed to separate the first record of this Range from the last record of the previous Range. If these records' timestamps differ, then the length should be 0, otherwise it should be the byte-length of their common ID-prefix plus 1.

  If the `idPrefix` length is less than `32` then the omitted trailing bytes are implicitly 0 bytes.


### Fingerprint Algorithm

The fingerprint of a Range is computed with the following algorithm:

* Compute the addition mod 2<sup>256</sup> of the element IDs (interpreted as 32-byte little-endian unsigned integers)
* Concatenate with the number of elements in the Range, encoded as a [Varint](#varint)
* Hash with SHA-256
* Take the first 16 bytes
