#pragma once

#include <hoytech/hex.h> //FIXME

#include "negentropy.h"


namespace negentropy { namespace storage {

using err = std::runtime_error;

const size_t MIN_ITEMS = 1;
const size_t MAX_ITEMS = 4;

struct Key {
    Item item;
    uint64_t nodeId;
};

inline bool operator<(const Key &a, const Key &b) {
    return a.item < b.item;
};

struct Node {
    uint64_t nextLeaf;

    uint64_t numItems;
    Key items[MAX_ITEMS + 1];

    Accumulator accum;
    uint64_t accumCount;

    Node() {
        memset((void*)this, '\0', sizeof(*this));
    }
};

struct NodePtr {
    Node *p;
    uint64_t nodeId;

    bool exists() {
        return p != nullptr;
    }

    Node &get() {
        return *p;
    }
};


struct BTree /*: StorageBase*/ {
    //// Node Storage

    std::unordered_map<uint64_t, Node> _nodeStorageMap;
    uint64_t _rootNodeId = 0; // 0 means no root
    uint64_t _nextNodeId = 1;

    NodePtr getNodeRead(uint64_t nodeId) {
        if (nodeId == 0) return {nullptr, 0};
        auto res = _nodeStorageMap.find(nodeId);
        if (res == _nodeStorageMap.end()) return NodePtr{nullptr, 0};
        return NodePtr{&res->second, nodeId};
    }

    NodePtr getNodeWrite(uint64_t nodeId) {
        return getNodeRead(nodeId);
    }

    NodePtr makeNode() {
        uint64_t nodeId = _nextNodeId++;
        _nodeStorageMap.try_emplace(nodeId);
        return getNodeRead(nodeId);
    }

    void deleteNode(uint64_t nodeId) {
        _nodeStorageMap.erase(nodeId);
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
            auto newNodePtr = makeNode();
            auto &newNode = newNodePtr.get();

            newNode.items[0].item = newItem;
            newNode.numItems++;
            newNode.accum.add(newItem.id);
            newNode.accumCount = 1;

            setRootNodeId(newNodePtr.nodeId);
            return;
        }


        // Traverse interior nodes, leaving breadcrumbs along the way

        struct Breadcrumb {
            size_t index;
            NodePtr nodePtr;
        };

        std::vector<Breadcrumb> breadcrumbs;

        {
            auto foundNode = getNodeRead(rootNodeId);

            while (foundNode.nodeId) {
                const auto &node = foundNode.get();
                size_t index = node.numItems - 1;

                if (node.numItems > 1) {
                    for (size_t i = 1; i < node.numItems + 1; i++) {
                        if (i == node.numItems + 1 || newItem < node.items[i].item) {
                            index = i - 1;
                            break;
                        }
                    }
                }

                if (newItem == node.items[index].item) throw err("already inserted");

                breadcrumbs.push_back({index, foundNode});
                foundNode = getNodeRead(node.items[index].nodeId);
            }
        }

        // Follow breadcrumbs back to root

        Key newKey = { newItem, 0 };
        bool needsMerge = true;

        while (breadcrumbs.size()) {
            auto crumb = breadcrumbs.back();
            breadcrumbs.pop_back();

            if (!needsMerge) {
                auto &node = getNodeWrite(crumb.nodePtr.nodeId).get();
                node.accum.add(newItem.id);
                node.accumCount++;
            } else if (crumb.nodePtr.get().numItems < MAX_ITEMS) {
                // Happy path: Node has room for new item
                auto &node = getNodeWrite(crumb.nodePtr.nodeId).get();

                node.items[node.numItems] = newKey;
                std::inplace_merge(node.items, node.items + node.numItems, node.items + node.numItems + 1);
                node.numItems++;

                node.accum.add(newItem.id);
                node.accumCount++;

                needsMerge = false;
            } else {
                // Node is full: Split it into 2

                auto &left = getNodeWrite(crumb.nodePtr.nodeId).get();
                auto rightPtr = makeNode();
                auto &right = rightPtr.get();

                left.items[MAX_ITEMS] = newKey;
                std::inplace_merge(left.items, left.items + MAX_ITEMS, left.items + MAX_ITEMS + 1);

                left.accum.setToZero();
                left.accumCount = 0;
                left.numItems = (MAX_ITEMS / 2) + 1;
                right.numItems = MAX_ITEMS / 2;

                for (size_t i = 0; i < left.numItems; i++) {
                    auto &left = getNodeWrite(crumb.nodePtr.nodeId).get();
                    addToAccum(left.items[i], left);
                }

                for (size_t i = 0; i < right.numItems; i++) {
                    right.items[i] = left.items[left.numItems + i];
                    addToAccum(right.items[i], right);
                }

                right.nextLeaf = left.nextLeaf;
                left.nextLeaf = rightPtr.nodeId;

                newKey = { right.items[0].item, rightPtr.nodeId };
            }

            // Update left-most key, in case item was inserted at the beginning

            {
                auto &node = getNodeWrite(crumb.nodePtr.nodeId).get();
                auto childNodePtr = getNodeRead(node.items[0].nodeId);
                if (childNodePtr.exists()) {
                    auto &childNode = childNodePtr.get();
                    node.items[0].item = childNode.items[0].item;
                }
            }
        }

