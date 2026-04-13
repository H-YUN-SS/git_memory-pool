#pragma once
/*****************************************************************************
 * @file CentralCache.cpp
 * @brief 内存池中心缓存模块实现
 * 
 * 核心功能：
 *  1. 分层内存池架构中的 CentralCache 实现
 *  2. 管理固定大小的内存块空闲链表（分桶结构）
 *  3. 从 PageCache 分配内存页并切分为小块内存
 *  4. 支持多线程并发内存分配与回收
 *  5. 实现延迟回收与空闲 Span 自动释放机制
 * 
 *****************************************************************************/
#include "Common.h"
#include <mutex>
#include <unordered_map>
#include <array>
#include <atomic>
// 时间库 → 用于内存延迟归还
#include <chrono>

namespace Kama_memoryPool
{
    //  Span 追踪器（无锁结构体）    记录一段连续内存页(Span)的使用状态
    struct SpanTracker
    {
        // Span 内存块的起始地址（原子变量 = 多线程无锁访问）
        std::atomic<void*> spanAddr{nullptr};
        // 这个 Span 占用了多少个“系统页”
        // 比如 1页=4KB，numPages=8 就是 32KB
        std::atomic<size_t> numPages{0};
        // 这个 Span 总共被切成了多少个“小内存块”
        std::atomic<size_t>blockCount{0};
        // 当前 Span 中 剩余空闲的小块数量  作用：如果 freeCount == blockCount → 说明整个Span完全空闲 → 可以还给PageCache
        std::atomic<size_t>freeCount{0};

    };
    
    class CentralCache
    {
        public:
        // 单例模式：全局只能有一个CentralCache实例
        static CentralCache& getInstance()
        {
            static CentralCache instance;
            return instance;
        }
    

        // 对外接口：从中心缓存获取一批内存块（给ThreadCache用）
        // index = 内存大小对应的自由链表下标
        void * fetchRange(size_t index);
        
        // 对外接口：ThreadCache归还一批内存块给中心缓存
        // start = 归还链的首地址
        // size = 每个内存块大小
        // index = 自由链表下标
        void returnRange(void* start, size_t size, size_t index);

        private:

        CentralCache();

        // 私有工具：当前中心缓存没内存了 → 向PageCache申请一个大Span
        // size = 要分配的内存块大小
        void* fetchFromPageCache(size_t size);

        // 私有工具：根据一个小内存块地址 → 反向找到它属于哪个Span
        // 返回这个Span的追踪器信息
        SpanTracker* getSpanTracker(void* blockAddr);

        // 私有工具：更新Span的空闲块计数，并判断是否满足“全部空闲”
        // 如果满足 → 把Span还给PageCache
        void updateSpanFreeCount(SpanTracker* tracker, size_t newFreeBlocks, size_t index);

        private://成员变量


        // 中心缓存的自由链表数组
        // 每个下标对应一种固定大小的内存块（8B、16B ... 256KB）
        // 用 atomic<void*> 实现无锁链表头
        std::array<std::atomic<void*>, FREE_LIST_SIZE> centralFreeList_;

        // 每个自由链表配一个**自旋锁**（原子flag）
        // 作用：批量申请/归还时，防止多线程同时修改同一个链表
        std::array<std::atomic_flag, FREE_LIST_SIZE> locks_;
        
        // 用数组存所有Span的追踪信息（性能远高于unordered_map）
        // 最多同时管理1024个Span，足够使用
        std::array<SpanTracker, 1024> spanTrackers_;

        // 当前已经用了多少个SpanTracker（原子计数）
        std::atomic<size_t> spanCount_{0};

        // ===================== 延迟归还策略 =====================
        // 目的：避免Span刚还回去又被申请，频繁切换导致性能损耗

        // 每个链表最大延迟归还计数（达到阈值才触发归还）
        static const size_t MAX_DELAY_COUNT = 48;

        // 每个大小类的延迟计数器
        std::array<std::atomic<size_t>, FREE_LIST_SIZE> delayCounts_;

        // 每个大小类上一次执行归还的时间点
        std::array<std::chrono::steady_clock::time_point, FREE_LIST_SIZE> lastReturnTimes_;

        // 延迟归还的时间间隔（比如50ms）
        static const std::chrono::milliseconds DELAY_INTERVAL;

        // 判断：是否满足延迟归还条件（计数达标 + 时间达标）
        bool shouldPerformDelayedReturn(size_t index, size_t currentCount, std::chrono::steady_clock::time_point currentTime);

        // 真正执行延迟归还：把完全空闲的Span还给PageCache
        void performDelayedReturn(size_t index);



    };

}