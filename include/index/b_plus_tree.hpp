#pragma once

#include "common/config.hpp"
#include <string>
#include <vector>
#include <memory>
#include <shared_mutex>
#include <mutex>
#include <atomic>
#include <optional>
#include <algorithm>

namespace nanoengine {

static constexpr size_t B_TREE_ORDER = 16; // Capacity per node

enum class BNodeType {
    INTERNAL,
    LEAF
};

class BNode {
public:
    explicit BNode(BNodeType type) : type_(type) {}
    virtual ~BNode() = default;

    bool IsLeaf() const { return type_ == BNodeType::LEAF; }

    BNodeType type_;
    std::vector<int64_t> keys_;
    std::shared_ptr<BNode> parent_{nullptr};
};

class BLeafNode : public BNode {
public:
    BLeafNode() : BNode(BNodeType::LEAF) {}
    std::vector<std::string> values_;
    std::shared_ptr<BLeafNode> next_leaf_{nullptr};
};

class BInternalNode : public BNode {
public:
    BInternalNode() : BNode(BNodeType::INTERNAL) {}
    std::vector<std::shared_ptr<BNode>> children_;
};

class ConcurrentBPlusTree {
public:
    ConcurrentBPlusTree();
    ~ConcurrentBPlusTree() = default;

    // Reader-Writer Latch concurrency
    bool Insert(int64_t key, const std::string& value);
    bool GetValue(int64_t key, std::string* result);
    bool Delete(int64_t key);
    std::vector<std::pair<int64_t, std::string>> ScanRange(int64_t min_key, int64_t max_key);

    size_t GetNumKeys() const;
    void Clear();

private:
    std::shared_ptr<BLeafNode> FindLeaf(int64_t key) const;
    void InsertIntoParent(std::shared_ptr<BNode> left, int64_t key, std::shared_ptr<BNode> right);

    std::shared_ptr<BNode> root_{nullptr};
    mutable std::shared_mutex tree_latch_;
    std::atomic<size_t> num_keys_{0};
};

} // namespace nanoengine