        // Out of breadcrumbs but still need to merge: New level required

        if (needsMerge) {
            auto &left = getNodeRead(rootNodeId).get();
            auto &right = getNodeRead(newKey.nodeId).get();

            auto newRootPtr = makeNode();
            auto &newRoot = newRootPtr.get();
            newRoot.numItems = 2;

            newRoot.accum.add(left.accum);
            newRoot.accum.add(right.accum);
            newRoot.accumCount = left.accumCount + right.accumCount;

            newRoot.items[0] = left.items[0];
            newRoot.items[0].nodeId = rootNodeId;
            newRoot.items[1] = right.items[0];
            newRoot.items[1].nodeId = newKey.nodeId;

            setRootNodeId(newRootPtr.nodeId);
        }
    }


    //// Utils

    void addToAccum(const Key &k, Node &node) {
        if (k.nodeId == 0) {
            node.accum.add(k.item);
            node.accumCount++;
        } else {
            auto nodePtr = getNodeRead(k.nodeId);
            node.accum.add(nodePtr.get().accum);
            node.accumCount += nodePtr.get().accumCount;
        }
    }

    void traverseToOffset(size_t index, const std::function<void(Node &node, size_t index)> &cb, std::function<void(Node &)> customAccum = nullptr) {
        auto rootNodeId = getRootNodeId();
        auto &rootNode = getNodeRead(rootNodeId).get();
        if (index > rootNode.accumCount) throw err("out of range");
        return traverseToOffsetAux(index, rootNode, cb, customAccum);
    }

    void traverseToOffsetAux(size_t index, Node &node, const std::function<void(Node &node, size_t index)> &cb, std::function<void(Node &)> customAccum) {
        if (node.numItems == node.accumCount) {
            cb(node, index);
            return;
        }

        for (size_t i = 0; i < node.numItems; i++) {
            auto &child = getNodeRead(node.items[i].nodeId).get();
            if (index < child.accumCount) return traverseToOffsetAux(index, child, cb, customAccum);
            index -= child.accumCount;
            if (customAccum) customAccum(child);
        }
    }



    //// Interface

    uint64_t size() {
        auto rootNodeId = getRootNodeId();
        auto &rootNode = getNodeRead(rootNodeId).get();
        return rootNode.accumCount;
    }

    const Item &getItem(size_t index) {
        Item *out;
        traverseToOffset(index, [&](Node &node, size_t index){
            out = &node.items[index].item;
        });
        return *out;
    }

    void iterate(size_t begin, size_t end, std::function<bool(const Item &, size_t)> cb) {
        auto rootNodeId = getRootNodeId();
        auto &rootNode = getNodeRead(rootNodeId).get();

        if (begin > end) throw err("begin > end");
        if (end > rootNode.accumCount) throw err("out of range");

        size_t num = end - begin;

        traverseToOffset(begin, [&](Node &node, size_t index){
            Node *currNode = &node;
            for (size_t i = 0; i < num; i++) {
                if (!cb(currNode->items[index].item, begin + i)) return;
                index++;
                if (index >= currNode->numItems) {
                    currNode = getNodeRead(currNode->nextLeaf).p;
                    index = 0;
                }
            }
        });
    }

    size_t findLowerBound(const Bound &value) {
        auto rootNodePtr = getNodeRead(getRootNodeId());
        auto &rootNode = rootNodePtr.get();
        if (value.item <= rootNode.items[0].item) return 0;
        return findLowerBoundAux(value, rootNodePtr, 0);
    }

    size_t findLowerBoundAux(const Bound &value, NodePtr nodePtr, uint64_t numToLeft) {
        if (!nodePtr.exists()) return numToLeft + 1;

        Node &node = nodePtr.get();

        for (size_t i = 1; i < node.numItems; i++) {
            if (value.item < node.items[i].item) {
                return findLowerBoundAux(value, getNodeRead(node.items[i - 1].nodeId), numToLeft);
            } else {
                if (node.items[i - 1].nodeId) numToLeft += getNodeRead(node.items[i - 1].nodeId).get().accumCount;
                else numToLeft++;
            }
        }

        return findLowerBoundAux(value, getNodeRead(node.items[node.numItems - 1].nodeId), numToLeft);
    }

