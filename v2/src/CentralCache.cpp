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

    //核心函数 从中心缓存获取一段内存块
    //参数index : 内存块大小对应的桶索引 （比如8字节、16字节.. 对应不同index
    //返回值 ： 分配好的单个内存块地址

    void* CentralCache::fetchRange(size_t index)
    {
        //检查索引是否越界 超过最大 返回空 （让系统malloc
        //FREE_LIST_SIZE 内存池管理最大桶数量
        if(index >= FREE_LIST_SIZE)
        {
            return nullptr;
        }

        //自旋锁 （高并发关键
        //test_and_set: 原子操作 尝试将锁置为已加锁
        //返回true 已被占用 循环等待
        //std::memory_order_acquire: 获取内存序，保证加锁后代码不会重排到锁前面
        while(locks_[index].test_and_set(std::memory_order_acquire))
        {
            //避免忙等待占CPU
            //线程让步
            std::this_thread::yield();//主动放弃cpu时间片
        }

        // 存储最终返回给用户的内存块地址
        void*result =nullptr;

        // try-catch：异常安全 → 即使抛异常，也能保证锁被释放
        try
        {
            //原子锁：加载当前痛的空闲内存块链表头
            result = centralFreeList_[index].load(std::memory_order_relaxed);

            //如果当前桶没有空闲块 向PageCache申请新Span
            if(!result)
            {
                //计算当前桶管理的内存块大小：（索引+1）*对齐大小
                //index=0 8B ；1 16B..
                size_t size = (index+1)*ALIGNMENT;

                //调用函数 从PageCache申请一大段连续内存（Span
                result = fetchFromPageCache(size);

                //申请失败 解锁+返回空
                if(!result)
                {
                    //clear:解锁; memory_order_release：释放内存序
                    locks_[index].clear(std::memory_order_release);
                    return nullptr;
                }


                //把连续Span切分成小块链表
                //char* 坐字节级指针偏移
                char* start = static_cast<char*>(result);
                //计算实际分配的页数
                //如果申请大小<=8页 默认8页
                size_t numPages = (size <= SPAN_PAGES*PageCache::PAGE_SIZE)?SPAN_PAGES:(size+PageCache::PAGE_SIZE-1)/PageCache::PAGE_SIZE;
                // 计算这个Span能切分成多少个小内存块
                // 总字节数 / 单个块大小
                size_t blockNum = (numPages * PageCache::PAGE_SIZE) / size;

                // 至少要切成2块以上，才能构建链表
                if (blockNum > 1) 
                {
                    // 循环构建单向链表
                    for (size_t i = 1; i < blockNum; ++i) 
                    {
                        // 当前块地址
                        void* current = start + (i - 1) * size;
                        // 下一个块地址
                        void* next = start + i * size;

                        // 强制类型转换 + 解引用
                        // 把 current 地址处的内存，当成“存指针的变量”
                        // 让当前块指向 下一个块（构建链表）
                        *reinterpret_cast<void**>(current) = next;
                    }
                    *reinterpret_cast<void**>(start + (blockNum - 1)*size)=nullptr;

                    //取第一块给用户，剩下的挂到中央链表
                    void* next = *reinterpret_cast<void**>(result);
                    *reinterpret_cast<void**>(result) = nullptr;
                    
                    //更新中心缓存
                    centralFreeList_[index].store(next,std::memory_order_release);

                    //记录Span信息（用于回收） 
                    // 使用无锁方式记录span信息
                    // 做记录是为了将中心缓存多余内存块归还给页缓存做准备。考虑点：
                    // 1.CentralCache 管理的是小块内存，这些内存可能不连续
                    // 2.PageCache 的 deallocateSpan 要求归还连续的内存
                    size_t trackerIndex = spanCount_++;
                    if(trackerIndex < spanTrackers_.size())
                    {
                        spanTrackers_[trackerIndex].spanAddr.store(start,std::memory_order_release);
                        spanTrackers_[trackerIndex].numPages.store(numPages,std::memory_order_release);
                        spanTrackers_[trackerIndex].blockCount.store(blockNum,std::memory_order_release);   //共分配了blockNum个内存块
                        spanTrackers_[trackerIndex].freeCount.store(blockNum-1,std::memory_order_release);  //第一个块resutl已被分配出去 初始空闲块数为blockNum-1
                    }

                }
            }
            else
            {
                // 保存result的下一个节点
                // 有空闲块直接取第一个
                void* next = *reinterpret_cast<void**>(result);

                // 将result与链表断开
                *reinterpret_cast<void**>(result) = nullptr;

                //更新中心缓存
                centralFreeList_[index].store(next,std::memory_order_release);

                //找到这个块属于哪个Span
                SpanTracker* tracker = getSpanTracker(result);
                if(tracker)
                {
                    //空闲块数量-1
                    tracker->freeCount.fetch_sub(1,std::memory_order_release);
                }
            }

        }
        catch(...)
        {
            //发生异常也要解锁
            locks_[index].clear(std::memory_order_release);
            throw;
        }
        
        locks_[index].clear(std::memory_order_release);
        return result;
        
    }

    // 检查是否需要执行延迟归还

    bool CentralCache::shouldPerformDelayedReturn(size_t index,size_t currentCount,std::chrono::steady_clock::time_point currentTime)
    {
        //归还次数够多 归还
        if(currentCount >= MAX_DELAY_COUNT)
        {
            return true;
        }

        //时间到了 归还
        auto lastTime = lastReturnTimes_[index];
        return (currentTime - lastTime) >= DELAY_INTERVAL;
    }

    //真正把空闲span还给PageCache
    void CentralCache::performDelayedReturn(size_t index)
    {
        //重置计数与时间
        delayCounts_[index].store(0,std::memory_order_relaxed);
        lastReturnTimes_[index]=std::chrono::steady_clock::now();

        //统计每个span有多少空闲块
        std::unordered_map<SpanTracker*,size_t>spanFreeCounts;
        void* currentBlock = centralFreeList_[index].load(std::memory_order_relaxed);

        while(currentBlock)
        {
            // 找到这个块属于哪个 Span
            SpanTracker* tracker = getSpanTracker(currentBlock);
            if(tracker)
            {
                // 给对应的Span计数+1
                spanFreeCounts[tracker]++;
            }
            currentBlock = *reinterpret_cast<void**>(currentBlock);
        }
        for(const auto& [tracker,count]:spanFreeCounts)
        {
            // 更新每个span的空闲计数并检查是否可以归还
            updateSpanFreeCount(tracker, count,index);
        }
    }


    void CentralCache::updateSpanFreeCount(SpanTracker* tracker,size_t newFreeBlocks,size_t index)
    {
        //空闲块+新归还
        size_t oldFreeCount =tracker->freeCount.load(std::memory_order_relaxed);
        size_t newFreeCount =oldFreeCount + newFreeBlocks;
        //读用relaxed 写用release
        tracker->freeCount.store(newFreeCount,std::memory_order_release);
        
        //空闲==总块 ->整个Span空闲
        if(newFreeCount == tracker->blockCount.load(std::memory_order_relaxed))
        {
            void* spanAddr = tracker->spanAddr.load(std::memory_order_relaxed);
            size_t numPages = tracker->numPages.load(std::memory_order_relaxed);

            //从桶链表中删除整个Span的所有块
            void* head = centralFreeList_[index].load(std::memory_order_relaxed);
            void* newHead = head;
            void* prev = nullptr;
            void* current = head;

            while(current)
            {
                void* next = *reinterpret_cast<void**>(current);
                //判断当前块是否属于要归还的Span
                if(current >= spanAddr && current < static_cast<char*>(spanAddr) + numPages * PageCache::PAGE_SIZE)
                {
                    if(prev)
                    {
                        *reinterpret_cast<void**>(prev) = next;
                    }
                    else
                    {
                        newHead = next;
                    }
                }
                else 
                {
                    prev=current;
                }
                current = next;
            } 
            //更新桶链表
            centralFreeList_[index].store(newHead,std::memory_order_release);
            //把整个Span归还给PageCache
            PageCache::getInstance().deallocateSpan(spanAddr,numPages);
        }
    }



    

}