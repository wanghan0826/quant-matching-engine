#pragma once
#include <cstdint>
#include <array>
#include <vector>
#include <iostream>
#include <algorithm>

enum class OrderSide : uint8_t { BUY = 1, SELL = 2 };

// 1. 极速订单结构体 (64字节完美对齐 Cache Line)
struct alignas(64) Order {
    uint64_t order_id;
    uint32_t price;
    uint32_t quantity;
    OrderSide side;
    Order* next; 
    Order* prev; 
};

// 2. 带有哨兵的循环双向链表 (消灭所有 if 的关键)
struct PriceLevel {
    Order sentinel;            
    uint64_t total_volume = 0; 
    PriceLevel() {
        sentinel.next = &sentinel;
        sentinel.prev = &sentinel;
    }
};

// 3. 极速订单簿类
class OrderBook {
private:
    static constexpr uint32_t MAX_PRICE_TICKS = 100000;

    std::array<PriceLevel, MAX_PRICE_TICKS> bids_; // 买盘
    std::array<PriceLevel, MAX_PRICE_TICKS> asks_; // 卖盘

    uint32_t best_bid_price_ = 0;
    uint32_t best_ask_price_ = MAX_PRICE_TICKS;

    std::vector<Order> order_pool_;
    size_t next_free_order_idx_ = 0;

    // 核心底层操作：极速挂单 (追加到链表尾部)
    void insert_order_at_tail(PriceLevel& level, Order* new_order) {
        Order* old_tail = level.sentinel.prev;
        old_tail->next = new_order;
        new_order->prev = old_tail;
        new_order->next = &level.sentinel;
        level.sentinel.prev = new_order;
        level.total_volume += new_order->quantity;
    }

public:
    OrderBook() {
        order_pool_.resize(100000); // 预分配 10 万个订单对象
    }

    Order* allocate_order() {
        if (next_free_order_idx_ >= order_pool_.size()) return nullptr;
        return &order_pool_[next_free_order_idx_++];
    }

    // 终极武器：进单与自动撮合逻辑
    void add_order(uint64_t id, OrderSide side, uint32_t price, uint32_t qty) {
        Order* new_order = allocate_order();
        if(!new_order) return;
        new_order->order_id = id;
        new_order->price = price;
        new_order->quantity = qty;
        new_order->side = side;

        if (side == OrderSide::BUY) {
            // --- 买单吃卖单 ---
            while (best_ask_price_ < MAX_PRICE_TICKS && new_order->price >= best_ask_price_ && new_order->quantity > 0) {
                PriceLevel& ask_level = asks_[best_ask_price_];
                Order* current_ask = ask_level.sentinel.next;

                // 遍历当前档位的所有排队卖单
                while (current_ask != &ask_level.sentinel && new_order->quantity > 0) {
                    uint32_t match_qty = std::min(new_order->quantity, current_ask->quantity);
                    new_order->quantity -= match_qty;
                    current_ask->quantity -= match_qty;
                    ask_level.total_volume -= match_qty;

                    //std::cout << "[⚡成交] 买单 " << new_order->order_id << " 吃了卖单 " << current_ask->order_id 
                              //<< " | 价格: " << best_ask_price_ << " | 数量: " << match_qty << "\n";

                    if (current_ask->quantity == 0) {
                        // 暴力美学：0 分支摘除被吃光的单子
                        Order* to_remove = current_ask;
                        current_ask = current_ask->next;
                        to_remove->prev->next = to_remove->next;
                        to_remove->next->prev = to_remove->prev;
                    } else {
                        break; // 对方没吃完，说明我们的买单耗尽了
                    }
                }
                if (ask_level.sentinel.next == &ask_level.sentinel) best_ask_price_++; // 当前档位吃空，寻找下一个价格
            }

            // --- 剩余买单挂入买盘 ---
            if (new_order->quantity > 0) {
                insert_order_at_tail(bids_[new_order->price], new_order);
                if (new_order->price > best_bid_price_) best_bid_price_ = new_order->price;
                //std::cout << "[挂单] 买单 " << new_order->order_id << " 挂入买盘 | 价格: " << new_order->price << " | 剩余数量: " << new_order->quantity << "\n";
            }

        } else {
            // --- 卖单吃买单 (完全对称的逻辑) ---
            while (best_bid_price_ > 0 && new_order->price <= best_bid_price_ && new_order->quantity > 0) {
                PriceLevel& bid_level = bids_[best_bid_price_];
                Order* current_bid = bid_level.sentinel.next;

                while (current_bid != &bid_level.sentinel && new_order->quantity > 0) {
                    uint32_t match_qty = std::min(new_order->quantity, current_bid->quantity);
                    new_order->quantity -= match_qty;
                    current_bid->quantity -= match_qty;
                    bid_level.total_volume -= match_qty;

                    //std::cout << "[⚡成交] 卖单 " << new_order->order_id << " 砸了买单 " << current_bid->order_id 
                              //<< " | 价格: " << best_bid_price_ << " | 数量: " << match_qty << "\n";

                    if (current_bid->quantity == 0) {
                        Order* to_remove = current_bid;
                        current_bid = current_bid->next;
                        to_remove->prev->next = to_remove->next;
                        to_remove->next->prev = to_remove->prev;
                    } else {
                        break;
                    }
                }
                if (bid_level.sentinel.next == &bid_level.sentinel) best_bid_price_--;
            }

            // --- 剩余卖单挂入卖盘 ---
            if (new_order->quantity > 0) {
                insert_order_at_tail(asks_[new_order->price], new_order);
                if (new_order->price < best_ask_price_) best_ask_price_ = new_order->price;
                //std::cout << "[挂单] 卖单 " << new_order->order_id << " 挂入卖盘 | 价格: " << new_order->price << " | 剩余数量: " << new_order->quantity << "\n";
            }
        }
    }
};