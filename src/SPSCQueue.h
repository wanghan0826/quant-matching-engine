#pragma once
#include <atomic>
#include <cstddef>
#include <array>

// 提取一个极简的订单指令结构，用于跨线程传递
struct OrderCommand {
    uint64_t order_id;
    uint8_t side; // 1: BUY, 2: SELL
    uint32_t price;
    uint32_t quantity;
};

// 容量必须是 2 的幂次方，方便用位运算代替取模操作 (极速优化)
template <typename T, size_t Capacity>
class SPSCQueue {
private:
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity 必须是 2 的幂次方");
    static constexpr size_t Mask = Capacity - 1;

    // 💀 硬核细节：强行 64 字节对齐，把 head 和 tail 物理隔离在不同的 CPU 缓存行！
    // 彻底消灭多核并发时的“伪共享 (False Sharing)”
    alignas(64) std::atomic<size_t> head_{0}; // 生产者写入位点
    alignas(64) std::atomic<size_t> tail_{0}; // 消费者读取位点
    
    // 环形数组实体
    alignas(64) std::array<T, Capacity> buffer_{};

public:
    // 网关线程 (Producer) 调用：塞入订单
    bool push(const T& item) {
        const size_t current_head = head_.load(std::memory_order_relaxed);
        const size_t next_head = current_head + 1;

        // 检查队列是否已满 (头追上了尾)
        if (next_head - tail_.load(std::memory_order_acquire) > Capacity) {
            return false; 
        }

        buffer_[current_head & Mask] = item; 
        
        // memory_order_release: 释放内存屏障！
        // 保证这行指令之前的数据写入，绝对不能被 CPU 乱序执行排到后面去
        head_.store(next_head, std::memory_order_release);
        return true;
    }

    // 撮合线程 (Consumer) 调用：取出订单
    bool pop(T& item) {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);

        // 检查队列是否为空 (尾追上了头)
        if (current_tail == head_.load(std::memory_order_acquire)) {
            return false;
        }

        item = buffer_[current_tail & Mask]; 
        
        // memory_order_release: 释放读取位点，告诉生产者这个坑位空出来了
        tail_.store(current_tail + 1, std::memory_order_release);
        return true;
    }
};