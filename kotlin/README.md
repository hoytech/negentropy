# Negentropy Kotlin Implementation

The library shouldn't need any dependencies and can run as Java or Native.

## Storage

First, you need to create a storage instance. Currently only `Vector` is implemented. 
Add all the items in your collection with `insert(timestamp, hash)` and call `seal()`

    StorageVector().apply {
        insert(1678011277, "eb6b05c2e3b008592ac666594d78ed83e7b9ab30f825b9b08878128f7500008c".hexToByteArray())
        insert(1678011278, "39b916432333e069a4386917609215cc688eb99f06fed01aadc29b1b4b92d6f0".hexToByteArray())
        insert(1678011279, "abc81d58ebe3b9a87100d47f58bf15e9b1cbf62d38623f11d0f0d17179f5f3ba".hexToByteArray())

        seal()
    }

*  `timestamp` should be a unix timestamp
*  `id` should be a byte array of the event id

## Reconciliation

Create a Negentropy object:

    val ne = Negentropy(storage, 50_000)

* The second parameter (`50_000` above) is the `frameSizeLimit`. This can be omitted (or `0`) to permit unlimited-sized frames.

On the client-side, create an initial message, and then transmit it to the server, receive the response, and `reconcile` until complete (signified by returning `null` for `newMsg`):

    val msg = ne.initiate();

    while (msg !== null) {
        val response = <queryServer>(msg);
        val (newMsg, have, need) = ne.reconcile(msg);
        msg = newMsg;
        // handle have/need (there may be duplicates from previous calls to reconcile())
    }

*  The output `msg`s and the IDs in the `have`/`need` arrays are hex strings.

The server-side is similar, except it doesn't create an initial message, there are no `have`/`need` arrays, and `newMsg` will never be `null`:

    while (1) {
        val msg = <receiveMsgFromClient>();
        val reconciled = ne.reconcile(msg);
        respondToClient(reconciled.msg);
    }

* The `initiate()` and `reconcile()` methods are not suspending functions but they will take a while to process.

## Developer Setup

Make sure to have the following pre-requisites installed:
1. Java 17+
2. Android Studio or IntelliJ Idea CE

## Building

Build the app:
```bash
./gradlew clean assemble
```

## Testing
```bash
./gradlew test
```

## Running Test suite

Run assemble to generate the `.jar` for the library 

```bash
perl test.pl kotlin,js
```

