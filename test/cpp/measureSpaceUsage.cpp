#include <iostream>
#include <cstdlib>

#include <sstream>
#include <memory>

#include <hoytech/error.h>
#include <hoytech/hex.h>

#include "negentropy.h"
#include "negentropy/storage/BTreeLMDB.h"
#include "negentropy/storage/BTreeMem.h"
#include "negentropy/storage/btree/debug.h"
#include "negentropy/storage/Vector.h"






int main() {
    auto env = lmdb::env::create();
    env.set_max_dbs(64);
    env.set_mapsize(1'000'000'000ULL);
    env.open("testdb/", 0);


    negentropy::storage::BTreeLMDB btree;

    {
        auto txn = lmdb::txn::begin(env);
        btree.setup(txn, "negentropy");
        txn.commit();
    }


    {
        auto txn = lmdb::txn::begin(env);

        btree.withWriteTxn(txn, [&]{
            auto add = [&](uint64_t timestamp){
                negentropy::Item item(timestamp, std::string(32, '\x01'));
                btree.insert(item);
            };

            for (size_t i = 1; i < 100'000; i++) add(i);
        });

        txn.commit();
    }

    {
        auto txn = lmdb::txn::begin(env, 0, MDB_RDONLY);

        auto cursor = lmdb::cursor::open(txn, btree.dbi);

        std::string_view key, val;
        size_t minStart = negentropy::MAX_U64;
        size_t maxEnd = 0;

        if (cursor.get(key, val, MDB_FIRST)) {
            do {
                size_t ptrStart = (size_t)val.data();
                size_t ptrEnd = ptrStart + sizeof(negentropy::storage::btree::Node);
                if (ptrStart < minStart) minStart = ptrStart;
                if (ptrEnd > maxEnd) maxEnd = ptrEnd;
            } while (cursor.get(key, val, MDB_NEXT));
        }

        std::cout << "data," << negentropy::storage::btree::MAX_ITEMS << "," << sizeof(negentropy::storage::btree::Node) << "," << (maxEnd - minStart) << std::endl;
    }

    return 0;
}
