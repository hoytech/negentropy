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
    lmdb::dbi dbi;
    lmdb::txn *_txn = nullptr;

    struct MetaData {
        uint64_t rootNodeId;
        uint64_t nextNodeId;

        bool operator==(const MetaData &other) const {
            return rootNodeId == other.rootNodeId && nextNodeId == other.nextNodeId;
        }
    };
    MetaData _metaDataCache;
    std::map<uint64_t, Node> _dirtyNodeCache;


    void setup(lmdb::txn &txn, const std::string &tableName) {
        dbi = lmdb::dbi::open(txn, tableName, MDB_CREATE | MDB_INTEGERKEY);
    }

    struct Cleaner {
        BTreeLMDB &self;

        Cleaner(BTreeLMDB *self) : self(*self) {}

        ~Cleaner() {
            if (self._txn) {
                self._txn->abort();
                self._txn = nullptr;
            }
            self._dirtyNodeCache.clear();
        }
    };

    void withReadTxn(lmdb::txn &txn, const std::function<void()> &cb, uint64_t treeId = 0) {
        _withTxn(txn, true, cb, treeId);
    }

    void withWriteTxn(lmdb::txn &txn, const std::function<void()> &cb, uint64_t treeId = 0) {
        _withTxn(txn, false, cb, treeId);
    }


    // Interface

    const btree::NodePtr getNodeRead(uint64_t nodeId) {
        auto res = _dirtyNodeCache.find(nodeId);
        if (res != _dirtyNodeCache.end()) return NodePtr{&res->second, nodeId};

        std::string_view sv;
        bool found = dbi.get(txn(), lmdb::to_sv<uint64_t>(nodeId), sv);
        if (!found) throw err("couldn't find node");
        return NodePtr{(Node*)sv.data(), nodeId};
    }

    btree::NodePtr getNodeWrite(uint64_t nodeId) {
        auto res = _dirtyNodeCache.try_emplace(nodeId);
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
    void _withTxn(lmdb::txn &parentTxn, bool readOnly, const std::function<void()> &cb, uint64_t treeId) {
        auto txn = lmdb::txn::begin(parentTxn.env(), parentTxn, readOnly ? MDB_RDONLY : 0);
        _txn = &txn;

        Cleaner cleaner(this);

        {
            static_assert(sizeof(MetaData) == 16);
            std::string_view v;
            bool found = dbi.get(txn, lmdb::to_sv<uint64_t>(0), v);
            _metaDataCache = found ? lmdb::from_sv<MetaData>(v) : MetaData{ 0, 1, };
        }

        auto origMetaData = _metaDataCache;

        cb();

        auto getKey = [&](uint64_t n){
            std::string k;
            if (treeId) k += lmdb::to_sv<uint64_t>(treeId);
            k += lmdb::to_sv<uint64_t>(n);
            return k;
        };

        for (auto &[nodeId, node] : _dirtyNodeCache) {
            dbi.put(txn, getKey(nodeId), node.sv());
        }

        if (_metaDataCache != origMetaData) {
            dbi.put(txn, getKey(0), lmdb::to_sv<MetaData>(_metaDataCache));
        }

        txn.commit();
        _txn = nullptr;
    }

    lmdb::txn &txn() {
        if (!_txn) throw err("txn not installed");
        return *_txn;
    }
};


}}
