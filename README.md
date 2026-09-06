# NanoEngine-ML

### A Crash-Resilient Relational Storage Engine with a Learned Buffer Pool Replacement Policy

NanoEngine-ML is an academic systems project that aims to build a **relational storage engine from the ground up**, combining Operating Systems concepts with Database Management System internals.

Instead of building an application on top of an existing database such as MySQL, PostgreSQL, or SQLite, the project focuses on implementing the low-level storage components directly over binary disk pages.

> **Core idea:** Build a crash-resilient storage engine with custom disk paging, buffer management, concurrent indexing, transaction locking, ARIES Write-Ahead Logging, and a lightweight machine-learning-based buffer replacement policy.

---

## Project Overview

Traditional buffer pool managers commonly use heuristic replacement policies such as **LRU** and **CLOCK**. These policies are simple and efficient, but their fixed rules may perform poorly when workloads contain non-uniform, bursty, or changing access patterns.

NanoEngine-ML explores a learned replacement layer that can use observed page-access behavior to estimate which pages are likely to be reused. The learned policy will be evaluated against traditional replacement policies such as LRU and CLOCK.

The project combines:

- Operating Systems
- Database Systems
- Storage Engines
- Concurrency Control
- Crash Recovery
- Machine Learning

The detailed project proposal describes the system as a C++20 storage engine using 4 KB binary pages, a buffer pool, a concurrent B+ Tree, Strict Two-Phase Locking (SS2PL), and ARIES recovery.

---

## Architecture

```text
                    +-----------------------------+
                    |      Execution Engine       |
                    | Tuple Scan / Insert /       |
                    | Update / Index Seek         |
                    +-------------+---------------+
                                  |
              +-------------------+-------------------+
              |                   |                   |
              v                   v                   v
    +----------------+  +------------------+  +----------------+
    | Thread-Safe    |  | Strict 2PL       |  | WAL / ARIES    |
    | B+ Tree Index  |  | Lock Manager     |  | Log Manager    |
    +----------------+  +------------------+  +----------------+
                                  |
                                  v
                    +-----------------------------+
                    |     Buffer Pool Manager     |
                    |                             |
                    | LRU / CLOCK baseline        |
                    | Learned replacement policy   |
                    | Frame table / pin counts    |
                    +-------------+---------------+
                                  |
                                  v
                    +-----------------------------+
                    |     Disk Storage Engine     |
                    |                             |
                    | 4 KB binary pages           |
                    | Slotted-page layout         |
                    +-----------------------------+
```

---

## Core Components

### 1. Disk Storage Engine

The storage layer operates on fixed-size **4 KB binary pages**.

Planned responsibilities include:

- Low-level binary page reads and writes
- Page allocation and management
- Slotted-page record organization
- Tuple serialization
- Free-space tracking
- Page-level metadata including `PageLSN`

### 2. Buffer Pool Manager

The buffer pool provides an in-memory cache of disk pages.

It is planned to support:

- Fixed-size memory frames
- Page pin/unpin operations
- Dirty-page tracking
- Pin counts
- Page eviction
- LRU replacement
- CLOCK / Second-Chance replacement
- Learned buffer replacement

The learned replacement component is the primary ML contribution of NanoEngine-ML.

### 3. Learned Buffer Replacement

The ML layer is intended to estimate the likelihood that a page will be accessed again.

The project will compare the learned policy against traditional policies using workload patterns such as:

- Uniform random access
- Zipfian/skewed access
- Mixed transactional workloads

Primary evaluation metrics include:

- Cache hit ratio
- Page faults
- Disk I/O behavior
- Replacement overhead

The project proposal treats the learned policy as an adaptive layer inside the Buffer Pool Manager rather than as a separate application-level ML system.

### 4. Concurrent B+ Tree

NanoEngine-ML plans to implement a disk-backed, thread-safe B+ Tree supporting:

- Search
- Insert
- Page splitting
- Page merging
- Sibling-pointer traversal
- Concurrent access
- Latch management

The target search complexity is **O(log N)**.

### 5. Transaction & Concurrency Control

The transaction system is based on **Strict Two-Phase Locking (SS2PL)**.

Planned capabilities include:

- Shared (S) locks
- Exclusive (X) locks
- Lock acquisition and release
- Lock upgrade/downgrade
- Serializable transaction schedules
- Concurrent transaction execution

Deadlock handling is planned through a **Wait-For Graph (WFG)** with cycle detection and transaction rollback.

### 6. Write-Ahead Logging & ARIES Recovery

NanoEngine-ML uses the **ARIES recovery model** for crash resilience.

