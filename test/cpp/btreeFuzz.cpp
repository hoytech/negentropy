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




struct Verifier {
    bool isLMDB;

    std::set<uint64_t> addedTimestamps;

    Verifier(bool isLMDB) : isLMDB(isLMDB) {}

    void insert(negentropy::storage::btree::BTreeCore &btree, uint64_t timestamp){
        negentropy::Item item(timestamp, std::string(32, (unsigned char)(timestamp % 256)));
        btree.insertItem(item);
        addedTimestamps.insert(timestamp);
        doVerify(btree);
    }

    void erase(negentropy::storage::btree::BTreeCore &btree, uint64_t timestamp){
        negentropy::Item item(timestamp, std::string(32, (unsigned char)(timestamp % 256)));
        btree.eraseItem(item);
        addedTimestamps.erase(timestamp);
        doVerify(btree);
    }

    void doVerify(negentropy::storage::btree::BTreeCore &btree) {
        try {
            negentropy::storage::btree::verify(btree, isLMDB);
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





void doFuzz(negentropy::storage::btree::BTreeCore &btree, Verifier &v) {
    if (btree.size() != 0) throw negentropy::err("expected empty tree");


    // Verify return values

    if (!btree.insert(100, std::string(32, '\x01'))) throw negentropy::err("didn't insert element?");
    if (btree.insert(100, std::string(32, '\x01'))) throw negentropy::err("double inserted element?");
    if (!btree.erase(100, std::string(32, '\x01'))) throw negentropy::err("didn't erase element?");
    if (btree.erase(100, std::string(32, '\x01'))) throw negentropy::err("erased non-existing element?");


    // Fuzz test: Insertion phase

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

    // Fuzz test: Removal phase

    std::cout << "REMOVING ALL" << std::endl;

    while (btree.size()) {
        auto it = v.addedTimestamps.begin();
        std::advance(it, rand() % v.addedTimestamps.size());
        auto timestamp = *it;

        std::cout << "DEL " << timestamp << " size = " << btree.size() << std::endl;
        v.erase(btree, *it);
    }
}



int main() {
    std::cout << "SIZEOF NODE: " << sizeof(negentropy::storage::Node) << std::endl;


    srand(0);


    if (::getenv("NE_FUZZ_LMDB")) {
        system("mkdir -p testdb/");
        system("rm -f testdb/*");

        auto env = lmdb::env::create();
        env.set_max_dbs(64);
        env.set_mapsize(1'000'000'000ULL);
        env.open("testdb/", 0);

        auto txn = lmdb::txn::begin(env);
        auto btreeDbi = negentropy::storage::BTreeLMDB::setupDB(txn, "test-data");

        negentropy::storage::BTreeLMDB btree(txn, btreeDbi, 0);

        Verifier v(true);
        doFuzz(btree, v);

        btree.flush();
        txn.commit();
    } else {
        Verifier v(false);
        negentropy::storage::BTreeMem btree;
        doFuzz(btree, v);
    }


    std::cout << "OK" << std::endl;

    return 0;
}
