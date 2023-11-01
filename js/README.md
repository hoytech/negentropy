# Negentropy Javascript Implementation

The library is contained in a single javascript file. It shouldn't need any dependencies, in either a browser or node.js:

    const Negentropy = require('Negentropy.js');

## Storage

First, you need to create a storage instance. Currently only `Vector` is implemented:

    let storage = new NegentropyStorageVector();

Next, add all the items in your collection, and `seal()`:

    for (let item of myItems) {
        storage.insert(timestamp, id);
    }

    ne.seal();

*  `timestamp` should be a JS `Number`
*  `id` should be a hex string, `Uint8Array`, or node.js `Buffer`

## Reconciliation

Create a Negentropy object:

    let ne = new Negentropy(storage, 50_000);

* The second parameter (`50'000` above) is the `frameSizeLimit`. This can be omitted (or `0`) to permit unlimited-sized frames.

On the client-side, create an initial message, and then transmit it to the server, receive the response, and `reconcile` until complete (signified by returning `null` for `newMsg`):

    let msg = await ne.initiate();

    while (msg.length !== null) {
        let response = queryServer(msg);
        let [newMsg, have, need] = await ne.reconcile(msg);
        msg = newMsg;
        // handle have/need
    }

*  The output `msg`s and the IDs in `have`/`need` are hex strings, but you can set `ne.wantUint8ArrayOutput = true` if you want `Uint8Array`s instead.

The server-side is similar, except it doesn't create an initial message, there are no `have`/`need` arrays, and `newMsg` will never be `null`:

    while (1) {
        let msg = receiveMsgFromClient();
        let [newMsg] = await ne.reconcile(msg);
        respondToClient(newMsg);
    }

* The `initiate()` and `reconcile()` methods are async because the `crypto.subtle.digest()` browser API is async.
* Timestamp values greater than `Number.MAX_VALUE` will currently cause failures.
