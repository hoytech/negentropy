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
        negentropy::Item item(timestamp, std::string(32, '\x01'));
        btree.insert(item);
        addedTimestamps.insert(timestamp);
        doVerify(btree);
    }

    void erase(negentropy::storage::btree::BTreeCore &btree, uint64_t timestamp){
        negentropy::Item item(timestamp, std::string(32, '\x01'));
        btree.erase(item);
        addedTimestamps.erase(timestamp);
    negentropy::storage::btree::dump(btree);
        doVerify(btree);
    }

    void doVerify(negentropy::storage::btree::BTreeCore &btree) {
        negentropy::storage::btree::verify(btree);
        auto iter = addedTimestamps.begin();

    negentropy::storage::btree::dump(btree);
        if (btree.size() != addedTimestamps.size()) throw negentropy::err("verify size mismatch");
        btree.iterate(0, btree.size(), [&](const auto &item, size_t i) {
            if (item.timestamp != *iter) throw negentropy::err("verify element mismatch");
            iter = std::next(iter);
            return true;
        });
    }
};





int main() {
    Verifier v;
    negentropy::storage::BTreeMem btree;

    srand(0);
    v.insert(btree, 0);
    v.insert(btree, 1);
    negentropy::storage::btree::dump(btree);

    for (uint64_t i = 2; i < 10; i++) v.insert(btree, i);

    v.erase(btree, 0);
    v.erase(btree, 3);
    v.erase(btree, 4);
    v.erase(btree, 5);
    v.erase(btree, 6);
    v.erase(btree, 7);
    negentropy::storage::btree::dump(btree);
    v.erase(btree, 8);
    v.erase(btree, 9);
    v.erase(btree, 1);
    v.erase(btree, 2);
    //for (uint64_t i = 4; i < 5; i++) v.erase(btree, i);
    negentropy::storage::btree::dump(btree);







    //std::cout << "SIZEOF LEAF: " << sizeof(negentropy::storage::LeafNode) << std::endl;
    //std::cout << "SIZEOF INTERIOR: " << sizeof(negentropy::storage::InteriorNode) << std::endl;


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



/*
    negentropy::storage::BTreeMem btree;

    auto add = [&](uint64_t timestamp){
        negentropy::Item item(timestamp, std::string(32, '\x01'));
        btree.insert(item);
        negentropy::storage::btree::verify(btree);
    };

    srand(0);
    for (int i = 0; i < 1000; i++) add(rand());
    negentropy::storage::btree::dump(btree);
    */

/*
    std::cout << "-----------------" << std::endl;
    std::cout << "SIZE = " << btree.size() << std::endl;

    for (size_t i = 0; i < btree.size(); i++) {
        std::cout << "GI " << i << "  = " << btree.getItem(i).timestamp << std::endl;
    }

    btree.iterate(5, 8, [&](const auto &item, size_t i) {
        std::cout << "II = " << item.timestamp << " (" << i << ")" << std::endl;
        return true;
    });


    std::cout << "FLB: " << btree.findLowerBound(negentropy::Bound(1031)) << std::endl;


    std::cout << "FP: " << hoytech::to_hex(btree.fingerprint(0, btree.size()).sv()) << std::endl;
    */



    return 0;
}
