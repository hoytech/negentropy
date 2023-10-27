#pragma once

#include <hoytech/hex.h> //FIXME

#include "negentropy.h"


namespace negentropy { namespace storage {

using err = std::runtime_error;

//const size_t MAX_ITEMS_PER_LEAF_NODE = 64;
//const size_t MAX_CHILDREN_PER_INTERIOR_NODE = 32;
const size_t MAX_ITEMS_PER_LEAF_NODE = 4;
const size_t MAX_CHILDREN_PER_INTERIOR_NODE = 3;

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

struct ChildSplitter {
    Item splitKey; // max item in child
    uint64_t childNodeId;
};

inline bool operator<(const ChildSplitter &a, const ChildSplitter &b) {
    return a.splitKey < b.splitKey;
};

struct InteriorNode {
    uint32_t type;
    uint32_t numChildren;

    Accumulator accum;
    uint64_t numItems;

    ChildSplitter children[MAX_CHILDREN_PER_INTERIOR_NODE];

    InteriorNode() {
        memset((void*)this, '\0', sizeof(*this));
        type = 2;
    }
};

struct NodePtr {
    void *p;
    uint64_t nodeId;

    bool exists() {
        return p != nullptr;
    }

    bool isLeaf() {
        return *(uint32_t*)p == 1;
    }

    LeafNode *leaf() {
        if (!isLeaf()) throw err("not a leaf node");
        return (LeafNode *)p;
    }

    InteriorNode *interior() {
        if (isLeaf()) throw err("not an interior node");
        return (InteriorNode *)p;
    }

    Item maxKey() {
        if (isLeaf()) {
            auto *p = leaf();
            return p->items[p->numItems - 1];
        } else {
            auto *p = interior();
            return p->children[p->numChildren].splitKey;
        }
    }
};


struct BTree /*: StorageBase*/ {
    //// Node Storage

    std::unordered_map<uint64_t, void*> _nodeStorageMap;
    uint64_t _rootNodeId = 0; // 0 means no root
    uint64_t _nextNodeId = 1;

    NodePtr getNodeRead(uint64_t nodeId) {
        auto res = _nodeStorageMap.find(nodeId);
        if (res == _nodeStorageMap.end()) return NodePtr{nullptr, 0};
        return NodePtr{res->second, nodeId};
    }

    NodePtr getNodeWrite(uint64_t nodeId) {
        return getNodeRead(nodeId);
    }

    NodePtr makeNewLeaf() {
        uint64_t nodeId = _nextNodeId++;
        void *m = new LeafNode;
        _nodeStorageMap[nodeId] = m;
        return NodePtr{m, nodeId};
    }

    NodePtr makeNewInterior() {
        uint64_t nodeId = _nextNodeId++;
        void *m = new InteriorNode;
        _nodeStorageMap[nodeId] = m;
        return NodePtr{m, nodeId};
    }

    void deleteNode(uint64_t nodeId) {
        auto prev = getNodeRead(nodeId);

        if (prev.exists()) {
            if (prev.isLeaf()) delete prev.leaf();
            else delete prev.interior();
        }
    }

    uint64_t getRootNodeId() {
        return _rootNodeId;
    }

    void setRootNodeId(uint64_t newRootNodeId) {
        _rootNodeId = newRootNodeId;
    }


    //// Insertion

