#include <iostream>
#include <sstream>
#include <memory>

#include <hoytech/error.h>
#include <hoytech/hex.h>

#include "negentropy.h"
#include "negentropy/storage/btree.h"






int main() {
    //std::cout << "SIZEOF LEAF: " << sizeof(negentropy::storage::LeafNode) << std::endl;
    //std::cout << "SIZEOF INTERIOR: " << sizeof(negentropy::storage::InteriorNode) << std::endl;

    negentropy::storage::BTree btree;

    auto add = [&](uint64_t timestamp){
        negentropy::Item item(timestamp, std::string(32, '\x01'));
        btree.insert(item);
    };

    for (size_t i = 100; i < 114; i++) add(i * 2);

    add(99);
    add(213);

    btree.walk();

    return 0;
}
