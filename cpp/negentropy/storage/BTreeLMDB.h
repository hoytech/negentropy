#pragma once

#include <map>

#include "lmdbxx/lmdb++.h"

#include "negentropy.h"
#include "negentropy/storage/btree/core.h"


namespace negentropy { namespace storage {

using err = std::runtime_error;
using Node = negentropy::storage::btree::Node;
using NodePtr = negentropy::storage::btree::NodePtr;


struct BTreeLMDB : btree::BTreeCore {
    lmdb::txn *_txn = nullptr;
    lmdb::dbi dbi;
    std::map<uint64_t, Node> _nodeCache;
    std::optional<uint64_t> newRootNodeId;
    // FIXME: how to track deletions?

    void setup(lmdb::txn &txn, const std::string &tableName) {
        dbi = lmdb::dbi::open(txn, tableName, MDB_CREATE | MDB_INTEGERKEY);
    }

    struct Cleaner {
        BTreeLMDB &self;

        Cleaner(BTreeLMDB *self) : self(*self) {}

        ~Cleaner() {
            self._txn = nullptr;
            self._nodeCache.clear();
            self.newRootNodeId = std::nullopt;
        }
    };

    void withTxn(lmdb::txn &txn, std::function<void()> cb) {
        _txn = &txn;
        Cleaner cleaner(this);

        cb();

        for (auto &[nodeId, node] : _nodeCache) {
            dbi.put(txn, lmdb::to_sv<uint64_t>(nodeId), node.sv());
        }

        if (newRootNodeId) dbi.put(txn, lmdb::to_sv<uint64_t>(0), lmdb::to_sv<uint64_t>(*newRootNodeId));
    }

    // Interface

    const btree::NodePtr getNodeRead(uint64_t nodeId) {
        auto res = _nodeCache.find(nodeId);
        if (res != _nodeCache.end()) return NodePtr{&res->second, nodeId};

        std::string_view sv;
        bool found = dbi.get(txn(), lmdb::to_sv<uint64_t>(nodeId), sv);
        if (!found) throw err("couldn't find node");
        return NodePtr{(Node*)sv.data(), nodeId};
    }

    btree::NodePtr getNodeWrite(uint64_t nodeId) {
        auto res = _nodeCache.try_emplace(nodeId);
        return NodePtr{&res.first->second, nodeId};
    }

    btree::NodePtr makeNode() {
    }

    void deleteNode(uint64_t nodeId) {
    }

    uint64_t getRootNodeId() {
    }

    void setRootNodeId(uint64_t newRootNodeId) {
    }

    // Internal utils

  private:
    lmdb::txn &txn() {
        if (!_txn) throw err("txn not installed");
        return *_txn;
    }
};


}}
