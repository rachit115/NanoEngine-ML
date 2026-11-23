#include "engine/nano_engine.hpp"
#include "common/logger.hpp"
#include <sstream>

namespace nanoengine {

NanoEngine::NanoEngine(const std::string& db_file, const std::string& log_file,
                       size_t buffer_pool_size, ReplacerType replacer_type)
    : db_file_name_(db_file), log_file_name_(log_file), buffer_pool_size_(buffer_pool_size) {
    
    disk_manager_ = std::make_unique<DiskManager>(db_file_name_, log_file_name_);
    log_manager_ = std::make_unique<LogManager>(disk_manager_.get());
    buffer_pool_manager_ = std::make_unique<BufferPoolManager>(
        buffer_pool_size_, disk_manager_.get(), log_manager_.get(), replacer_type);
    lock_manager_ = std::make_unique<LockManager>();
    index_ = std::make_unique<ConcurrentBPlusTree>();
    recovery_engine_ = std::make_unique<AriesRecoveryEngine>(
        disk_manager_.get(), buffer_pool_manager_.get(), log_file_name_);

    // If database file has existing pages, we ensure page 0 is accessible
    if (disk_manager_->GetNumPages() == 0) {
        page_id_t root_pid = INVALID_PAGE_ID;
        Page* root_page = buffer_pool_manager_->NewPage(&root_pid);
        if (root_page) {
            buffer_pool_manager_->UnpinPage(root_pid, true);
        }
    }
}

NanoEngine::~NanoEngine() {
    buffer_pool_manager_->FlushAllPages();
    log_manager_->Flush();
}

std::shared_ptr<Transaction> NanoEngine::BeginTransaction() {
    txn_id_t tid = next_txn_id_++;
    auto txn = std::make_shared<Transaction>(tid);

    // Append BEGIN record to WAL
    LogRecord rec(tid, INVALID_LSN, LogRecordType::BEGIN);
    lsn_t lsn = log_manager_->AppendLogRecord(rec);
    txn->SetPrevLSN(lsn);

    std::lock_guard<std::mutex> guard(txn_map_latch_);
    active_transactions_[tid] = txn;
    return txn;
}

bool NanoEngine::Commit(std::shared_ptr<Transaction> txn) {
    if (!txn || txn->GetState() != TransactionState::GROWING) {
        return false;
    }

    // Append COMMIT record to WAL
    LogRecord rec(txn->GetTxnId(), txn->GetPrevLSN(), LogRecordType::COMMIT);
    log_manager_->AppendLogRecord(rec);

    // Enforce WAL: flush log to disk before committing!
    log_manager_->Flush();

    txn->SetState(TransactionState::COMMITTED);
    lock_manager_->ReleaseAll(txn.get());

    std::lock_guard<std::mutex> guard(txn_map_latch_);
    active_transactions_.erase(txn->GetTxnId());
    return true;
}

bool NanoEngine::Abort(std::shared_ptr<Transaction> txn) {
    if (!txn) return false;

    // Append ABORT record to WAL
    LogRecord rec(txn->GetTxnId(), txn->GetPrevLSN(), LogRecordType::ABORT);
    log_manager_->AppendLogRecord(rec);
    log_manager_->Flush();

    txn->SetState(TransactionState::ABORTED);
    lock_manager_->ReleaseAll(txn.get());

    std::lock_guard<std::mutex> guard(txn_map_latch_);
    active_transactions_.erase(txn->GetTxnId());
    return true;
}

bool NanoEngine::Put(std::shared_ptr<Transaction> txn, int64_t key, const std::string& value) {
    std::string lock_key = "K_" + std::to_string(key);

    // 1. Strict 2PL: Acquire Exclusive Lock
    if (txn && !lock_manager_->LockExclusive(txn.get(), lock_key)) {
        return false;
    }

    // 2. Fetch or allocate target storage page in Buffer Pool
    page_id_t target_page_id = static_cast<page_id_t>(std::abs(key) % std::max<size_t>(1, disk_manager_->GetNumPages()));
    Page* page = buffer_pool_manager_->FetchPage(target_page_id, true);
    if (!page) {
        page = buffer_pool_manager_->NewPage(&target_page_id);
    }

    // 3. Get old value if present in B+ Tree
    std::string old_val;
    index_->GetValue(key, &old_val);

    // 4. Append UPDATE record to WAL
    lsn_t lsn = INVALID_LSN;
    if (txn) {
        LogRecord update_rec(txn->GetTxnId(), txn->GetPrevLSN(), LogRecordType::UPDATE,
                             target_page_id, key, old_val, value);
        lsn = log_manager_->AppendLogRecord(update_rec);
        txn->SetPrevLSN(lsn);
    }

    // 5. Update physical Page data in memory
    if (page) {
        page->WLock();
        if (lsn != INVALID_LSN) {
            page->SetLSN(lsn);
        }
        page->WUnlock();
        buffer_pool_manager_->UnpinPage(target_page_id, true);
    }

    // 6. Update B+ Tree index
    return index_->Insert(key, value);
}

bool NanoEngine::Get(std::shared_ptr<Transaction> txn, int64_t key, std::string* result) {
    std::string lock_key = "K_" + std::to_string(key);

    // 1. Strict 2PL: Acquire Shared Lock
    if (txn && !lock_manager_->LockShared(txn.get(), lock_key)) {
        return false;
    }

    // 2. Access buffer pool page to simulate data block access and count cache hits/misses
    page_id_t target_page_id = static_cast<page_id_t>(std::abs(key) % std::max<size_t>(1, disk_manager_->GetNumPages()));
    Page* page = buffer_pool_manager_->FetchPage(target_page_id, false);
    if (page) {
        page->RLock();
        page->RUnlock();
        buffer_pool_manager_->UnpinPage(target_page_id, false);
    }

    // 3. Search B+ Tree index
    return index_->GetValue(key, result);
}

bool NanoEngine::Delete(std::shared_ptr<Transaction> txn, int64_t key) {
    std::string lock_key = "K_" + std::to_string(key);

    if (txn && !lock_manager_->LockExclusive(txn.get(), lock_key)) {
        return false;
    }

    std::string old_val;
    if (!index_->GetValue(key, &old_val)) {
        return false; // Key does not exist
    }

    page_id_t target_page_id = static_cast<page_id_t>(std::abs(key) % std::max<size_t>(1, disk_manager_->GetNumPages()));
    if (txn) {
        LogRecord del_rec(txn->GetTxnId(), txn->GetPrevLSN(), LogRecordType::UPDATE,
                          target_page_id, key, old_val, "");
        lsn_t lsn = log_manager_->AppendLogRecord(del_rec);
        txn->SetPrevLSN(lsn);
    }

    return index_->Delete(key);
}

std::vector<std::pair<int64_t, std::string>> NanoEngine::Scan(std::shared_ptr<Transaction> txn,
                                                              int64_t min_key, int64_t max_key) {
    (void)txn;
    return index_->ScanRange(min_key, max_key);
}

void NanoEngine::SetReplacerType(ReplacerType type) {
    buffer_pool_manager_->SetReplacerType(type);
}

void NanoEngine::SimulateCrash() {
    // Drop memory state without flushing dirty pages to simulate power cutoff!
    active_transactions_.clear();
    index_->Clear();
    // Re-instantiate buffer pool manager from scratch (wipes RAM frames)
    buffer_pool_manager_ = std::make_unique<BufferPoolManager>(
        buffer_pool_size_, disk_manager_.get(), log_manager_.get(), ReplacerType::LRU);
    recovery_engine_ = std::make_unique<AriesRecoveryEngine>(
        disk_manager_.get(), buffer_pool_manager_.get(), log_file_name_);
}

RecoveryStats NanoEngine::Recover() {
    RecoveryStats stats = recovery_engine_->Recover();
    
    // Rebuild index only from durable COMMITTED transactions
    auto records = recovery_engine_->ReadAllLogRecords();
    std::unordered_set<txn_id_t> committed_txns;
    for (const auto& r : records) {
        if (r.type == LogRecordType::COMMIT) {
            committed_txns.insert(r.txn_id);
        }
    }

    for (const auto& r : records) {
        if (r.type == LogRecordType::UPDATE && committed_txns.count(r.txn_id) > 0) {
            if (!r.new_val.empty()) {
                index_->Insert(r.key, r.new_val);
            } else {
                index_->Delete(r.key);
            }
        }
    }
    return stats;
}

EngineStats NanoEngine::GetStats() {
    EngineStats stats;
    stats.buffer_hits = buffer_pool_manager_->GetBufferHits();
    stats.buffer_misses = buffer_pool_manager_->GetBufferMisses();
    stats.cache_hit_ratio = buffer_pool_manager_->GetCacheHitRatio();
    stats.disk_reads = disk_manager_->GetNumDiskReads();
    stats.disk_writes = disk_manager_->GetNumDiskWrites();
    stats.index_num_keys = index_->GetNumKeys();
    return stats;
}

std::vector<FrameTableEntry> NanoEngine::GetFrameTableSnapshot() {
    return buffer_pool_manager_->GetFrameTableSnapshot();
}

void NanoEngine::ResetMetrics() {
    buffer_pool_manager_->ResetMetrics();
    disk_manager_->ResetMetrics();
}

} // namespace nanoengine
