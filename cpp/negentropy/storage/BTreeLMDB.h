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

    struct MetaData {
        uint64_t rootNodeId;
        uint64_t nextNodeId;

        bool operator==(const MetaData &other) const {
            return rootNodeId == other.rootNodeId && nextNodeId == other.nextNodeId;
        }
    };

    struct TxnCtx {
        lmdb::txn *txn = nullptr;
        bool readOnly;
        uint64_t treeId;
        MetaData metaDataCache;
        std::map<uint64_t, Node> dirtyNodeCache;
    };

    std::optional<TxnCtx> ctx;


    void setup(lmdb::txn &txn, const std::string &tableName) {
        dbi = lmdb::dbi::open(txn, tableName, MDB_CREATE | MDB_INTEGERKEY);
    }

    struct Cleaner {
        BTreeLMDB &self;

        Cleaner(BTreeLMDB *self) : self(*self) {}

        ~Cleaner() {
            if (!self.ctx) return;

            if (self.ctx->txn) {
                if (!self.ctx->readOnly) self.ctx->txn->abort(); // abort nested transaction
                self.ctx->txn = nullptr;
            }

            self.ctx = std::nullopt;
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
        if (nodeId == 0) return {nullptr, 0};

        auto res = ctx->dirtyNodeCache.find(nodeId);
        if (res != ctx->dirtyNodeCache.end()) return NodePtr{&res->second, nodeId};

        std::string_view sv;
        bool found = dbi.get(txn(), getKey(nodeId), sv);
        if (!found) throw err("couldn't find node");
        return NodePtr{(Node*)sv.data(), nodeId};
    }

    btree::NodePtr getNodeWrite(uint64_t nodeId) {
        if (nodeId == 0) return {nullptr, 0};

        {
            auto res = ctx->dirtyNodeCache.find(nodeId);
            if (res != ctx->dirtyNodeCache.end()) return NodePtr{&res->second, nodeId};
        }

        std::string_view sv;
        bool found = dbi.get(txn(), getKey(nodeId), sv);
        if (!found) throw err("couldn't find node");

        auto res = ctx->dirtyNodeCache.try_emplace(nodeId);
        Node *newNode = &res.first->second;
        memcpy(newNode, sv.data(), sizeof(Node));

        return NodePtr{newNode, nodeId};
    }

    btree::NodePtr makeNode() {
        uint64_t nodeId = ctx->metaDataCache.nextNodeId++;
        auto res = ctx->dirtyNodeCache.try_emplace(nodeId);
        return NodePtr{&res.first->second, nodeId};
    }

    void deleteNode(uint64_t nodeId) {
        if (nodeId == 0) throw err("can't delete metadata");
        dbi.del(txn(), getKey(nodeId));
    }

    uint64_t getRootNodeId() {
        return ctx->metaDataCache.rootNodeId;
    }

    void setRootNodeId(uint64_t newRootNodeId) {
        ctx->metaDataCache.rootNodeId = newRootNodeId;
    }

    // Internal utils

  private:
    void _withTxn(lmdb::txn &parentTxn, bool readOnly, const std::function<void()> &cb, uint64_t treeId) {
        ctx = TxnCtx{};

        lmdb::txn childTxn(nullptr);

        ctx->readOnly = readOnly;

        if (readOnly) {
            ctx->txn = &parentTxn;
        } else {
            childTxn = lmdb::txn::begin(parentTxn.env(), parentTxn);
            ctx->txn = &childTxn;
        }

        ctx->treeId = treeId;

        Cleaner cleaner(this);

        {
            static_assert(sizeof(MetaData) == 16);
            std::string_view v;
            bool found = dbi.get(txn(), lmdb::to_sv<uint64_t>(0), v);
            ctx->metaDataCache = found ? lmdb::from_sv<MetaData>(v) : MetaData{ 0, 1, };
        }

        auto origMetaData = ctx->metaDataCache;

        cb();

        for (auto &[nodeId, node] : ctx->dirtyNodeCache) {
            dbi.put(txn(), getKey(nodeId), node.sv());
        }

        if (ctx->metaDataCache != origMetaData) {
            dbi.put(txn(), getKey(0), lmdb::to_sv<MetaData>(ctx->metaDataCache));
        }

        if (!ctx->readOnly) {
            ctx->txn->commit();
            ctx->txn = nullptr;
        }
    }

    lmdb::txn &txn() {
        if (!ctx || !ctx->txn) throw err("txn not installed");
        return *ctx->txn;
    }

    std::string getKey(uint64_t n) {
        std::string k;
        if (ctx->treeId) k += lmdb::to_sv<uint64_t>(ctx->treeId);
        k += lmdb::to_sv<uint64_t>(n);
        return k;
    }
};


}}