    Fingerprint fingerprint(size_t begin, size_t end) {
        if (begin > end) throw err("begin > end");

        auto getAccumLeftOf = [&](size_t index) {
            Accumulator accum;
            accum.setToZero();

            traverseToOffset(index, [&](Node &node, size_t index){
                for (size_t i = 0; i < index; i++) accum.add(node.items[i].item);
            }, [&](Node &node){
                accum.add(node.accum);
            });

            return accum;
        };

        auto accum1 = getAccumLeftOf(begin);
        auto accum2 = getAccumLeftOf(end);

        accum1.negate();
        accum2.add(accum1);

        return accum2.getFingerprint(end - begin);
    }



    //// Debug

    void walk() {
        walk(getRootNodeId(), 0);
    }

    void walk(uint64_t nodeId, int depth) {
        if (nodeId == 0) return;

        auto nodePtr = getNodeRead(nodeId);
        auto &node = nodePtr.get();
        std::string indent(depth * 4, ' ');

        std::cout << indent << "NODE id=" << nodeId << " numItems=" << node.numItems << " accum=" << hoytech::to_hex(node.accum.sv()) << " accumCount=" << node.accumCount << std::endl;

        for (size_t i = 0; i < node.numItems; i++) {
            std::cout << indent << "  item: " << node.items[i].item.timestamp << "," << hoytech::to_hex(node.items[i].item.getId()) << std::endl;
            walk(node.items[i].nodeId, depth + 1);
        }
    }


    struct VerifyContext {
        std::optional<uint64_t> leafDepth;
        std::vector<uint64_t> leafNodeIds;
    };

    void verify() {
        VerifyContext ctx;
        Accumulator accum;
        accum.setToZero();
        uint64_t accumCount = 0;

        verify(getRootNodeId(), 0, ctx, &accum, &accumCount);

        if (ctx.leafNodeIds.size()) {
            uint64_t i = 0, totalItems = 0;
            auto nodePtr = getNodeRead(ctx.leafNodeIds[0]);
            std::optional<Item> prevItem;

            while (nodePtr.exists()) {
                auto &node = nodePtr.get();
                if (nodePtr.nodeId != ctx.leafNodeIds[i]) throw err("verify: leaf id mismatch");
                nodePtr = getNodeRead(node.nextLeaf);
                i++;

                for (size_t j = 0; j < node.numItems; j++) {
                    if (prevItem && !(*prevItem < node.items[j].item)) throw err("verify: leaf item out of order");
                    prevItem = node.items[j].item;
                    totalItems++;
                }
            }

            if (totalItems != accumCount) throw err("verify: leaf count mismatch");
        }
    }

    void verify(uint64_t nodeId, uint64_t depth, VerifyContext &ctx, Accumulator *accumOut = nullptr, uint64_t *accumCountOut = nullptr) {
        if (nodeId == 0) return;

        auto nodePtr = getNodeRead(nodeId);
        auto &node = nodePtr.get();

        if (node.numItems < MIN_ITEMS) throw err("verify: too few items");
        if (node.numItems > MAX_ITEMS) throw err("verify: too many items");

        if (node.items[0].nodeId == 0) {
            if (ctx.leafDepth) {
                if (*ctx.leafDepth != depth) throw err("verify: mismatch of leaf depth");
            } else {
                ctx.leafDepth = depth;
            }

            ctx.leafNodeIds.push_back(nodeId);
        }

        Accumulator accum;
        accum.setToZero();
        uint64_t accumCount = 0;

        for (size_t i = 0; i < node.numItems; i++) {
            uint64_t childNodeId = node.items[i].nodeId;
            if (childNodeId == 0) {
                accum.add(node.items[i].item);
                accumCount++;
            } else {
                {
                    auto firstChildPtr = getNodeRead(childNodeId);
                    auto &firstChild = firstChildPtr.get();
                    if (firstChild.numItems == 0 || firstChild.items[0].item != node.items[i].item) throw err("verify: key does not match child's first key");
                }
                verify(childNodeId, depth + 1, ctx, &accum, &accumCount);
            }

            if (i < node.numItems - 1) {
                if (!(node.items[i].item < node.items[i + 1].item)) throw err("verify: items out of order");
            }
        }

        if (accum.sv() != node.accum.sv()) throw err("verify: accum mismatch");
        if (accumCount != node.accumCount) throw err("verify: accumCount mismatch");

        if (accumOut) accumOut->add(accum);
        if (accumCountOut) *accumCountOut += accumCount;
    }

};


}}
