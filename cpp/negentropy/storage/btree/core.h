#pragma once

#include "negentropy.h"


namespace negentropy { namespace storage { namespace btree {

using err = std::runtime_error;

//const size_t MIN_ITEMS = 30;
//const size_t MAX_ITEMS = 80;
const size_t MIN_ITEMS = 2;
const size_t MAX_JOIN = 4;
const size_t MAX_ITEMS = 6;

struct Key {
    Item item;
    uint64_t nodeId;
};

inline bool operator<(const Key &a, const Key &b) {
    return a.item < b.item;
};

struct Node {
    uint64_t numItems; // Number of items in this Node
    uint64_t accumCount; // Total number of items in or under this Node
    uint64_t nextLeaf; // Pointer to next leaf in this level
    uint64_t prevLeaf; // Pointer to previous leaf in this level

    Accumulator accum;

    Key items[MAX_ITEMS + 1];


    Node() {
        memset((void*)this, '\0', sizeof(*this));
    }

    std::string_view sv() {
        return std::string_view(reinterpret_cast<char*>(this), sizeof(*this));
    }
};

struct NodePtr {
    Node *p;
    uint64_t nodeId;


    bool exists() {
        return p != nullptr;
    }

    Node &get() const {
        return *p;
    }
};

struct Breadcrumb {
    size_t index;
    NodePtr nodePtr;
};


struct BTreeCore : StorageBase {
    //// Node Storage

    virtual const NodePtr getNodeRead(uint64_t nodeId) = 0;

    virtual NodePtr getNodeWrite(uint64_t nodeId) = 0;

    virtual NodePtr makeNode() = 0;

    virtual void deleteNode(uint64_t nodeId) = 0;

    virtual uint64_t getRootNodeId() = 0;

    virtual void setRootNodeId(uint64_t newRootNodeId) = 0;


    //// Search

    std::vector<Breadcrumb> searchItem(uint64_t rootNodeId, const Item &newItem, bool &found) {
        found = false;
        std::vector<Breadcrumb> breadcrumbs;

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

            if (!found && (newItem == node.items[index].item)) found = true;

            breadcrumbs.push_back({index, foundNode});
            foundNode = getNodeRead(node.items[index].nodeId);
        }

        return breadcrumbs;
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
            newNode.accum.add(newItem);
            newNode.accumCount = 1;

            setRootNodeId(newNodePtr.nodeId);
            return;
        }


        // Traverse interior nodes, leaving breadcrumbs along the way


        bool found;
        auto breadcrumbs = searchItem(rootNodeId, newItem, found);

        if (found) throw err("already inserted");


        // Follow breadcrumbs back to root

        Key newKey = { newItem, 0 };
        bool needsMerge = true;

        while (breadcrumbs.size()) {
            auto crumb = breadcrumbs.back();
            breadcrumbs.pop_back();

            if (!needsMerge) {
                auto &node = getNodeWrite(crumb.nodePtr.nodeId).get();
                node.accum.add(newItem);
                node.accumCount++;
            } else if (crumb.nodePtr.get().numItems < MAX_ITEMS) {
                // Happy path: Node has room for new item
                auto &node = getNodeWrite(crumb.nodePtr.nodeId).get();

                node.items[node.numItems] = newKey;
                std::inplace_merge(node.items, node.items + node.numItems, node.items + node.numItems + 1);
                node.numItems++;

                node.accum.add(newItem);
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
                    addToAccum(left.items[i], left);
                }

                for (size_t i = 0; i < right.numItems; i++) {
                    right.items[i] = left.items[left.numItems + i];
                    addToAccum(right.items[i], right);
                }

                right.nextLeaf = left.nextLeaf;
                left.nextLeaf = rightPtr.nodeId;
                std::cout << "SETTING PL OF NODE " << rightPtr.nodeId << " TO " << crumb.nodePtr.nodeId << std::endl;
                right.prevLeaf = crumb.nodePtr.nodeId;

                if (right.nextLeaf) {
                    auto &rightRight = getNodeWrite(right.nextLeaf).get();
                    rightRight.prevLeaf = rightPtr.nodeId;
                }

                newKey = { right.items[0].item, rightPtr.nodeId };
            }

            // Update left-most key, in case item was inserted at the beginning

            refreshIndex(getNodeWrite(crumb.nodePtr.nodeId).get(), 0);
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

