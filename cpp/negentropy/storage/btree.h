#pragma once

#include "negentropy.h"


namespace negentropy { namespace storage {

using err = std::runtime_error;

const size_t MAX_ITEMS_PER_LEAF_NODE = 64;
const size_t MAX_CHILDREN_PER_INTERIOR_NODE = 32;

// Node types: 1=Leaf, 2=Interior

struct LeafNode {
    uint32_t type;
    uint32_t numItems;

    Accumulator accum;

    uint64_t nextLeaf;
    Item items[MAX_ITEMS_PER_LEAF_NODE];

    LeafNode() {
        memset((void*)this, '\0', sizeof(*this));
        type = 1;
    }
};

struct InteriorNode {
    uint32_t type;
    uint32_t numChildren;

    Accumulator accum;
    uint64_t numItems;

    uint64_t children[MAX_CHILDREN_PER_INTERIOR_NODE];
    Item keys[MAX_CHILDREN_PER_INTERIOR_NODE - 1];

    InteriorNode() {
        memset((void*)this, '\0', sizeof(*this));
        type = 2;
    }

    /*
    size_t findChild(const Item &item) {
        auto endOfKeys = keys + size_t(numChildren - 1);
        auto lower = std::lower_bound(keys, endOfKeys, item);
    }
    */
};

struct NodePtr {
    const void *p;

    bool exists() {
        return p != nullptr;
    }

    bool isLeaf() {
        return *(uint32_t*)p == 1;
    }

    LeafNode *leaf() {
        return (LeafNode *)p;
    }

    InteriorNode *interior() {
        return (InteriorNode *)p;
    }
};


struct BTree /*: StorageBase*/ {
    std::unordered_map<uint64_t, void*> nodeStorageMap;
    uint64_t nextNodeId = 2;

    NodePtr getNode(uint64_t nodeId) {
        auto res = nodeStorageMap.find(nodeId);
        if (res == nodeStorageMap.end()) return NodePtr{nullptr};
        return NodePtr{res->second};
    }

    uint64_t saveNode(NodePtr node, uint64_t nodeId = 0) {
        void *m;

        if (node.isLeaf()) {
            m = new LeafNode;
            memcpy(m, node.p, sizeof(LeafNode));
        } else {
            m = new InteriorNode;
            memcpy(m, node.p, sizeof(InteriorNode));
        }

        if (nodeId == 0) nodeId = nextNodeId++;

        {
            auto prev = getNode(nodeId);

            if (prev.exists()) {
                if (prev.isLeaf()) delete prev.leaf();
                else delete prev.interior();
            }
        }

        nodeStorageMap[nodeId] = m;

        return nodeId;
    }

    void insert(const Item &item) {
        auto currNodeId = 1;
        auto currNode = getNode(currNodeId);

        if (!currNode.exists()) {
            LeafNode newLeaf;

            newLeaf.numItems++;
            newLeaf.accum.add(item.id);

            saveNode(NodePtr{&newLeaf}, currNodeId);
            return;
        }

        while (!currNode.isLeaf()) {
            throw err("not impl");
        }

        if (currNode.leaf()->numItems < MAX_ITEMS_PER_LEAF_NODE) {
            const LeafNode &oldLeaf = *currNode.leaf();
            LeafNode newLeaf = oldLeaf;

            size_t j = 0;
            for (size_t i = 0; i < newLeaf.numItems; i++) {
                if (oldLeaf.items[i] < item) {
                    if (item == oldLeaf.items[i]) throw err("already exists");
                    newLeaf.items[j++] = item;
                }

                newLeaf.items[j++] = oldLeaf.items[i];
            }

            newLeaf.numItems++;
            newLeaf.accum.add(item.id);
        }
    }
};


}}
