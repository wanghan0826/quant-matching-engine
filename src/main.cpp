#include <iostream>
#include <memory>
#include <vector>
#include <random>
#include <iomanip>
#include <thread>
#include <x86intrin.h>
#include "OrderBook.h"
#include "SPSCQueue.h"

inline uint64_t get_cpu_cycles() {
    unsigned int aux;
    return __rdtscp(&aux);
}

int main() {
    std::cout << "========== 量化多核无锁撮合架构压测 ==========\n";
    
    auto book = std::make_unique<OrderBook>();
    
    // 创建一个容量为 131072 (2的17次方) 的无锁环形队列
    SPSCQueue<OrderCommand, 131072> lock_free_queue;
    
    // 准备 10 万笔压测数据
    const int NUM_ORDERS = 100000;
    std::vector<OrderCommand> mock_network_data(NUM_ORDERS);

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> side_dist(1, 2);
    std::uniform_int_distribution<uint32_t> price_dist(10000, 10100); 
    std::uniform_int_distribution<uint32_t> qty_dist(10, 100);

    for (int i = 0; i < NUM_ORDERS; ++i) {
        mock_network_data[i] = {static_cast<uint64_t>(i + 1), 
                                static_cast<uint8_t>(side_dist(rng)), 
                                price_dist(rng), 
                                qty_dist(rng)};
    }

    std::cout << ">>> 准备就绪，双核并发启动 (Producer -> Lock-free Queue -> Consumer)...\n\n";

    std::atomic<bool> producer_done{false};

    // ---------------------------------------------------------
    // 线程 2：消费者 (撮合引擎)
    // 死死盯住无锁队列，一旦有订单进来，立刻拔枪撮合
    // ---------------------------------------------------------
    std::thread consumer_thread([&]() {
        OrderCommand cmd;
        // 只要生产者没发完，或者队列里还有货，就一直转
        while (!producer_done.load(std::memory_order_acquire) || lock_free_queue.pop(cmd)) {
            if (lock_free_queue.pop(cmd)) {
                book->add_order(cmd.order_id, static_cast<OrderSide>(cmd.side), cmd.price, cmd.quantity);
            }
        }
    });

    // ---------------------------------------------------------
    // 线程 1：生产者 (主线程模拟网络网关)
    // ---------------------------------------------------------
    uint64_t start_cycles = get_cpu_cycles();
    
    for (int i = 0; i < NUM_ORDERS; ++i) {
        // 极速自旋：如果队列满了就死等，有空位立刻塞进去
        while (!lock_free_queue.push(mock_network_data[i])) {} 
    }
    
    // 发送完毕信号
    producer_done.store(true, std::memory_order_release);
    
    // 等待撮合线程将队列彻底清空并退出
    consumer_thread.join(); 
    
    uint64_t end_cycles = get_cpu_cycles();
    uint64_t total_latency = end_cycles - start_cycles;

    // ---------------------------------------------------------
    // 终极结算
    // ---------------------------------------------------------
    std::cout << "========== 终极并发压测报告 (End-to-End Report) ==========\n";
    std::cout << "总处理订单数: " << NUM_ORDERS << " 笔\n";
    std::cout << "全链路(跨核传递+撮合)平均延迟: " << std::fixed << std::setprecision(2) 
              << static_cast<double>(total_latency) / NUM_ORDERS << " CPU 周期/笔\n";
    std::cout << "==========================================================\n";

    return 0;
}