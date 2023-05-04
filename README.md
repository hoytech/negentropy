# negentropy

This repo contains the protocol specification, reference implementations, and tests for the negentropy set-reconcilliation protocol.

<!-- TOC FOLLOWS -->
<!-- START OF TOC -->
* [Introduction](#introduction)
* [Protocol](#protocol)
  * [Data Requirements](#data-requirements)
  * [Setup](#setup)
  * [Alternating Messages](#alternating-messages)
  * [Algorithm](#algorithm)
* [Definitions](#definitions)
  * [Varint](#varint)
  * [Bound](#bound)
  * [Range](#range)
  * [Message](#message)
* [Analysis](#analysis)
* [APIs](#apis)
  * [C++](#c)
  * [Javascript](#javascript)
* [Implementation Enhancements](#implementation-enhancements)
  * [Deferred Range Processing](#deferred-range-processing)
  * [Pre-computing](#pre-computing)
* [Copyright](#copyright)
<!-- END OF TOC -->


## Introduction

Set reconcilliation supports the replication or syncing of data-sets, either because they were created independently, or because they have drifted out of sync because of downtime, network partitions, misconfigurations, etc. In the latter case, detecting and fixing these inconsistencies is sometimes called [anti-entropy repair](https://docs.datastax.com/en/cassandra-oss/3.x/cassandra/operations/opsRepairNodesManualRepair.html).

Suppose two participants on a network each have a set of records that they have collected independently. Set-reconcilliation efficiently determines which records one side has that the other side doesn't, and vice versa. After the records that are missing have been determined, this information can be used to transfer the missing data items. The actual transfer is external to the negentropy protocol.

Although there are many ways to do set reconcilliation, negentropy is based on [Aljoscha Meyer's method](https://github.com/AljoschaMeyer/set-reconciliation), which has the advantage of being simple to explain and implement.



## Protocol

### Data Requirements

In order to use negentropy, you need to define some mappings from your data records:

* `record -> ID`
  * Typically a cryptographic hash of the entire record
  * The ID should be 32 bytes in length (although smaller IDs are supported too)
  * Two different records should not have the same ID (satisfied by using a cryptographic hash)
  * Two identical records should not have different IDs (records should be canonicalised prior to hashing, if necessary)
* `record -> timestamp`
  * Although timestamp is the most obvious, any ordering criteria can be used. The protocol will be most efficient if records with similar timestamps are often downloaded/stored/generated together
  * Timestamps do *not* need to be unique (different records can have the same timestamp)
  * Units can be anything (seconds, microseconds, etc) as long as they fit in an 64-bit unsigned integer
  * The largest 64-bit unsigned integer should be reserved as a special "infinity" value

Negentropy does not support the concept of updating or changing a record while preserving its ID. This should instead be modelled as deleting the old record and inserting a new one.

### Setup

The two parties engaged in the protocol are called the client and the server. The client is also called the *initiator*, because it creates and sends the first message in the protocol.

Each party should begin by sorting their records in ascending order by timestamp. If the timestamps are equivalent, records should be sorted lexically by their IDs. This sorted array and contiguous slices of it are called *ranges*. The *fingerprint* of a range is equal to the bitwise eXclusive OR (XOR) of the IDs of all contained records.

Because each side potentially has a different set of records, ranges cannot be referred to by their indices in one side's sorted array. Instead, they are specified by lower and upper *bounds*. A bound is a timestamp and a variable-length ID prefix. In order to reduce the sizes of reconcilliation messages, ID prefixes are as short as possible while still being able to separate records from their predecessors in the sorted array. If two adjacent records have different timestamps, then the prefix for a bound between them is empty.

Lower bounds are *inclusive* and upper bounds are *exclusive*, as is [typical in computer science](https://www.cs.utexas.edu/users/EWD/transcriptions/EWD08xx/EWD831.html). This means that given two adjacent ranges, the upper bound of the first is equal to the lower bound of the second. In order for a range to have full coverage over the universe of possible timestamps/IDs, the lower bound would have a 0 timestamp and all-0s ID, and the upper-bound would be the specially reserved "infinity" timestamp (max u64), and the ID doesn't matter.

When negotiating a reconcilliation, the client and server should decide on a special `idSize` value. This must be `<= 32`. Using values less than the full 32 bytes will save bandwidth, at the expense of making collisions more likely.

### Alternating Messages

After both sides have setup their sorted arrays, the client creates an initial message and sends it to the server. The server will then reply with another message, and the two parties continue exchanging messages until the protocol terminates (see below). After the protocol terminates, the client will have determined what IDs it has (and the server needs) and which it needs (and the server has).

Each message consists of an ordered sequence of ranges. Each range contains an upper bound, a mode, and a payload. The range's lower bound is the same as the previous range's upper bound (or 0, if none). The mode indicates what type of processing is needed for this range, and therefore how the payload should be parsed.

The modes supported are:

* `Skip`: No further processing is needed for this range. Payload is empty.
* `Fingerprint`: Payload contains the fingerprint for this range.
* `IdList`: Payload contains a complete list of IDs for this range.
* `IdListResponse`: Only allowed for server->client messages. Contains a list of IDs the server has and the client needs, as well as a bit-field that represents which IDs the server needs and the client has.

If a message does not end in a range with an "infinity" upper bound, an implicit range with upper bound of "infinity" and mode `Skip` is appended. This means that an empty message indicates that all ranges have been processed and the sender believes the protocol can now terminate.

### Algorithm

Upon receiving a message, the recipient should loop over the message's ranges in order, while concurrently constructing a new message. `Skip` ranges are answered with `Skip` ranges, and adjacent `Skip` ranges should be coalesced into a single `Skip` range.

`IdList` ranges represent a complete list of IDs held by the sender. Because the receiver obviously knows the items it has, this information is enough to fully reconcile the range. Therefore, when the client receives an `IdList` range, it should reply with a `Skip` range. However, since the goal of the protocol is to ensure the *client* has this information, when a server receives an `IdList` range it should reply with an `IdListResponse` range.

The `IdListResponse` range contains a list of the IDs the server has (and the client needs), but uses a packed bit-field representation to refer to the IDs the client has that the server needs. This avoids having to either a) transfer the complete set of its own IDs, or b) redundantly re-transfer IDs that were sent by the client.

`Fingerprint` ranges contain enough information to determine whether or not the data items within a range are equivalent on each side, however determining the actual difference in IDs requires further recursive processing.
  * Because `IdList` and `IdListResponse` messages terminate processing for a given range, they are called *base case* messages.
  * When the fingerprints on each side differ, the reciever should *split* its own range and send the results back in the next message. When splitting, the number of records within each sub-range should be considered. When small, an `IdList` range should be sent. When large, then the sub-range should itself be sent as a `Fingerprint` (this is the recursion).
  * When a range is split, the sub-ranges should completely cover the original range's lower and upper bounds.
  * How to split the range is implementation-defined. The simplest way is to divide the records that fall within the range into N equal-sized buckets, and emit a Fingerprint sub-range for each of these buckets. However, an implementation could choose different grouping criteria. For example, events with similar timestamps could be grouped into a single bucket. If the implementation believes the other side is less likely to have recent events, it could make the most recent bucket an `IdList`.
  * Note that if alternate grouping strategies are used, an implementation should never reply to a range with a single `Fingerprint` range, otherwise the protocol may never terminate (if the other side does the same).

The initial message should cover the full universe, and therefore must have at least one range. The last range's upper bound should have the infinity timestamp (and the `id` doesn't matter, so should be empty also). How many ranges used in the initial message depends on the implementation. The most obvious implementation is to use the same logic as described above, either using the base case or splitting, depending on set size. However, an implementation may choose to use fewer or more buckets in its initial message, and/or may use different grouping strategies.

Once the client has looped over all ranges in a server's message and its constructed response message is a full-universe `Skip` range (ie, the empty string `""`), then it needs no more information from the server and therefore it should terminate the protocol.



## Definitions

### Varint

Varints (variable-sized integers) are a format for storing unsigned integers in a small number of bytes, commonly called BER (Binary Encoded Representation). They are stored as base 128 digits, most significant digit first, with as few digits as possible. Bit eight (the high bit) is set on each byte except the last.

    Varint := <Digit+128>* <Digit>

### Bound

The protocol relies on bounds to group ranges of data items. Each range is specified by an *inclusive* lower bound, followed by an *exclusive* upper bound. As noted above, only upper bounds are transmitted (the lower bound of a range is the upper bound of the previous range, or 0 for the first range).

Each bound consists of an encoded timestamp and a variable-length disambiguating prefix of an event ID (in case multiple items have the same timestamp):

    Bound := <encodedTimestamp (Varint)> <length (Varint)> <idPrefix (Byte)>*

* The timestamp is encoded specially. The "infinity timestamp" (such that all valid items precede it) is encoded as `0`. All other values are encoded as `1 + offset`, where offset is the difference between this timestamp and the previously encoded timestamp. The initial offset starts at `0` and resets at the beginning of each message.

  Offsets are always non-negative since the upper bound's timestamp is always `>=` to the lower bound's timestamp, ranges in a message are always encoded in ascending order, and ranges never overlap.

* The `idPrefix` parameter's size is encoded in `length`, and can be between `0` and `idSize` bytes. Efficient implementations will use the shortest possible prefix needed to separate the first record of this range from the last record of the previous range. If these records' timestamps differ, then the length should be 0, otherwise it should be the byte-length of their common prefix plus 1.

  If the `idPrefix` length is less than `idSize` then the omitted trailing bytes are filled with 0 bytes.

### Range

IDs are represented as byte-strings truncated to length `idSize`:

    Id := Byte{idSize}

A range consists of an upper bound, a mode, and a payload (determined by mode):

    Range := <upperBound (Bound)> <mode (Varint)> <Skip | Fingerprint | IdList | IdListResponse>

* If `mode = 0`, then payload is `Skip`, which is simply empty:

      Skip :=

* If `mode = 1`, then payload is `Fingerprint`, the bitwise eXclusive OR of all the IDs in this range, truncated to `idSize`:

      Fingerprint := <Id>

* If `mode = 2`, the payload is `IdList`, a variable-length list of all IDs within this range:

      IdList := <length (Varint)> <ids (Id)>*

* If `mode = 3`, the payload is `IdListResponse`. This is only sent by the server in response to an `IdList` range. It contains an `IdList` containing IDs only the server-side has, and a bit-field where each bit (starting from the least-significant bit of first byte) indicates if the Nth client-side ID is needed by the server:

      IdListResponse := <haveIds (IdList)> <bitFieldLength (Varint)> <bitField (Byte)>*

### Message

A reconcilliation message is just an ordered list of ranges:

    Message := <Range>*

An empty message is an implicit `Skip` over the full universe of IDs, and represents that the protocol can terminate.



## Analysis

If you are searching for a single record in an ordered array, binary search allows you to find the record with a logarithmic number of operations. This is because each operation cuts the search space in half. So, searching a list of 1 million items will take about 20 operations:

    log(1e6)/log(2) = 19.9316

Negentropy uses a similar principle. Each communication divides items into their own buckets and compares the fingerprints of the buckets. If we always split into 2 buckets, and there was exactly 1 difference, we would cut the search-space in half on each operation.

For effective performance, negentropy requires minimising the number of "round-trips" between the two sides. A sync that takes 20 back-and-forth communications to determine a single difference would take unacceptably long. Fortunately we can expend a small amount of extra bandwidth by splitting our ranges into more than 2 ranges. This has the effect of increasing the base of the logarithm. For example, if we split it into 16 pieces instead:

    log(1e6)/log(16) = 4.98289

Additionally, each direction of the protocol's communication can result in a split, so since we are measuring round-trips, we divide this by two:

    log(1e6)/log(16)/2 = 2.49145

This means that in general, three round-trip communications will be required to synchronise two sets of 1 million records that differ by 1 record. With an `idSize` of 16, each communication will consume `16*16 + overhead` -- roughly 300 bytes. So total bandwidth in one direction would be about 900 bytes and the other direction about 600 bytes.

What if they differ by multiple records? Because communication is batched, the splitting of multiple differing ranges can happen in parallel. So, the number of round-trips will not be affected (assuming that every message can be delivered in exactly one packet transmission, independent of size, which is of course not entirely true on real networks).

The amount of bandwidth consumed will grow linearly with the number of differences, but this would of course be true even assuming a perfect synchronisation method that had no overhead other than transmitting the differing records.




## APIs

### C++

The library is contained in a single-header with no non-standard dependencies:

    #include "Negentropy.h"

First, create a `Negentropy` object. The `16` argument is `idSize`:

    Negentropy ne(16);

Next, add all the items in your collection, and `seal()`:

    for (const auto &item : myItems) {
        ne.addItem(item.timestamp(), item.id());
    }

    ne.seal();

On the client-side, create an initial message, and then transmit it to the server, receive the response, and `reconcile` until complete:

    std::string msg = ne.initiate();

    while (msg.size() != 0) {
        std::string response = queryServer(msg);
        std::vector<std::string> have, need;
        msg = ne.reconcile(response, have, need);
        // handle have/need
    }

In each loop iteration, `have` contains IDs that the client has that the server doesn't, and `need` contains IDs that the server has that the client doesn't.

The server-side is similar, except it doesn't create an initial message, and there are no `have`/`need` arrays:

    while (1) {
        std::string msg = receiveMsgFromClient();
        std::string response = ne.reconcile(msg);
        respondToClient(response);
    }

### Javascript

The library is contained in a single javascript file. It shouldn't need any dependencies, in either a browser or node.js:

    const Negentropy = require('Negentropy.js');

First, create a `Negentropy` object. The `16` argument is `idSize`:

    let ne = new Negentropy(16);

Next, add all the items in your collection, and `seal()`:

    for (let item of myItems) {
        ne.addItem(item.timestamp(), item.id());
    }

    ne.seal();

On the client-side, create an initial message, and then transmit it to the server, receive the response, and `reconcile` until complete:

    let msg = ne.initiate();

    while (msg.length != 0) {
        let response = queryServer(msg);
        let [newMsg, have, need] = ne.reconcile(msg);
        msg = newMsg;
        // handle have/need
    }

The server-side is similar, except it doesn't create an initial message, and there are no `have`/`need` arrays:

    while (1) {
        let msg = receiveMsgFromClient();
        let [response] = ne.reconcile(msg);
        respondToClient(response);
    }


## Implementation Enhancements

### Deferred Range Processing

If there are too many differences and/or they are too randomly distributed throughout the range, then message sizes may become unmanageably large. This may be undesirable because of the memory required for buffering, and also because large batch sizes prevents work pipelining, where the synchronised records are processed while additional syncing is occurring.

Because of this, a client implementation may choose to defer the processing of ranges. Rather than transmit all the ranges it has found that need syncing, it can transmit a smaller number and keep the remaining for subsequent message rounds. This will decrease the message size at the expense of increasing the number of messaging round-trips.

A client could target fixed size messages, or could dynamically tune the message sizes based on its throughput metrics.

### Pre-computing

Instead of aggregating the data items for each query, servers and/or clients may choose to pre-compute fingerprints for their entire set of data items, or particular subsets. Most likely, fingerprints will be aggregated in a tree data-structure so it is efficient to add or remove items.

How or if this is implemented is independent of the protocol as described in this document.



## Copyright

(C) 2023 Doug Hoyte

Protocol specification, reference implementations, and tests are MIT licensed.
