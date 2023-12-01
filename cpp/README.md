# Negentropy C++ Implementation

The C++ implementation is header-only and the only required dependency is OpenSSL (for SHA-256). The main `Negentropy` class can be imported with the following:

    #include "negentropy.h"

## Storage

First, you need to create a storage instance. Currently the following are available:

### negentropy::storage::Vector

All the elements are put into a contiguous vector in memory, and are then sorted. This can be useful for syncing the results of a dynamic query, since it can be constructed rapidly and consumes a minimal amount of memory. However, modifying it by adding or removing elements is expensive (linear in the size of the data-set).

    #include "negentropy/storage/Vector.h"

To use `Vector`, add all your items with `addItem` and then call `seal`:

    negentropy::storage::Vector storage;

    for (const auto &item : myItems) {
        storage.addItem(item.timestamp(), item.id());
    }

    storage.seal();

After sealing, no more items can be added.

### negentropy::storage::BTreeMem

Keeps the elements in an in-memory B+Tree. Computing fingerprints, adding, and removing elements are all logarithmic in data-set size. However, the elements will not be persisted to disk, and the data-structure is not thread-safe.

    #include "negentropy/storage/BTreeMem.h"

To use `BTreeMem`, items can be added in the same way as with `Vector`, however sealing is not necessary (although is supported -- it is a no-op):

    negentropy::storage::BTreeMem storage;

    for (const auto &item : myItems) {
        storage.addItem(item.timestamp(), item.id());
    }

More items can be added at any time, and items can be removed with `eraseItem`:

    storage.addItem(timestamp, id);
    storage.eraseItem(timestamp, id);


### negentropy::storage::BTreeLMDB

Uses the same implementation as BTreeMem, except that it uses [LMDB](http://lmdb.tech/) to save the data-set to persistent storage. Because the database is memory mapped, its read-performance is identical to the "in-memory" version (it is also in-memory, the memory just happens to reside in the page cache). Additionally, the tree can be concurrently accessed by multiple threads/processes using ACID transactions.

    #include "negentropy/storage/BTreeLMDB.h"

First create an LMDB environment and open the DB. Next, call `setup` on the `storage` instance inside of a write transaction. The `"test-data"` argument is the LMDB DBI table name you want to use.

    negentropy::storage::BTreeLMDB storage;

    auto env = lmdb::env::create();
    env.set_max_dbs(64);
    env.open("testdb/", 0);

    {
        auto txn = lmdb::txn::begin(env);
        storage.setup(txn, "test-data");
        txn.commit();
    }

Adding or remove items should be done with write transactions also. Use the `withWriteTxn` method to wrap the operations:

    {
        auto txn = lmdb::txn::begin(env);

        storage.withWriteTxn(txn, 300, [&]{
            storage.addItem(timestamp, id);
        });
    }

* Operations inside `withWriteTxn` are performed in a child transaction, so any exceptions will rollback all changes performed within the `withWriteTxn` block.
* The second parameter (`300` in the above example) is the `treeId`. This allows many different trees to co-exist in the same DB.


## Reconciliation

Reconciliation works mostly the same for all storage types. First create a `Negentropy` object:

    auto ne = Negentropy(storage, 50'000);

* The object is templated on the storage type, but can often be auto-deduced (as above).
* The second parameter (`50'000` above) is the `frameSizeLimit`. This can be omitted (or `0`) to permit unlimited-sized frames.

On the client-side, create an initial message, and then transmit it to the server, receive the response, and `reconcile` until complete:

    std::string msg = ne.initiate();

    while (true) {
        std::string response = queryServer(msg);

        std::vector<std::string> have, need;
        std::optional<std::string> newMsg = ne.reconcile(response, have, need);

        // handle have/need

        if (!newMsg) break; // done
        else std::swap(msg, *newMsg);
    }

In each loop iteration, `have` contains IDs that the client has that the server doesn't, and `need` contains IDs that the server has that the client doesn't.

The server-side is similar, except it doesn't create an initial message, there are no `have`/`need` arrays, and it doesn't return an optional (servers must always reply to a request):

    while (true) {
        std::string msg = receiveMsgFromClient();
        std::string response = ne.reconcile(msg);
        respondToClient(response);
    }

Reconciliation with `BTreeLMDB` requires that calls to `initiate` and/or `reconcile` are wrapped with a `withReadTxn` (or `withWriteTxn`) like so:

    std::string response;

    storage.withReadTxn(txn, 300, [&]{
        response = ne.reconcile(msg);
    });



## BTree Implementation

The BTree implementation is technically a B+Tree since all records are stored in the leaves. Every node has `next` and `prev` pointers that point to the neighbour nodes on the same level, which allows efficient iteration.

Each node has an accumulator that contains the sum of the IDs of all nodes below it, allowing fingerprints to be computed in logarithmic time relative to the number of tree leaves.

Nodes will split and rebalance themselves as necessary to keep the tree balanced. This is a major advantage over rigid data-structures like merkle-search trees and prolly trees, which are only probabilisticly balanced.

If records are always inserted to the "right" of the tree, nodes will be fully packed. Otherwise, the tree attempts to keep them 50% full.