    void erase(const Item &oldItem) {
        auto rootNodeId = getRootNodeId();
        if (!rootNodeId) throw err("not found");


        // Traverse interior nodes, leaving breadcrumbs along the way

        bool found;
        auto breadcrumbs = searchItem(rootNodeId, oldItem, found);
        if (!found) throw err("not found");


        // Remove from node

        bool needsRemove = true;
        bool neighbourRefreshNeeded = false;

        while (breadcrumbs.size()) {
            auto crumb = breadcrumbs.back();
            breadcrumbs.pop_back();

            auto &node = getNodeWrite(crumb.nodePtr.nodeId).get();

            if (!needsRemove) {
                node.accum.sub(oldItem);
                node.accumCount--;
            } else {
                for (size_t i = crumb.index + 1; i < node.numItems; i++) node.items[i - 1] = node.items[i];
                node.numItems--;

                node.accum.sub(oldItem);
                node.accumCount--;

                needsRemove = false;
            }


            // FIXME: describe

            if (crumb.index < node.numItems) refreshIndex(node, crumb.index);

            if (neighbourRefreshNeeded) {
            std::cout << "NRN " << crumb.nodePtr.nodeId << std::endl;
                if (crumb.index == node.numItems - 1) {
                    if (node.nextLeaf) {
            std::cout << "NL " << node.nextLeaf << std::endl;
                        auto &rightNode = getNodeWrite(node.nextLeaf).get();
                        refreshIndex(rightNode, 0);
                    } else {
                        throw err("no neighbour to refresh");
                    }
                } else {
                    refreshIndex(node, crumb.index + 1);
                    neighbourRefreshNeeded = false;
                }
            }


            if (node.numItems < MIN_ITEMS && node.nextLeaf) {
                auto &rightNode = getNodeWrite(node.nextLeaf).get();
                size_t totalItems = node.numItems + rightNode.numItems;

                if (totalItems <= MAX_JOIN) {
                    // Move all items into rightNode

                    ::memmove(rightNode.items + node.numItems, rightNode.items, sizeof(rightNode.items[0]) * rightNode.numItems);
                    ::memcpy(rightNode.items, node.items, sizeof(node.items[0]) * node.numItems);

                    rightNode.numItems += node.numItems;
                    rightNode.accumCount += node.accumCount;
                    rightNode.accum.add(node.accum);

                    if (node.prevLeaf) getNodeWrite(node.prevLeaf).get().nextLeaf = node.nextLeaf;
                    rightNode.prevLeaf = node.prevLeaf;

                    node.numItems = 0;
                } else {
                    // Re-balance from right to left

                    size_t numLeft = (totalItems + 1) / 2;
                    size_t numRight = totalItems - numLeft;
                    if (numLeft <= node.numItems) throw("right node too small for rebalance");

                    Accumulator accum;
                    accum.setToZero();

                    size_t moveFromRight = rightNode.numItems - numRight;

                    for (size_t i = 0; i < moveFromRight; i++) {
                        auto &item = rightNode.items[i];
                        node.items[node.numItems + i] = item;
                        accum.add(item.item);
                    }

                    ::memmove(rightNode.items, rightNode.items + moveFromRight, (rightNode.numItems - moveFromRight) * sizeof(rightNode.items[0]));
                    ::memset((void*)(rightNode.items + numRight), '\0', moveFromRight * sizeof(rightNode.items[0]));

                    node.accum.add(accum);
                    rightNode.accum.sub(accum);

                    node.accumCount += moveFromRight;
                    rightNode.accumCount -= moveFromRight;

                    node.numItems = numLeft;
                    rightNode.numItems = numRight;

                    neighbourRefreshNeeded = true;
                }
            }

            if (node.numItems == 0) {
                if (node.prevLeaf) getNodeWrite(node.prevLeaf).get().nextLeaf = node.nextLeaf;
                if (node.nextLeaf) getNodeWrite(node.nextLeaf).get().prevLeaf = node.prevLeaf;

                needsRemove = true;

                // FIXME: actually deallocate node.items[crumb.index].nodeId
            }

            if (breadcrumbs.size() == 0) {
                // FIXME: deallocate old root node

                if (node.numItems == 1) {
                    if (node.items[0].nodeId) setRootNodeId(node.items[0].nodeId);
                } else if (node.numItems == 0) {
                    setRootNodeId(0);
                }
            }
        }
    }


    //// Compat with the vector interface

    void addItem(uint64_t createdAt, std::string_view id) {
        insert(Item(createdAt, id));
    }

    void seal() {
    }


    //// Utils

    void refreshIndex(Node &node, size_t index) {
        auto childNodePtr = getNodeRead(node.items[index].nodeId);
        if (childNodePtr.exists()) {
            auto &childNode = childNodePtr.get();
            node.items[index].item = childNode.items[0].item;
        }
    }

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
        auto rootNodePtr = getNodeRead(getRootNodeId());
        if (!rootNodePtr.exists()) return;
        auto &rootNode = rootNodePtr.get();

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
        auto rootNodePtr = getNodeRead(getRootNodeId());
        if (!rootNodePtr.exists()) return 0;
        auto &rootNode = rootNodePtr.get();
        return rootNode.accumCount;
    }

    const Item &getItem(size_t index) {
        if (index >= size()) throw err("out of range");

        Item *out;
        traverseToOffset(index, [&](Node &node, size_t index){
            out = &node.items[index].item;
        });
        return *out;
    }

    void iterate(size_t begin, size_t end, std::function<bool(const Item &, size_t)> cb) {
        checkBounds(begin, end);

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

    size_t findLowerBound(size_t begin, size_t end, const Bound &value) {
        checkBounds(begin, end);

        auto rootNodePtr = getNodeRead(getRootNodeId());
        if (!rootNodePtr.exists()) return 0;
        auto &rootNode = rootNodePtr.get();
        if (value.item <= rootNode.items[0].item) return 0;
        return findLowerBoundAux(value, rootNodePtr, 0);
    }

    size_t findLowerBoundAux(const Bound &value, NodePtr nodePtr, uint64_t numToLeft) {
        if (!nodePtr.exists()) return numToLeft + 1;

        Node &node = nodePtr.get();

        for (size_t i = 1; i < node.numItems; i++) {
            if (value.item <= node.items[i].item) {
                return findLowerBoundAux(value, getNodeRead(node.items[i - 1].nodeId), numToLeft);
            } else {
                if (node.items[i - 1].nodeId) numToLeft += getNodeRead(node.items[i - 1].nodeId).get().accumCount;
                else numToLeft++;
            }
        }

        return findLowerBoundAux(value, getNodeRead(node.items[node.numItems - 1].nodeId), numToLeft);
    }

    Fingerprint fingerprint(size_t begin, size_t end) {
        checkBounds(begin, end);

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

  private:
    void checkBounds(size_t begin, size_t end) {
        if (begin > end || end > size()) throw negentropy::err("bad range");
    }
};


}}}
