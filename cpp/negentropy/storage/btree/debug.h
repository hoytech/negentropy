#pragma once

#include <hoytech/hex.h>

#include "negentropy/storage/btree/core.h"


namespace negentropy { namespace storage { namespace btree {


using err = std::runtime_error;


inline void dump(BTreeCore &btree, uint64_t nodeId, int depth) {
    if (nodeId == 0) return;

    auto nodePtr = btree.getNodeRead(nodeId);
    auto &node = nodePtr.get();
    std::string indent(depth * 4, ' ');

    std::cout << indent << "NODE id=" << nodeId << " numItems=" << node.numItems << " accum=" << hoytech::to_hex(node.accum.sv()) << " accumCount=" << node.accumCount << std::endl;

    for (size_t i = 0; i < node.numItems; i++) {
        std::cout << indent << "  item: " << node.items[i].item.timestamp << "," << hoytech::to_hex(node.items[i].item.getId()) << std::endl;
        dump(btree, node.items[i].nodeId, depth + 1);
    }
}

inline void dump(BTreeCore &btree) {
    dump(btree, btree.getRootNodeId(), 0);
}


struct VerifyContext {
    std::optional<uint64_t> leafDepth;
    std::vector<uint64_t> leafNodeIds;
};

inline void verify(BTreeCore &btree, uint64_t nodeId, uint64_t depth, VerifyContext &ctx, Accumulator *accumOut = nullptr, uint64_t *accumCountOut = nullptr) {
    if (nodeId == 0) return;

    auto nodePtr = btree.getNodeRead(nodeId);
    auto &node = nodePtr.get();

    if (node.nextLeaf && node.numItems < MIN_ITEMS) throw err("verify: too few items in node");
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
                auto firstChildPtr = btree.getNodeRead(childNodeId);
                auto &firstChild = firstChildPtr.get();
                if (firstChild.numItems == 0 || firstChild.items[0].item != node.items[i].item) throw err("verify: key does not match child's first key");
            }
            verify(btree, childNodeId, depth + 1, ctx, &accum, &accumCount);
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

inline void verify(BTreeCore &btree) {
    VerifyContext ctx;
    Accumulator accum;
    accum.setToZero();
    uint64_t accumCount = 0;

    verify(btree, btree.getRootNodeId(), 0, ctx, &accum, &accumCount);

    if (ctx.leafNodeIds.size()) {
        uint64_t i = 0, totalItems = 0;
        auto nodePtr = btree.getNodeRead(ctx.leafNodeIds[0]);
        std::optional<Item> prevItem;
        uint64_t prevLeaf = 0;

        while (nodePtr.exists()) {
            auto &node = nodePtr.get();
            if (nodePtr.nodeId != ctx.leafNodeIds[i]) throw err("verify: leaf id mismatch");

            if (prevLeaf != node.prevLeaf) throw err("verify: prevLeaf mismatch");
            prevLeaf = nodePtr.nodeId;

            nodePtr = btree.getNodeRead(node.nextLeaf);
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



}}}
