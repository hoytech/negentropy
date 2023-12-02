![negentropy logo](docs/logo.svg)

This repo contains the protocol specification, reference implementations, and tests for the negentropy set-reconciliation protocol.

<!-- TOC FOLLOWS -->
<!-- START OF TOC -->
* [Introduction](#introduction)
* [Protocol](#protocol)
  * [Data Requirements](#data-requirements)
  * [Setup](#setup)
  * [Alternating Messages](#alternating-messages)
  * [Algorithm](#algorithm)
  * [Frame Size Limits](#frame-size-limits)
* [Definitions](#definitions)
  * [Varint](#varint)
  * [Bound](#bound)
  * [Range](#range)
  * [Message](#message)
* [Analysis](#analysis)
* [Reference Implementation APIs](#reference-implementation-apis)
  * [C++](#c)
  * [Javascript](#javascript)
  * [Rust](#rust)
* [Adversarial Input](#adversarial-input)
* [Pre-computing Fingerprints](#pre-computing-fingerprints)
* [Use-Cases](#use-cases)
* [Copyright](#copyright)
<!-- END OF TOC -->


## Introduction

Set-reconciliation supports the replication or syncing of data-sets, either because they were created independently, or because they have drifted out of sync due to downtime, network partitions, misconfigurations, etc. In the latter case, detecting and fixing these inconsistencies is sometimes called [anti-entropy repair](https://docs.datastax.com/en/cassandra-oss/3.x/cassandra/operations/opsRepairNodesManualRepair.html).

Suppose two participants on a network each have a set of records that they have collected independently. Set-reconciliation efficiently determines which records one side has that the other side doesn't, and vice versa. After the records that are missing have been determined, this information can be used to transfer the missing data items. The actual transfer is external to the negentropy protocol.

Negentropy is based on Aljoscha Meyer's work on "Range-Based Set Reconciliation" ([overview](https://github.com/AljoschaMeyer/set-reconciliation) / [paper](https://arxiv.org/abs/2212.13567) / [master's thesis](https://github.com/AljoschaMeyer/master_thesis/blob/main/main.pdf)).



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
  * Units can be anything (seconds, microseconds, etc) as long as they fit in an 64-bit unsigned integer
  * The largest 64-bit unsigned integer should be reserved as a special "infinity" value
  * Timestamps do *not* need to be unique (different records can have the same timestamp). If necessary, `0` can be used as the timestamp for every record

Negentropy does not support the concept of updating or changing a record while preserving its ID. This should instead be modelled as deleting the old record and inserting a new one.

### Setup

The two parties engaged in the protocol are called the client and the server. The client is also called the *initiator*, because it creates and sends the first message in the protocol.

Each party should begin by sorting their records in ascending order by timestamp. If the timestamps are equivalent, records should be sorted lexically by their IDs. This sorted array and contiguous slices of it are called *ranges*. The *fingerprint* of a range is equal to the SHA-256 hash of the IDs of all records within this range (sorted as described above).

Because each side potentially has a different set of records, ranges cannot be referred to by their indices in one side's sorted array. Instead, they are specified by lower and upper *bounds*. A bound is a timestamp and a variable-length ID prefix. In order to reduce the sizes of reconciliation messages, ID prefixes are as short as possible while still being able to separate records from their predecessors in the sorted array. If two adjacent records have different timestamps, then the prefix for a bound between them is empty.

Lower bounds are *inclusive* and upper bounds are *exclusive*, as is [typical in computer science](https://www.cs.utexas.edu/users/EWD/transcriptions/EWD08xx/EWD831.html). This means that given two adjacent ranges, the upper bound of the first is equal to the lower bound of the second. In order for a range to have full coverage over the universe of possible timestamps/IDs, the lower bound would have a 0 timestamp and all-0s ID, and the upper-bound would be the specially reserved "infinity" timestamp (max u64), and the ID doesn't matter.

When negotiating a reconciliation, the client and server should decide on a special `idSize` value. It must satisfy `8 <= idSize <= 32`. Using values less than the full 32 bytes will save bandwidth, at the expense of making collisions more likely.

### Alternating Messages

After both sides have setup their sorted arrays, the client creates an initial message and sends it to the server. The server will then reply with another message, and the two parties continue exchanging messages until the protocol terminates (see below). After the protocol terminates, the client will have determined what IDs it has (and the server needs) and which it needs (and the server has).

The initial message consists of a protocol version byte followed by an ordered sequence of ranges. Subsequent messages are the same, except without the protocol version byte. Each range contains an upper bound, a mode, and a payload. The range's lower bound is the same as the previous range's upper bound (or 0, if it is the first range). The mode indicates what type of processing is needed for this range, and therefore how the payload should be parsed.

The modes supported are:

* `Skip`: No further processing is needed for this range. Payload is empty.
* `Fingerprint`: Payload contains the fingerprint for this range.
* `IdList`: Payload contains a complete list of IDs for this range.
* `Continuation`: Indicates that the sender has more to send in the next reconciliation round, but is not sending now because of a configured [frame size limit](#frame-size-limits).
* `UnsupportedProtocolVersion`: The server is unable to handle the protocol version suggested by the client.

If a message does not end in a range with an "infinity" upper bound, an implicit range with upper bound of "infinity" and mode `Skip` is appended. This means that an empty message indicates that all ranges have been processed and the sender believes the protocol can now terminate.

### Algorithm

Upon receiving a message, the recipient should loop over the message's ranges in order, while concurrently constructing a new message. `Skip` ranges are answered with `Skip` ranges, and adjacent `Skip` ranges should be coalesced into a single `Skip` range.

`IdList` ranges represent a complete list of IDs held by the sender. Because the receiver obviously knows the items it has, this information is enough to fully reconcile the range. Therefore, when the client receives an `IdList` range, it should reply with a `Skip` range. However, since the goal of the protocol is to ensure the *client* has this information, when a server receives an `IdList` range it should reply with its own ranges (typically `IdList` and/or skip ranges).

`Fingerprint` ranges contain enough information to determine whether or not the data items within a range are equivalent on each side, however determining the actual difference in IDs requires further recursive processing.
  * Since `IdList` or `Skip` messages will always cause the client to terminate processing for the given ranges, these messages are considered *base cases*.
  * When the fingerprints on each side differ, the reciever should *split* its own range and send the results back in the next message. When splitting, the number of records within each sub-range should be considered. When small, an `IdList` range should be sent. When large, the sub-ranges should themselves be sent as `Fingerprint`s (this is the recursion).
  * When a range is split, the sub-ranges should completely cover the original range's lower and upper bounds.
  * Unlike in Meyer's designs, an "empty" fingerprint is never sent to indicate the absence of items within a range. Instead, an `IdList` of length 0 is sent because it is smaller.
  * How to split the range is implementation-defined. The simplest way is to divide the records that fall within the range into N equal-sized buckets, and emit a `Fingerprint` sub-range for each of these buckets. However, an implementation could choose different grouping criteria. For example, events with similar timestamps could be grouped into a single bucket. If the implementation believes recent events are less likely to be reconciled, it could make the most recent bucket an `IdList` instead of `Fingerprint`.
  * Note that if alternate grouping strategies are used, an implementation should never reply to a range with a single `Fingerprint` range, otherwise the protocol may never terminate (if the other side does the same).

The initial message should cover the full universe, and therefore must have at least one range. The last range's upper bound should have the infinity timestamp (and the `id` doesn't matter, so should be empty also). How many ranges used in the initial message depends on the implementation. The most obvious implementation is to use the same logic as described above, either using the base case or splitting, depending on set size. However, an implementation may choose to use fewer or more buckets in its initial message, and/or may use different grouping strategies.

Once the client has looped over all ranges in a server's message and its constructed response message is a full-universe `Skip` range (ie, the empty string `""`), then it needs no more information from the server and therefore it should terminate the protocol.

### Frame Size Limits

If there are too many differences and/or they are too evenly distributed throughout the range, then message sizes may become unmanageably large. This may be undesirable because the network transport may have message size limitations, and also because large batch sizes inhibit work pipelining, where the synchronised records are processed as additional syncing occurs.

Because of this, implementations may support a *frame size limit* parameter. If configured, all messages created by this instance will be of length equal to or smaller than this number of bytes. After processing each message, any discovered differences will be included in the `have`/`need` arrays on the client.

To implement this, instead of sending all the ranges it has found that need syncing, the instance will send a smaller number of them to stay under the size limit. Remaining ranges will be transmitted in subsequent message rounds. This means that a frame size limit can increase the number of messaging round-trips.


## Definitions

### Varint

Varints (variable-sized integers) are a format for serialising unsigned integers into a small number of bytes. They are stored as base 128 digits, most significant digit first, with as few digits as possible. Bit eight (the high bit) is set on each byte except the last.

    Varint := <Digit+128>* <Digit>

### Bound

The protocol relies on bounds to group ranges of data items. Each range is specified by an *inclusive* lower bound, followed by an *exclusive* upper bound. As noted above, only upper bounds are transmitted (the lower bound of a range is the upper bound of the previous range, or 0 for the first range).

Each encoded bound consists of an encoded timestamp and a variable-length disambiguating prefix of an event ID (in case multiple items have the same timestamp):

    Bound := <encodedTimestamp (Varint)> <length (Varint)> <idPrefix (Byte)>*

* The timestamp is encoded specially. The "infinity timestamp" (such that all valid items precede it) is encoded as `0`. All other values are encoded as `1 + offset`, where offset is the difference between this timestamp and the previously encoded timestamp. The initial offset starts at `0` and resets at the beginning of each message.

  Offsets are always non-negative since the upper bound's timestamp is always `>=` to the lower bound's timestamp, ranges in a message are always encoded in ascending order, and ranges never overlap.

* The `idPrefix` parameter's size is encoded in `length`, and can be between `0` and `idSize` bytes, inclusive. Efficient implementations will use the shortest possible prefix needed to separate the first record of this range from the last record of the previous range. If these records' timestamps differ, then the length should be 0, otherwise it should be the byte-length of their common prefix plus 1.

  If the `idPrefix` length is less than `idSize` then the omitted trailing bytes are filled with 0 bytes.

### Range

IDs are represented as byte-strings truncated to length `idSize`:

    Id := Byte{idSize}

A range consists of an upper bound, a mode, and a payload (determined by mode):

    Range := <upperBound (Bound)> <mode (Varint)> <Skip | Fingerprint | IdList | Continuation | UnsupportedProtocolVersion>

* If `mode = 0`, then payload is `Skip`, which is simply empty:

      Skip :=

* If `mode = 1`, then payload is `Fingerprint`, the SHA-256 hash of all the IDs in this range sorted ascending by `(timestamp,id)` and truncated to `idSize`:

      Fingerprint := <Id>

* If `mode = 2`, the payload is `IdList`, a variable-length list of all IDs within this range:

      IdList := <length (Varint)> <ids (Id)>*

* If `mode = 3`, then payload is `Continuation`, which is simply empty. If present, a continuation range should always be the last range in a message, and its upperBound should be "infinity".

      Continuation :=

* If `mode = 4`, then payload is `UnsupportedProtocolVersion`. This is created by the server when it is unable to handle the protocol version indicated by the protocol version byte in the initial message. The server's preferred protocol version is stored in the timestamp of the `upperBound` of this range (offset by `0x60`). If the client can support this version then its reply should be another initial message using this protocol version.

      UnsupportedProtocolVersion :=

### Message

A reconciliation message is an ordered list of ranges:

    Message := <Range>*

* Zero ranges represents an implicit `Skip` over the full universe of IDs.

The initial message is prefixed by a protocol version byte:

    InitialMessage := <protocolVersion (Byte)> <Message>

* The current protocol version is 0, represented by the byte `0x60`. Protocol version 1 will be `0x61`, and so forth.



## Analysis

If you are searching for a single record in an ordered array, binary search allows you to find the record with a logarithmic number of operations. This is because each operation cuts the search space in half. So, searching a list of 1 million items will take about 20 operations:

    log(1e6)/log(2) = 19.9316

Range-based reconciliation uses a similar principle. Each communication divides items into their own buckets and compares the fingerprints of the buckets. If we always split into 2 buckets, and there was exactly 1 difference, we would cut the search-space in half on each operation.

For effective performance, negentropy requires minimising the number of "round-trips" between the two sides. A sync that takes 20 back-and-forth communications to determine a single difference would take unacceptably long. Fortunately we can expend a small amount of extra bandwidth by splitting our ranges into more than 2 ranges. This has the effect of increasing the base of the logarithm. For example, if we split it into 16 pieces instead:

    log(1e6)/log(16) = 4.98289

Additionally, each direction of the protocol's communication can result in a split, so since we are measuring round-trips, we divide this by two:

    log(1e6)/log(16)/2 = 2.49145

This means that in general, three round-trip communications will be required to synchronise two sets of 1 million records that differ by 1 record. With an `idSize` of 16, each communication will consume `16*16 + overhead` -- roughly 300 bytes. So total bandwidth in one direction would be about 900 bytes and the other direction about 600 bytes.

What if they differ by multiple records? Because communication is batched, the splitting of multiple differing ranges can happen in parallel. So, the number of round-trips will not be affected (assuming that every message can be delivered in exactly one packet transmission, independent of size, which is of course not entirely true on real networks).

The amount of bandwidth consumed will grow linearly with the number of differences, but this would of course be true even assuming a perfect synchronisation method that had no overhead other than transmitting the differing records.




## Implementations

This section lists all the currently-known negentropy implementations. If you know of a new one, please let us know by [opening an issue](https://github.com/hoytech/negentropy/issues/new).

| **Language** | **Author** | **Status** | **Storage** |
|--------------|------------|-------------|
| [C++](cpp/README.md) | reference | Stable | Vector, BTreeMem, BTreeLMDB |
| [Javascript](js/README.md) | reference | Stable | Vector |
| [Rust](https://github.com/yukibtc/rust-negentropy) | Yuki Kishimoto | Stable | Vector |

### Testing

There is a conformance test-suite available in the `testing` directory.

In order to test a new language you must create a "harness", which is a basic stdio line-based adapter for your implementation. See the [test/cpp/harness.cpp](test/cpp/harness.cpp) and [test/js/harness.js](test/js/harness.js) files for examples. Next, edit the file `test/Utils.pm` and configure how your harness should be invoked.

Harnesses may require some setup before they are usable. For example, you need to run `make` within the `test/cpp/` directory.

Once setup, you should be able to run something like `perl test.pl cpp,js` from the `test/` directory. This will perform the following:

* For each combination of language run the follwing fuzz tests:
  * Client has all records
  * Server has all records
  * Both have all records
  * Client is missing some and server is missing some

The test is repeated using each language as both the client and the server.

Afterwards, a different fuzz test is run for each language in isolation, and the exact protocol output is stored for each language. These are compared to ensure they are byte-wise identical.

Finally, a protocol upgrade test is run for each language to ensure that when run as a server it correctly indicates to the client when it cannot handle a specific protocol version.

* For the Rust implementation, check out its repo in the same directory as the `negentropy` repo, build the `harness` commands for both C++ and Rust, and then inside `negentropy/test/` directory running `perl test.pl cpp,rust`




## Adversarial Input

Negentropy is intended to be secure against a user who can insert arbitrary records into the DBs of one or both parties of the protocol. Secure in this sense means that the reconciliation will succeed without missing any records. A previous version of negentropy used xor to combine hashes, meaning it was only suitable for trusted input, or application-level countermeasures should have been taken (as described in Meyer's master's thesis).

Note that the threat model does not include one of the sides of the parties in the protocol themselves being malicious (except that implementations should obviously be secure against implementation defects such as buffer overflows, etc). In this case, a malicious party could always pretend to have or not have records in the first place, meaning that corrupting the protocol doesn't gain them anything additional, except possibly wasting the other side's CPU/bandwidth/memory resources.


## Pre-computing Fingerprints

The reference implementations work by storing the IDs (sorted by timestamp and then id) in a contiguous memory buffer. This means that a single call to the SHA-256 hash function is sufficient for computing the fingerprint of any sub-range. The result is that fingerprints can be computed rapidly, especially when using dedicated CPU instructions. On my laptop, computing a fingerprint for 1 million records (with ID size 16) takes approximately 1 millisecond.

However, for extremely large DBs, and also in cases where a significant amount of IO would be required to load all the IDs into a contiguous memory buffer, it would be helpful to be able to pre-compute fingerprints. As Meyer points out, storing a fingerprint for every possible sub-range would be infeasible, and SHA-256 cannot be computed incrementally, which reduces our options.

Fortunately, because of the flexibility of the range-based approach, we can still pre-compute fingerprints for various common ranges, and then direct the protocol to prefer using those ranges. For example, suppose we wanted to send a large range that we haven't pre-computed, but we have several pre-computed sub-ranges within this range. The protocol functions perfectly well if we instead sent multiple adjacent range fingerprints instead of re-hashing the entire range, at the expense of some extra bandwidth usage.

For example, suppose we pre-computed the fingerprints for all records within each day. In this case, when performing a reconciliation, these daily ranges would always be preferred. Depending on how many records are stored each day, perhaps hourly fingerprints would be pre-computed in addition. When reconciling differing hours, it would fall back to loading the IDs into contiguous memory regions.

This approach will work best if all participants in the system agree on globally-consistent daily/hourly boundaries. Because most often records that are being inserted or modified are for the current day, it may not be beneficial to pre-compute the current day's fingerprint as it would be changing too frequently. But a corollary to this is that re-computing the fingerprints of old/archival ranges would be infrequent.


## Use-Cases

* [Bandwidth-efficient Nostr event syncing](https://github.com/hoytech/strfry/blob/next/docs/negentropy.md)


## Copyright

(C) 2023 Doug Hoyte

Protocol specification, reference implementations, and tests are MIT licensed.
