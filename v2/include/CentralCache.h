#pragma once

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
        static CentralCache& getInstance()
        {
            static CentralCache instance;
            return instance;
        }
    };

    

}