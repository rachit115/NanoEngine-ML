#include "index/b_plus_tree.hpp"

namespace nanoengine {

ConcurrentBPlusTree::ConcurrentBPlusTree() {
    root_ = std::make_shared<BLeafNode>();
}

size_t ConcurrentBPlusTree::GetNumKeys() const {
    return num_keys_.load();
}

void ConcurrentBPlusTree::Clear() {
    std::unique_lock<std::shared_mutex> lock(tree_latch_);
    root_ = std::make_shared<BLeafNode>();
    num_keys_.store(0);
}

std::shared_ptr<BLeafNode> ConcurrentBPlusTree::FindLeaf(int64_t key) const {
    std::shared_ptr<BNode> curr = root_;
    while (!curr->IsLeaf()) {
        auto internal = std::static_pointer_cast<BInternalNode>(curr);
        size_t idx = 0;
        while (idx < internal->keys_.size() && key >= internal->keys_[idx]) {
            idx++;
        }
        curr = internal->children_[idx];
    }
    return std::static_pointer_cast<BLeafNode>(curr);
}

bool ConcurrentBPlusTree::GetValue(int64_t key, std::string* result) {
    std::shared_lock<std::shared_mutex> lock(tree_latch_);
    if (!root_) return false;

    auto leaf = FindLeaf(key);
    auto it = std::lower_bound(leaf->keys_.begin(), leaf->keys_.end(), key);
    if (it != leaf->keys_.end() && *it == key) {
        if (result) {
            size_t idx = std::distance(leaf->keys_.begin(), it);
            *result = leaf->values_[idx];
        }
        return true;
    }
    return false;
}

void ConcurrentBPlusTree::InsertIntoParent(std::shared_ptr<BNode> left, int64_t key, std::shared_ptr<BNode> right) {
    if (left == root_) {
        // Create new root
        auto new_root = std::make_shared<BInternalNode>();
        new_root->keys_.push_back(key);
        new_root->children_.push_back(left);
        new_root->children_.push_back(right);
        left->parent_ = new_root;
        right->parent_ = new_root;
        root_ = new_root;
        return;
    }

    auto parent = std::static_pointer_cast<BInternalNode>(left->parent_);
    // Find position to insert key and right child
    size_t idx = 0;
    while (idx < parent->children_.size() && parent->children_[idx] != left) {
        idx++;
    }

    parent->keys_.insert(parent->keys_.begin() + idx, key);
    parent->children_.insert(parent->children_.begin() + idx + 1, right);
    right->parent_ = parent;

    // If parent exceeds capacity, split parent
    if (parent->keys_.size() >= B_TREE_ORDER) {
        auto new_internal = std::make_shared<BInternalNode>();
        size_t mid = parent->keys_.size() / 2;
        int64_t promoted_key = parent->keys_[mid];

        new_internal->keys_.assign(parent->keys_.begin() + mid + 1, parent->keys_.end());
        new_internal->children_.assign(parent->children_.begin() + mid + 1, parent->children_.end());

        for (auto& child : new_internal->children_) {
            child->parent_ = new_internal;
        }

        parent->keys_.erase(parent->keys_.begin() + mid, parent->keys_.end());
        parent->children_.erase(parent->children_.begin() + mid + 1, parent->children_.end());

        InsertIntoParent(parent, promoted_key, new_internal);
    }
}

bool ConcurrentBPlusTree::Insert(int64_t key, const std::string& value) {
    std::unique_lock<std::shared_mutex> lock(tree_latch_);

    auto leaf = FindLeaf(key);
    auto it = std::lower_bound(leaf->keys_.begin(), leaf->keys_.end(), key);

    if (it != leaf->keys_.end() && *it == key) {
        // Key exists: update value
        size_t idx = std::distance(leaf->keys_.begin(), it);
        leaf->values_[idx] = value;
        return true;
    }

    // Insert new key and value
    size_t idx = std::distance(leaf->keys_.begin(), it);
    leaf->keys_.insert(it, key);
    leaf->values_.insert(leaf->values_.begin() + idx, value);
    num_keys_++;

    // Split leaf if needed
    if (leaf->keys_.size() >= B_TREE_ORDER) {
        auto new_leaf = std::make_shared<BLeafNode>();
        size_t mid = leaf->keys_.size() / 2;

        new_leaf->keys_.assign(leaf->keys_.begin() + mid, leaf->keys_.end());
        new_leaf->values_.assign(leaf->values_.begin() + mid, leaf->values_.end());

        leaf->keys_.erase(leaf->keys_.begin() + mid, leaf->keys_.end());
        leaf->values_.erase(leaf->values_.begin() + mid, leaf->values_.end());

        new_leaf->next_leaf_ = leaf->next_leaf_;
        leaf->next_leaf_ = new_leaf;

        InsertIntoParent(leaf, new_leaf->keys_[0], new_leaf);
    }

    return true;
}

bool ConcurrentBPlusTree::Delete(int64_t key) {
    std::unique_lock<std::shared_mutex> lock(tree_latch_);
    if (!root_) return false;

    auto leaf = FindLeaf(key);
    auto it = std::lower_bound(leaf->keys_.begin(), leaf->keys_.end(), key);

    if (it != leaf->keys_.end() && *it == key) {
        size_t idx = std::distance(leaf->keys_.begin(), it);
        leaf->keys_.erase(it);
        leaf->values_.erase(leaf->values_.begin() + idx);
        num_keys_--;
        return true;
    }
    return false;
}

std::vector<std::pair<int64_t, std::string>> ConcurrentBPlusTree::ScanRange(int64_t min_key, int64_t max_key) {
    std::shared_lock<std::shared_mutex> lock(tree_latch_);
    std::vector<std::pair<int64_t, std::string>> results;
    if (!root_) return results;

    auto leaf = FindLeaf(min_key);
    while (leaf) {
        for (size_t i = 0; i < leaf->keys_.size(); ++i) {
            if (leaf->keys_[i] >= min_key && leaf->keys_[i] <= max_key) {
                results.emplace_back(leaf->keys_[i], leaf->values_[i]);
            } else if (leaf->keys_[i] > max_key) {
                return results;
            }
        }
        leaf = leaf->next_leaf_;
    }
    return results;
}

} // namespace nanoengine
