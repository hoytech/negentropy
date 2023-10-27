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


    for (size_t i = 100; i < 107; i++) {
        negentropy::Item item(i, std::string(32, '\x01'));
        btree.insert(item);
    }

    btree.walk();

    return 0;
}