The WAL system follows the principle that the required log records must be persisted before the corresponding dirty data page is written.

Recovery is organized around the ARIES three-pass process:

1. **Analysis**
2. **Redo**
3. **Undo**

The project also plans to test recovery through simulated crashes and verify that the database returns to a consistent state.

---

## OS Concepts Demonstrated

| OS Concept | NanoEngine-ML Implementation |
|---|---|
| Process/Thread Synchronization | Mutexes, reader-writer latches, condition variables |
| Deadlock Management | Wait-For Graph and cycle detection |
| Virtual Memory & Paging | Buffer pool and page replacement |
| Page Replacement | LRU and CLOCK policies |
| File Systems & Disk I/O | 4 KB binary pages and low-level file I/O |
| Concurrent Execution | Multi-threaded transaction and buffer operations |

---

## DBMS Concepts Demonstrated

| DBMS Concept | NanoEngine-ML Implementation |
|---|---|
| Storage & Indexing | Disk-backed concurrent B+ Tree |
| ACID Transactions | Transaction state management |
| Concurrency Control | Strict Two-Phase Locking |
| Crash Recovery | ARIES three-pass recovery |
| Write-Ahead Logging | LSN-based WAL |
| Buffer Management | LRU, CLOCK and learned replacement |

---

## Technology Stack

- **Language:** C++20
- **Storage:** Binary disk files
- **Page size:** 4 KB
- **Concurrency:** C++ threading and synchronization primitives
- **Recovery:** ARIES / Write-Ahead Logging
- **Indexing:** B+ Tree
- **Machine Learning:** Lightweight online inference for buffer replacement
- **Platform assumption:** POSIX-style file-system primitives such as binary I/O and `fsync`

---

## Evaluation Plan

The project will evaluate the storage engine across three major areas.

### Memory Paging

Compare:

- LRU
- CLOCK
- Learned replacement

Under:

- Uniform random workloads
- Zipfian/skewed workloads

Metrics:

- Cache hit ratio
- Page faults

### Transaction Concurrency

Benchmark with:

- 1 thread
- 2 threads
- 4 threads
- 8 threads
- 16 threads

Under different levels of transaction conflict.

Metrics:

- Transactions per second (TPS)
- Lock contention latency
- Throughput scaling

### Crash Recovery

Measure recovery performance using different log/checkpoint workloads.

Target checkpoint intervals include:

- 500 log records
- 2,000 log records
- 10,000 log records

Metric:

- Recovery Time Objective (RTO)

---

## Development Roadmap

| Phase | Planned Deliverable |
|---|---|
| Weeks 1–4 | Disk Storage Manager and Slotted Pages |
| Weeks 5–8 | Buffer Pool Manager with LRU/CLOCK and learned replacement |
| Weeks 9–12 | Concurrent B+ Tree and Strict 2PL Lock Manager |
| Weeks 13–16 | WAL Logger, ARIES Recovery and Benchmarks |

The project proposal defines the final crash-recovery milestone as passing simulated crash tests with verified data consistency.

---

## Project Status

**Current stage: Architecture / implementation planning**

The repository is intended to evolve alongside the implementation. Components will be added and documented as the storage engine is developed.

The README intentionally describes planned capabilities separately from completed implementation so that the documentation remains accurate during development.

---

## Team

**Team KernelInnovators**

- Rachit Singh
- Shantanu Devrani
- Harsh Sharma
- Bhanu Panwar

---

## Research Foundations

The project draws on established work in database architecture, concurrency, and recovery:

1. Mohan, C. et al. (1992). *ARIES: A Transaction Recovery Method Supporting Fine-Granularity Locking and Partial Rollbacks Using Write-Ahead Logging.* ACM TODS.
2. Stonebraker, M. (1981). *Operating System Support for Database Management.* Communications of the ACM.
3. Lehman, P. L. & Yao, S. B. (1981). *Efficient Locking for Concurrent Operations on B-Trees.* ACM TODS.
4. Hellerstein, J. M., Stonebraker, M. & Hamilton, J. (2007). *Architecture of a Database System.* Foundations and Trends in Databases.

---

## Contributing

Contributions are welcome as the project develops.

A typical contribution workflow is:

```bash
git clone <repository-url>
cd NanoEngine-ML

git checkout -b docs/readme

# make your changes

git add README.md
git commit -m "docs: add project README"
git push origin docs/readme
```

If you are contributing through a fork, push the branch to your fork and open a Pull Request against the main repository.

For implementation changes, contributors should keep the modular architecture in mind and add or update tests for the affected storage, concurrency, buffer-management, or recovery component.

---

## License

License information will be added when the project license is established.
