# Ultra-Low Latency Matching Engine ⚡

A high-performance, ultra-low latency Limit Order Book (LOB) and matching engine written in modern C++ (C++17). Designed for High-Frequency Trading (HFT) scenarios, achieving end-to-end processing latency at the **nanosecond level**.

## 🚀 Core Architecture & Highlights

* **Zero-Branching Intrusive List:** Implemented a doubly linked list with **Sentinel Nodes** to completely eliminate `if-else` branch prediction overheads during order insertion and deletion.
* **$O(1)$ Price Level Indexing:** Utilized continuous flat arrays for bid/ask price levels, achieving absolute $O(1)$ memory access without the overhead of tree-based structures (e.g., `std::map`).
* **Cache-Line Alignment:** Core data structures and critical pointers are strictly aligned to 64 bytes (`alignas(64)`) to fit perfectly into CPU cache lines, effectively preventing **False Sharing** in multi-core environments.
* **Zero Runtime Allocation:** Built a static memory pool pre-allocated at startup. Absolutely no `new`/`delete` or system calls are triggered during the hot path of the trading session.
* **Lock-Free Concurrency (SPSC):** Implemented a Single-Producer-Single-Consumer (SPSC) lock-free ring buffer using `std::atomic` and precise **Memory Barriers** (`std::memory_order_acquire/release`), enabling ultra-fast cross-thread communication without `std::mutex`.

## 📊 Performance Benchmarks

Performance is measured using inline assembly `__rdtscp` to fetch highly accurate CPU hardware timestamps, avoiding standard library timing overheads.

* **Test Environment:** WSL (Ubuntu 22.04) / Single Core Affinity
* **Workload:** 100,000 randomized orders (dense crossing & matching)
* **End-to-End Latency (Cross-thread pass + Matching):** `~42.4 CPU Cycles` (approx. **~14 nanoseconds** per order).

## 🛠️ Build & Run

Ensure you have CMake (>= 3.10) and a C++17 compatible compiler (GCC/Clang) installed.

```bash
mkdir build && cd build
cmake ..
make
./ome_test