#include <iostream>
#include <cstdlib>

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
        btree.verify();
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


    btree.walk();
    btree.verify();


    //srand(0);
    //for (int i = 0; i < 1000; i++) add(rand());



    std::cout << "-----------------" << std::endl;
    std::cout << "SIZE = " << btree.size() << std::endl;

    for (size_t i = 0; i < btree.size(); i++) {
        std::cout << "GI = " << btree.getItem(i).timestamp << std::endl;
    }

    btree.iterate(5, 8, [&](const auto &item, size_t i) {
        std::cout << "II = " << item.timestamp << " (" << i << ")" << std::endl;
        return true;
    });
/*
    */

    return 0;
}