    void insert(const Item &newItem) {
        // Make root leaf in case it doesn't exist

        auto rootNodeId = getRootNodeId();

        if (!rootNodeId) {
            auto newLeafPtr = makeNewLeaf();
            auto &newLeaf = *newLeafPtr.leaf();

            newLeaf.items[0] = newItem;
            newLeaf.numItems++;
            newLeaf.accum.add(newItem.id);

            setRootNodeId(newLeafPtr.nodeId);
            return;
        }

        // Traverse interior nodes until we find a leaf

        struct Breadcrumb {
            size_t index;
            NodePtr node;
        };

        std::vector<Breadcrumb> breadcrumbs;
        auto currNode = getNodeRead(rootNodeId);

        while (!currNode.isLeaf()) {
            const auto &interior = *currNode.interior();

            for (size_t i = 0; i < interior.numChildren; i++) {
                if (i == (interior.numChildren - 1) || newItem <= interior.children[i].splitKey) {
                    breadcrumbs.push_back({i, currNode});
                    currNode = getNodeRead(interior.children[i].childNodeId);
                    break;
                }
            }
        }

        // Update leaf node

        std::optional<NodePtr> newInterior;

        if (currNode.leaf()->numItems < MAX_ITEMS_PER_LEAF_NODE) {
            // Happy path: Leaf has room for new item

            auto &leaf = *getNodeWrite(currNode.nodeId).leaf();

            leaf.items[leaf.numItems] = newItem;
            std::inplace_merge(leaf.items, leaf.items + leaf.numItems, leaf.items + leaf.numItems + 1);

            leaf.numItems++;
            leaf.accum.add(newItem.id);
        } else {
            // Leaf is full: Split it into 2

            auto &left = *getNodeWrite(currNode.nodeId).leaf();
            auto rightPtr = makeNewLeaf();
            auto &right = *rightPtr.leaf();

            Item tmpItems[MAX_ITEMS_PER_LEAF_NODE + 1];
            for (size_t i = 0; i < MAX_ITEMS_PER_LEAF_NODE; i++) tmpItems[i] = left.items[i];
            tmpItems[MAX_ITEMS_PER_LEAF_NODE] = newItem;
            std::inplace_merge(tmpItems, tmpItems + MAX_ITEMS_PER_LEAF_NODE, tmpItems + MAX_ITEMS_PER_LEAF_NODE + 1);

            left.accum.setToZero();
            left.numItems = (MAX_ITEMS_PER_LEAF_NODE / 2) + 1;
            right.numItems = MAX_ITEMS_PER_LEAF_NODE / 2;

            for (size_t i = 0; i < left.numItems; i++) {
                left.items[i] = tmpItems[i];
                left.accum.add(left.items[i]);
            }

            for (size_t i = 0; i < right.numItems; i++) {
                right.items[i] = tmpItems[left.numItems + i];
                right.accum.add(right.items[i]);
            }

            right.nextLeaf = left.nextLeaf;
            left.nextLeaf = rightPtr.nodeId;

            // Create interior

            newInterior = makeNewInterior();
            auto &interior = *newInterior->interior();
            interior.numChildren = 2;

            interior.accum.add(left.accum);
            interior.accum.add(right.accum);
            interior.numItems = left.numItems + right.numItems;

            interior.children[0].childNodeId = currNode.nodeId;
            interior.children[0].splitKey = left.items[left.numItems - 1];
            interior.children[1].childNodeId = rightPtr.nodeId;
            interior.children[1].splitKey = right.items[right.numItems - 1];
        }

        // Split upwards

        while (breadcrumbs.size()) {
            auto bc = breadcrumbs.back();
            breadcrumbs.pop_back();

            auto &oldInterior = *getNodeWrite(bc.node.nodeId).interior();

            if (newInterior) {
                throw err("not impl");
            /*
                if (oldInterior.numChildren < MAX_CHILDREN_PER_INTERIOR_NODE) {
                    // Happy path: No splitting needed

                    {
                        auto &left = *getNodeWrite(oldInterior. bc.index
                    }

                    oldInterior.children[oldInterior.numChildren] = { };

                    oldInterior.numItems++;
                    oldInterior.accum.add(newItem.id);
                } else {
                    throw err("not impl");
                }
                */
            } else {
                oldInterior.numItems++;
                oldInterior.accum.add(newItem.id);
            }
        }

        if (newInterior) {
            setRootNodeId(newInterior->nodeId);
        }
    }

    void walk() {
        walk(getRootNodeId(), 0);
    }

    void walk(uint64_t nodeId, int depth) {
        auto currNode = getNodeRead(nodeId);
        std::string indent(depth * 4, ' ');

        if (currNode.isLeaf()) {
            const auto &leaf = *currNode.leaf();

            std::cout << indent << "LEAF id=" << nodeId << " numItems=" << leaf.numItems << " accum=" << hoytech::to_hex(leaf.accum.sv()) << std::endl;

            for (size_t i = 0; i < leaf.numItems; i++) {
                std::cout << indent << "  item: " << leaf.items[i].timestamp << "," << hoytech::to_hex(leaf.items[i].getId()) << std::endl;
            }
        } else {
            const auto &interior = *currNode.interior();

            std::cout << indent << "INTERIOR id=" << nodeId << " numItems=" << interior.numItems << " accum=" << hoytech::to_hex(interior.accum.sv()) << std::endl;

            for (size_t i = 0; i < interior.numChildren; i++) {
                walk(interior.children[i].childNodeId, depth + 1);
            }
        }
    }
};


}}
