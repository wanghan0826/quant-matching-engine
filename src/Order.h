#pragma once
#include <cstdint>

// 买卖方向枚举 (用 uint8_t 极致压缩内存)
enum class OrderSide : uint8_t {
    BUY = 1,
    SELL = 2
};

// 极速订单结构体 (按 32 字节对齐，完美适配现代 CPU Cache Line，防止伪共享)
struct alignas(32) Order {
    uint64_t order_id;   // 订单唯一标识符
    uint64_t timestamp;  // 纳秒级时间戳 (时间优先核心原则)
    uint32_t price;      // 价格 (量化中绝对不用浮点数！通常放大为整数，如 100.5 存为 10050)
    uint32_t quantity;   // 订单数量
    OrderSide side;      // 买卖方向
    
    // 侵入式链表指针 (为后续无锁数据结构做准备)
    Order* next;
    Order* prev;
};