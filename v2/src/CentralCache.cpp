#include "../include/CentralCache.h"
#include "../include/PageCache.h"

#include <cassert>
#include <thread>
#include <chrono>

namespace Kama_memoryPool
{
    //静态常量 CentralCache 延迟归还的时间间隔 1000ms
    //constexpr：编译期常量，效率高；std::chrono::milliseconds 毫秒类型
    const std::chrono::milliseconds CentralCache::DELAY_INTERVAL{1000};

    // 静态全局变量：每次向PageCache获取Span的默认大小（8页）
    // static：仅当前文件可见；size_t：无符号整数（表示大小/索引）
    static const size_t SPAN_PAGES=8;

    //CentralCache 构造函数
    //作用：初始化所有成员变量（空闲链表、锁、延迟计数器、Span追踪器）
    CentralCache::CentralCache()
    {
        // 遍历 centralFreeList_ 数组（每个元素是一个空闲内存块链表头）
        // auto& ptr：（避免拷贝）
        for(auto& ptr : centralFreeList_)
        {
            //store 原子写操作 无视线程安全
            // std::memory_order_relaxed：最轻内存序，只保证原子性，不保证顺序
            ptr.store(nullptr,std::memory_order_relaxed);
        }
        //遍历自旋锁数组，初始化所有锁为“未加锁”状态
        for(auto&lock:locks_)
        {
            //clear():清除锁标记=解锁
            lock.clear();
        }
        
        //初始化“延迟归还”计数器数组
        for(auto& count : delayCounts_)
        {
            //原子写： 所有桶初始归还次数=0
            count.store(0,std::memory_order_relaxed);
        }

        //初始化每个桶“最后一次归还时间”
        for(auto& time:lastReturnTimes_)
        {
            //steady_clock: 单调时钟（不会回退），记录当前系统时间
            time = std::chrono::steady_clock::now();
        }

        //原子写：已使用的Span追踪器数量初始化为0
        spanCount_.store(0,std::memory_order_relaxed);

    }




    

}