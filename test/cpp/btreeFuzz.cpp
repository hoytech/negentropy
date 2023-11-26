#include <iostream>
#include <cstdlib>

#include <sstream>
#include <memory>
#include <set>

#include <hoytech/error.h>
#include <hoytech/hex.h>

#include "negentropy.h"
#include "negentropy/storage/BTreeLMDB.h"
#include "negentropy/storage/BTreeMem.h"
#include "negentropy/storage/btree/debug.h"
#include "negentropy/storage/Vector.h"




struct Verifier {
    std::set<uint64_t> addedTimestamps;

    void insert(negentropy::storage::btree::BTreeCore &btree, uint64_t timestamp){
        negentropy::Item item(timestamp, std::string(32, (unsigned char)(timestamp % 256)));
        btree.insert(item);
        addedTimestamps.insert(timestamp);
        doVerify(btree);
    }

    void erase(negentropy::storage::btree::BTreeCore &btree, uint64_t timestamp){
        negentropy::Item item(timestamp, std::string(32, (unsigned char)(timestamp % 256)));
        btree.erase(item);
        addedTimestamps.erase(timestamp);
        doVerify(btree);
    }

    void doVerify(negentropy::storage::btree::BTreeCore &btree) {
        try {
            negentropy::storage::btree::verify(btree, true);
        } catch (...) {
            std::cout << "TREE FAILED INVARIANTS:" << std::endl;
            negentropy::storage::btree::dump(btree);
            throw;
        }

        if (btree.size() != addedTimestamps.size()) throw negentropy::err("verify size mismatch");
        auto iter = addedTimestamps.begin();

        btree.iterate(0, btree.size(), [&](const auto &item, size_t i) {
            if (item.timestamp != *iter) throw negentropy::err("verify element mismatch");
            iter = std::next(iter);
            return true;
        });
    }
};





int main() {
    std::cout << "SIZEOF NODE: " << sizeof(negentropy::storage::Node) << std::endl;


    srand(0);

    Verifier v;
    negentropy::storage::BTreeMem btree;

    while (btree.size() < 5000) {
        if (rand() % 3 <= 1) {
            int timestamp;

            do {
                timestamp = rand();
            } while (v.addedTimestamps.contains(timestamp));

            std::cout << "INSERT " << timestamp << " size = " << btree.size() << std::endl;
            v.insert(btree, timestamp);
        } else if (v.addedTimestamps.size()) {
            auto it = v.addedTimestamps.begin();
            std::advance(it, rand() % v.addedTimestamps.size());

            std::cout << "DEL " << (*it) << std::endl;
            v.erase(btree, *it);
        }
    }

    std::cout << "REMOVING ALL" << std::endl;

    while (btree.size()) {
        auto it = v.addedTimestamps.begin();
        std::advance(it, rand() % v.addedTimestamps.size());
        auto timestamp = *it;

        std::cout << "DEL " << timestamp << " size = " << btree.size() << std::endl;
        v.erase(btree, *it);
    }


/*
    auto env = lmdb::env::create();
    env.set_max_dbs(64);
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
                negentropy::storage::btree::verify(btree);
            };

            for (size_t i = 100; i < 114; i++) add(i * 10);

            add(99);
            add(1081);
            add(1082);
            add(1083);
            add(1084);
            add(1085);
            add(89);
            add(88);
            add(87);


            negentropy::storage::btree::dump(btree);
        });

        txn.commit();
    }

    {
        auto txn = lmdb::txn::begin(env, 0, MDB_RDONLY);

        btree.withReadTxn(txn, [&]{
            negentropy::storage::btree::dump(btree);
        });
    }
    */


    return 0;
}
